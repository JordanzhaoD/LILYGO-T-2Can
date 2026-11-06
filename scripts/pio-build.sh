#!/usr/bin/env bash
# pio build wrapper with a never-download guarantee (2026-11-06 incident).
#
# Root cause back then: espressif32@7.0.0 platform.py force-overrides the
# toolchain-riscv32-esp spec to the registry form
# "platformio/toolchain-riscv32-esp @ 15.2.0+20251204", and the local .piopm
# record said "15.2.0-20251204" (hyphen = semver PRERELEASE tag) which can
# never equal "+20251204" (build tag). Result: pio re-downloaded the ~200MB
# toolchain on EVERY build — at ~3 KB/s a 15-hour silent stall.
#
# This wrapper refuses to build unless every package the ESP builds resolve
# LOCALLY. If a future platform update bumps a forced spec, you get an
# instant, actionable error here instead of a hung download.
#
# Usage: scripts/pio-build.sh run -e p4_3can        (args pass through to pio)
#        scripts/pio-build.sh test -e native_legacy (works for pio test too)
set -euo pipefail

PIO_BIN="${PIO_BIN:-$HOME/.platformio/penv/bin/pio}"
PY_BIN="${PY_BIN:-$HOME/.platformio/penv/bin/python}"

# The specs every ESP-IDF build must resolve without the network:
#  - the platform.py-forced registry spec for the RISC-V toolchain
#  - the file:// pins declared in platformio_local.ini [espidf6]
"$PY_BIN" - <<'PREFLIGHT'
import sys

from platformio.package.manager.tool import ToolPackageManager
from platformio.package.meta import PackageSpec

TOOLS = "/Users/ziwind/.platformio/tools"
REQUIRED_SPECS = [
    # platform.py configure_default_packages() force-mutates this one to the
    # registry form; keep in sync if a platform update bumps the version.
    "platformio/toolchain-riscv32-esp @ 15.2.0+20251204",
    # file:// pins from platformio_local.ini [espidf6]
    "toolchain-xtensa-esp-elf @ file://%s/toolchain-xtensa-esp-elf" % TOOLS,
    "toolchain-riscv32-esp @ file://%s/toolchain-riscv32-esp-15.2.0" % TOOLS,
    "tool-esptoolpy @ file://%s/tool-esptoolpy" % TOOLS,
    "tool-riscv32-esp-elf-gdb @ file://%s/tool-riscv32-esp-elf-gdb" % TOOLS,
    "tool-xtensa-esp-elf-gdb @ file://%s/tool-xtensa-esp-elf-gdb" % TOOLS,
]

manager = ToolPackageManager()
failed = False
for spec_str in REQUIRED_SPECS:
    # NOTE: PackageSpec(raw_string) does NOT split "name @ file://..." the way
    # platformio.ini does (the whole string becomes the uri). Build the spec
    # from kwargs instead.
    name, _, rest = spec_str.partition(" @ ")
    if rest.startswith("file://"):
        spec = PackageSpec(name=name, uri=rest)
    else:
        owner, _, tail = name.partition("/")
        spec = PackageSpec(owner=owner, name=tail, requirements=rest)
    pkg = manager.get_package(spec)
    if pkg is None:
        failed = True
        print(
            "PREFLIGHT-FAIL: %s does not resolve to a local package.\n"
            "  pio would try the network for it (15h stall risk). Fix the\n"
            "  pin in platformio_local.ini or the .piopm record under\n"
            "  ~/.platformio/packages/ (see scripts/pio-build.sh header)."
            % spec_str,
            file=sys.stderr,
        )
    else:
        print("preflight OK: %s -> %s" % (spec_str, pkg.path))
if failed:
    sys.exit(1)
PREFLIGHT

# Second belt: even if preflight passes, abort within seconds if pio starts
# downloading anything (catches specs we did not think to pin).
LOG="$(mktemp /tmp/pio-build.XXXXXX.log)"
IDF_MAINTAINER=1 "$PIO_BIN" "$@" >"$LOG" 2>&1 &
PID=$!
abort() {
    kill "$PID" 2>/dev/null || true
    echo "BUILD-ABORT: pio tried to fetch a package from the network:" >&2
    grep -m1 -E "Downloading|Tool Manager: Installing" "$LOG" >&2 || true
    echo "Fix the local pin (platformio_local.ini / .piopm), then retry." >&2
    exit 1
}
while kill -0 "$PID" 2>/dev/null; do
    if grep -qE "Downloading|Tool Manager: Installing" "$LOG"; then
        abort
    fi
    sleep 2
done
wait "$PID"
STATUS=$?
cat "$LOG"
rm -f "$LOG"
exit $STATUS
