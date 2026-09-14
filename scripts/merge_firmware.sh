#!/usr/bin/env bash
# 合并 LILYGO T-2CAN (ESP32-S3) 全量固件。
#
# 为什么需要这个脚本：
#   beta.33 的 full_merged.bin 把二级 bootloader 合并到 0x1000（经典 ESP32 偏移），
#   但 ESP32-S3 ROM 从 0x0 加载 bootloader —— 板子上电后 ROM 找不到 bootloader，
#   不启动、无热点。本脚本用分区表推导的正确偏移，并自检 0x0 处为 0xE9。
#
# 为什么不用 .pio/build/<env>/flash_args 的偏移：
#   flash_args 把 app 列在 0x10000（PIO 默认 app 偏移），但我们用的是自定义分区表
#   partitions_16mb_ota_4096k_nvs64.csv，app0 在 0x20000。两者冲突：用 flash_args 的
#   0x10000 会把 app 写进 nvs/otadata 区域，板子损坏。所以 `pio run -t upload` 不可用，
#   必须走本脚本的全量合并路由。
#
# 偏移来源（权威）：
#   bootloader  0x0       —— ESP32-S3 ROM 固定
#   partitions  0x8000    —— ESP-IDF 分区表固定
#   otadata     0x19000   —— 分区表 otadata 行
#   app         0x20000   —— 分区表 app0 行
#
# 跨平台：自检与 SHA256 用 python3，不依赖 xxd/shasum/sha256sum（CI=ubuntu / 本地=mac 都能跑）。
set -euo pipefail

ENV="${1:-lilygo_t2can_dual}"
OUT_DIR="${2:-firmware-assets/merged}"
BUILD_DIR=".pio/build/${ENV}"

BL="${BUILD_DIR}/bootloader.bin"
PT="${BUILD_DIR}/partitions.bin"
OTA="${BUILD_DIR}/ota_data_initial.bin"
APP="${BUILD_DIR}/firmware.bin"

size() { wc -c < "$1" | tr -d ' '; }

echo "合并环境：${ENV}"
echo "构建目录：${BUILD_DIR}"
for f in "$BL" "$PT" "$OTA" "$APP"; do
    if [ ! -f "$f" ]; then
        echo "❌ 缺失：$f" >&2
        echo "   先跑：pio run -e ${ENV}" >&2
        exit 1
    fi
done

mkdir -p "$OUT_DIR"
OUT="${OUT_DIR}/full_merged.bin"

echo "偏移：bootloader@0x0  partitions@0x8000  otadata@0x19000  app@0x20000"
python3 -m esptool --chip esp32s3 merge_bin --format raw \
    -o "$OUT" \
    0x0     "$BL" \
    0x8000  "$PT" \
    0x19000 "$OTA" \
    0x20000 "$APP"

# 自检：0x0 处必须是 0xE9（ESP bootloader 魔数）。这是 beta.33 bug 的回归防线。
first_byte=$(python3 -c "import sys; print(open(sys.argv[1],'rb').read(1).hex())" "$OUT")
if [ "$first_byte" != "e9" ]; then
    echo "❌ 自检失败：full_merged.bin 的 0x0 处应为 e9，实际 ${first_byte}" >&2
    echo "   bootloader 没在 0x0，ESP32-S3 ROM 无法加载。" >&2
    exit 1
fi
echo "✅ 自检通过：0x0 = 0xe9（bootloader 在正确偏移）"

# ota_app.bin：仅 app 镜像，用于 Web UI OTA（保留 NVS/配置）。
cp "$APP" "${OUT_DIR}/ota_app.bin"

# SHA256SUMS（python3 跨平台，格式同 shasum/sha256sum："<hash>  <file>"）。
python3 - "$OUT_DIR" <<'PYEOF'
import hashlib, os, sys
out_dir = sys.argv[1]
lines = []
for fn in ("full_merged.bin", "ota_app.bin"):
    path = os.path.join(out_dir, fn)
    h = hashlib.sha256(open(path, "rb").read()).hexdigest()
    lines.append(f"{h}  {fn}")
with open(os.path.join(out_dir, "SHA256SUMS"), "w") as f:
    f.write("\n".join(lines) + "\n")
PYEOF

echo "✅ 合并完成："
echo "   ${OUT}              ($(size "$OUT") B)"
echo "   ${OUT_DIR}/ota_app.bin ($(size "${OUT_DIR}/ota_app.bin") B)"
echo "   ${OUT_DIR}/SHA256SUMS"
