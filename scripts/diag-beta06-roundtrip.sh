#!/bin/bash
# diag-beta06-roundtrip.sh — beta06「开关保存后回 OFF」真机往返取证
#
# 用法: bash scripts/diag-beta06-roundtrip.sh <设备IP>
# 三条往返链，每条 = POST 写入 → 立即读回（两个独立视角）→ 恢复原值:
#   A. /config  {can,force,bootCan}      → /status 的 ci/bootCan        (FSD 注入大开关)
#   B. /defense_config {enabled}         → /defense_config + /status    (FSD 防御总开关)
#   C. /config  {apg}                    → /status 的 apGateEnabled     (AP 注入门控)
# 判定:
#   写入后读回=true → 设备 RAM 生效; 再查 /status 同步 → 单一事实源一致
#   读回≠写入     → 设备侧保存链断裂（POST 未生效）= 真机复现
# 全程结束恢复原值，净变化为零。不触碰 ap_re_request（8.3.6）。

set -u
IP="${1:?用法: bash scripts/diag-beta06-roundtrip.sh <设备IP>}"
BASE="http://${IP}"
T=5

jget() { printf '%s' "$1" | python3 -c '
import sys, json
try: d = json.load(sys.stdin)
except Exception: print("BADJSON"); sys.exit(0)
v = d
for k in sys.argv[1].split("."):
    if isinstance(v, dict) and k in v: v = v[k]
    else: print("MISSING"); sys.exit(0)
print("true" if v is True else "false" if v is False else v)
' "$1" 2>/dev/null; }

# jget 取不到时 $2 为默认
jget2() { local r; r="$(jget "$1")"; case "$r" in BADJSON|MISSING) echo "$2" ;; *) echo "$r" ;; esac; }

post() { curl -s -m "$T" -X POST "$@" 2>/dev/null; }

echo "═══ 0. 设备身份 ═══"
S0="$(curl -s -m "$T" "$BASE/status" 2>/dev/null)"
echo "version=$(jget2 "$S0" version '?')  uptime=$(jget2 "$S0" uptime '?')s  can=$(jget2 "$S0" can '?')  ci=$(jget2 "$S0" ci '?')"
[ -z "$S0" ] && { echo "FATAL: /status 不可达"; exit 1; }

# 原值快照
ORIG_CI="$(jget2 "$S0" ci false)";   ORIG_BOOT="$(jget2 "$S0" bootCan false)"
ORIG_DEF="$(jget2 "$S0" defenseEnabled false)"
ORIG_APG="$(jget2 "$S0" apGateEnabled false)"
D0="$(curl -s -m "$T" "$BASE/defense_config" 2>/dev/null)"
echo "原始值: ci=$ORIG_CI bootCan=$ORIG_BOOT defenseEnabled=$ORIG_DEF(/defense_config=$(jget2 "$D0" enabled '?')) apGate=$ORIG_APG"
echo

echo "═══ A. FSD 注入大开关（POST /config can/force/bootCan）═══"
post -d "can=1&force=1&bootCan=1" "$BASE/config" >/dev/null
SA="$(curl -s -m "$T" "$BASE/status" 2>/dev/null)"
echo "写入 1 → /status: ci=$(jget2 "$SA" ci '?') bootCan=$(jget2 "$SA" bootCan '?')"
echo "睡眠 5s 后再读: "; sleep 5
SA2="$(curl -s -m "$T" "$BASE/status" 2>/dev/null)"
echo "         ci=$(jget2 "$SA2" ci '?') bootCan=$(jget2 "$SA2" bootCan '?')"
# 恢复原值（用明确的 1/0）
post -d "can=$([ "$ORIG_CI" = true ] && echo 1 || echo 0)&force=$([ "$ORIG_CI" = true ] && echo 1 || echo 0)&bootCan=$([ "$ORIG_BOOT" = true ] && echo 1 || echo 0)" "$BASE/config" >/dev/null
echo "(已恢复原值)"
echo

echo "═══ B. FSD 防御总开关（POST /defense_config enabled）═══"
post -d "enabled=1" "$BASE/defense_config" >/dev/null
DB="$(curl -s -m "$T" "$BASE/defense_config" 2>/dev/null)"
SB="$(curl -s -m "$T" "$BASE/status" 2>/dev/null)"
echo "写入 1 → /defense_config.enabled=$(jget2 "$DB" enabled '?')  /status.defenseEnabled=$(jget2 "$SB" defenseEnabled '?')"
sleep 5
DB2="$(curl -s -m "$T" "$BASE/defense_config" 2>/dev/null)"
SB2="$(curl -s -m "$T" "$BASE/status" 2>/dev/null)"
echo "5s 后  → /defense_config.enabled=$(jget2 "$DB2" enabled '?')  /status.defenseEnabled=$(jget2 "$SB2" defenseEnabled '?')"
post -d "enabled=$([ "$ORIG_DEF" = true ] && echo 1 || echo 0)" "$BASE/defense_config" >/dev/null
echo "(已恢复原值)"
echo

echo "═══ C. AP 注入门控（POST /config apg）═══"
post -d "apg=$([ "$ORIG_APG" = true ] && echo 0 || echo 1)" "$BASE/config" >/dev/null   # 取反写入
SC="$(curl -s -m "$T" "$BASE/status" 2>/dev/null)"
echo "写入 $([ "$ORIG_APG" = true ] && echo 0 || echo 1) → /status.apGateEnabled=$(jget2 "$SC" apGateEnabled '?')"
sleep 5
SC2="$(curl -s -m "$T" "$BASE/status" 2>/dev/null)"
echo "5s 后  → apGateEnabled=$(jget2 "$SC2" apGateEnabled '?')"
post -d "apg=$([ "$ORIG_APG" = true ] && echo 1 || echo 0)" "$BASE/config" >/dev/null   # 恢复
echo "(已恢复原值)"
echo

echo "═══ D. 结束状态核对（应与原始值一致）═══"
SF="$(curl -s -m "$T" "$BASE/status" 2>/dev/null)"
echo "ci=$(jget2 "$SF" ci '?') bootCan=$(jget2 "$SF" bootCan '?') defenseEnabled=$(jget2 "$SF" defenseEnabled '?') apGate=$(jget2 "$SF" apGateEnabled '?') uptime=$(jget2 "$SF" uptime '?')s"
echo "原始: ci=$ORIG_CI bootCan=$ORIG_BOOT defenseEnabled=$ORIG_DEF apGate=$ORIG_APG"
