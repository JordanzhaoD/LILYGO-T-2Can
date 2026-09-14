#!/bin/bash
# diag-beta06-watch.sh — beta06「NVS 无法保存 / 未连接 / WiFi 不稳」取证监测脚本
#
# 用法:  bash scripts/diag-beta06-watch.sh <设备IP> [监测秒数,默认300]
#   设备 IP: AP 直连模式一般是 192.168.4.1；STA 模式用路由器里查到的 IP。
#   按 Ctrl-C 随时停，结束时自动打印汇总。
#
# 它做什么（全部只读/无害操作，不动 8.3.6 防甩功能，不改任何驾驶相关开关）:
#   1. 每 2s 拉一次 /status → 记录 uptime / can(在线) / fps(CAN帧率)
#      - uptime 变小  = 设备重启了（重启循环的铁证）
#      - 拉取失败     = Web/WiFi 层不通（与 can 字段无关）
#      - can 变 false = 超过 10 秒收不到任何 CAN 帧
#   2. 每 20s 做一次「保存路径」探针: POST /config ap_auto_restore=<当前值>
#      （原值回写=零行为变化，但走的正是 UI 开关保存的同一条链：
#        handleConfig → 全量 dashSavePrefs → NVS flash 写入。
#       若设备上 /status 不含该字段则退回 POST /update_beta 探针）
#   3. 每 15s 拉一次 /log 增量（设备端日志: Bus OFFLINE / REC / CFG / PWR 等）
#   4. 启动时读一次 /power_mgmt（自动关机/WiFi自动关闭两个开关的当前状态）
#
# 输出: 屏幕实时事件 + diag-<ip>-<时间戳>.log 全量留档。把屏幕输出和
#       log 文件一起发回即可。

set -u
IP="${1:?用法: bash scripts/diag-beta06-watch.sh <设备IP> [秒数]}"
DURATION="${2:-300}"
BASE="http://${IP}"
OUT="diag-${IP}-$(date +%Y%m%d-%H%M%S).log"
POLL_INTERVAL=2
PROBE_INTERVAL=20   # NVS 写探针周期
LOG_INTERVAL=15     # /log 拉取周期
CURL_TIMEOUT=4

# 模拟器没有的端点自动降级为 n/a，不算失败
PROBE_AVAILABLE=1
LOG_AVAILABLE=1

event() { # event <TAG> <msg>
  local line="[$(date '+%H:%M:%S')] $1 $2"
  echo "$line" | tee -a "$OUT"
}

json_field() { # json_field <json> <key>  → 值或 "n/a"
  printf '%s' "$1" | python3 -c '
import sys, json
try:
    d = json.load(sys.stdin)
except Exception:
    print("badjson"); sys.exit(0)
v = d.get(sys.argv[1], "n/a")
if isinstance(v, float): v = round(v, 1)
print(v)
' "$2"
}

fetch() { # fetch <path> [curl额外参数...] → body（失败输出空串）
  curl -s -m "$CURL_TIMEOUT" "$@" "$BASE$1" 2>/dev/null
}

# ── 预检 ─────────────────────────────────────────────
event "=====" "diag-beta06-watch 开始, 目标 $BASE, 时长 ${DURATION}s"

S="$(fetch /status)"
if [ -z "$S" ]; then
  event "FATAL" "设备 $BASE 不可达（检查 IP / 设备是否上电 / 是否连到设备网络）"
  exit 1
fi
event "INFO" "版本=$(json_field "$S" version) uptime=$(json_field "$S" uptime) can=$(json_field "$S" can) ci=$(json_field "$S" ci) fps=$(json_field "$S" fps)"

P="$(fetch /power_mgmt)"
if [ -n "$P" ]; then
  event "INFO" "电源管理: autoShutdown=$(json_field "$P" autoShutdown) wifiAutoOff=$(json_field "$P" wifiAutoOff)  （任一为 true 都会主动断 WiFi/休眠——若是，先关掉再测）"
else
  event "INFO" "/power_mgmt 不可用（跳过）"
fi

# NVS 探针预检：读当前 beta 通道
BETA_CUR="$(fetch /update_beta -X POST | python3 -c 'import sys,json
try: print(json.load(sys.stdin).get("beta","n/a"))
except Exception: print("n/a")' 2>/dev/null)"
APAR_CUR="$(printf '%s' "$S" | python3 -c 'import sys,json
try: print(json.load(sys.stdin).get("apAutoRestore","n/a"))
except Exception: print("n/a")' 2>/dev/null)"
if [ "$APAR_CUR" != "n/a" ] && [ "$APAR_CUR" != "badjson" ]; then
  PROBE_MODE="config"
  event "INFO" "保存路径探针就绪: POST /config ap_auto_restore=${APAR_CUR}（原值回写，零行为变化，走 UI 开关同一条保存链）"
elif [ "$BETA_CUR" != "n/a" ] && [ "$BETA_CUR" != "badjson" ]; then
  PROBE_MODE="beta"
  event "INFO" "NVS 写探针就绪（beta 通道当前=${BETA_CUR}，探针只写相同值，零行为变化）"
else
  PROBE_AVAILABLE=0
  event "INFO" "/config 与 /update_beta 探针均不可用 → 仅监测 /status"
fi

# /log 预检
L0="$(fetch /log)"
case "$L0" in
  *log*|*\[*|*{*) LOG_AVAILABLE=1 ;;
  "") LOG_AVAILABLE=0; event "INFO" "/log 不可用（跳过日志尾随）" ;;
esac
LOGPREV="$(printf '%s' "$L0" | md5 2>/dev/null || printf '%s' "$L0" | md5sum)"

# ── 主循环 ───────────────────────────────────────────
START=$(date +%s)
PREV_UPTIME=""
PREV_CAN=""
POLLS=0; WEBFAILS=0; REBOOTS=0; CAN_OFF_EVENTS=0; PROBE_FAILS=0
NEXT_PROBE=$((START + PROBE_INTERVAL))
NEXT_LOG=$((START + LOG_INTERVAL))
trap 'event "=====" "手动停止"' INT TERM

while :; do
  NOW=$(date +%s)
  [ $((NOW - START)) -ge "$DURATION" ] && break

  S="$(fetch /status)"
  POLLS=$((POLLS + 1))
  if [ -z "$S" ]; then
    WEBFAILS=$((WEBFAILS + 1))
    event "WEBFAIL" "/status 拉取失败（第 ${WEBFAILS} 次）"
    PREV_UPTIME=""   # 断联后 uptime 不可比，重置基线避免误报重启
  else
    STATUS_RAW="$S"
    UP="$(json_field "$S" uptime)"
    CAN="$(json_field "$S" can)"
    FPS="$(json_field "$S" fps)"
    # 重启检测：uptime 倒退超过 5s
    if [ -n "$PREV_UPTIME" ] && [ "$UP" != "n/a" ] && [ "$PREV_UPTIME" != "n/a" ]; then
      if [ "$UP" != "badjson" ] && [ "$PREV_UPTIME" != "badjson" ]; then
        if [ "$UP" -lt $((PREV_UPTIME - 5)) ] 2>/dev/null; then
          REBOOTS=$((REBOOTS + 1))
          event "REBOOT!" "设备重启了: uptime ${PREV_UPTIME}s → ${UP}s（第 ${REBOOTS} 次）——崩溃/看门狗/断电的铁证"
        fi
      fi
    fi
    [ "$UP" != "badjson" ] && PREV_UPTIME="$UP"
    # CAN 在线状态迁移
    if [ -n "$PREV_CAN" ] && [ "$CAN" != "$PREV_CAN" ] && [ "$CAN" != "n/a" ]; then
      if [ "$CAN" = "false" ]; then
        CAN_OFF_EVENTS=$((CAN_OFF_EVENTS + 1))
        event "CAN-DOWN" "顶栏将显示「未连接」: 超过10秒无CAN帧 (fps=${FPS})"
      else
        event "CAN-UP" "CAN 帧恢复 (fps=${FPS})"
      fi
    fi
    [ "$CAN" != "n/a" ] && PREV_CAN="$CAN"
  fi

  # 保存路径 / NVS 写探针
  if [ "$PROBE_AVAILABLE" -eq 1 ] && [ $NOW -ge $NEXT_PROBE ]; then
    NEXT_PROBE=$((NOW + PROBE_INTERVAL))
    R=""
    if [ "${PROBE_MODE:-}" = "config" ]; then
      APAR_NOW="$(printf '%s' "${STATUS_RAW:-}" | python3 -c 'import sys,json
try: print(json.load(sys.stdin).get("apAutoRestore","n/a"))
except Exception: print("n/a")' 2>/dev/null)"
      case "$APAR_NOW" in True|true) VAL=1 ;; *) VAL=0 ;; esac
      R="$(fetch "/config" -X POST -d "ap_auto_restore=${VAL}")"
    else
      R="$(fetch "/update_beta?beta=${BETA_CUR}" -X POST)"
    fi
    if [ -z "$R" ] || ! printf '%s' "$R" | grep -q '"ok":true'; then
      PROBE_FAILS=$((PROBE_FAILS + 1))
      event "SAVEPROBE-FAIL" "保存路径探针失败（第 ${PROBE_FAILS} 次）: ${R:-<无响应>} ——若当时无 WEBFAIL=设备侧问题，若有=传输层问题"
    else
      event "SAVEPROBE-OK" "保存路径探针 ok (uptime=${PREV_UPTIME}s)"
    fi
  fi

  # /log 增量
  if [ "$LOG_AVAILABLE" -eq 1 ] && [ $NOW -ge $NEXT_LOG ]; then
    NEXT_LOG=$((NOW + LOG_INTERVAL))
    L="$(fetch /log)"
    if [ -n "$L" ]; then
      H="$(printf '%s' "$L" | md5 2>/dev/null || printf '%s' "$L" | md5sum)"
      if [ "$H" != "$LOGPREV" ]; then
        printf '%s' "$L" | python3 -c '
import sys
raw = sys.stdin.read()
try:
    import json; d = json.loads(raw)
    lines = d.get("log", d.get("lines", raw))
except Exception:
    lines = raw
if isinstance(lines, list):
    for x in lines: print(x)
else:
    print(lines)
' >> "$OUT" 2>/dev/null
        LOGPREV="$H"
      fi
    fi
  fi

  sleep "$POLL_INTERVAL"
done

# ── 汇总 ─────────────────────────────────────────────
{
  echo "================ 汇总 ================"
  echo "监测时长   : ${DURATION}s（实际 $(($(date +%s) - START))s）"
  echo "/status 轮询: ${POLLS} 次"
  echo "Web 失败    : ${WEBFAILS} 次  （>0 = WiFi/服务层不稳，与 CAN 无关）"
  echo "设备重启    : ${REBOOTS} 次  （>0 = 崩溃/看门狗/断电——最优先处理）"
  echo "CAN 离线事件: ${CAN_OFF_EVENTS} 次（>0 = 超过10秒无CAN帧，顶栏会显未连接）"
  echo "NVS 探针失败: ${PROBE_FAILS} 次 （仅真实设备；HTTP层失败与Web失败联动看）"
  echo "日志留档   : $OUT"
} | tee -a "$OUT"
