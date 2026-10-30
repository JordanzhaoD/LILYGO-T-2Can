# P4-3CAN boards only: two ESP32-P4 build quirks that the platform's
# espidf builder (espressif32@7.0.0, written before IDF 6.0 rev-specific
# bootloader scripts and hosted WiFi) does not cover.

Import("env")

import os
import re

# --- Quirk 1: $(ESP_IDF_VERSION) Kconfig macro -----------------------------
# The P4 WiFi stack lives on the onboard ESP32-C6 companion chip, pulled
# in as managed components (esp_hosted 2.12.13 + esp_wifi_remote 1.6.4,
# the pairing the Atlas-3CAN-P4 builds verified against this board's C6
# firmware). Those components reference the $(ESP_IDF_VERSION) Kconfig
# macro:
#   - esp_hosted Kconfig: `depends on $(ESP_IDF_VERSION) >= "6.1"`
#   - esp_wifi_remote Kconfig: `orsource "./Kconfig.idf_v$ESP_IDF_VERSION.in"`
# IDF >= 6.1 defines that macro for kconfiglib; IDF 6.0.1 (this repo's
# pinned framework) instead resolves $(NAME) from the process
# environment and hard-fails with "macro expanded to blank string".
# Setting it here mirrors what newer IDF does and unblocks the parse.
#
# The value MUST be major.minor form ("6.0"), never "6.0.1": the
# orsource above resolves per-version option fragments, and a
# full-version value would miss Kconfig.idf_v6.0.in silently (orsource
# ignores missing files), leaving every CONFIG_WIFI_RMT_* option
# undefined and the runtime compile broken. With "6.0" the hosted
# version guard correctly evaluates False on this IDF.
os.environ.setdefault("ESP_IDF_VERSION", "6.0")

# --- Quirk 2: rev-specific bootloader linker script -------------------------
# ESP32-P4 rev >= 3 (this board is v3.2/ECO7; <v3 and >=v3 silicon are
# mutually exclusive) links the bootloader against a chip-rev linker
# script (bootloader.rev3.ld), preprocessed from
# bootloader.rev3.ld.in by the IDF CMake subproject. The platform
# builder preprocesses only the rev-less bootloader.ld.in and links
# bootloader.elf itself, so on a clean build the rev script is never
# generated and the link dies with "cannot open linker script file
# bootloader.rev3.ld". Mirror the builder's own
# preprocess_linker_script() SCons node for the rev script and make
# bootloader.elf depend on it.
build_dir = env.subst("$BUILD_DIR")
fw_dir = env.PioPlatform().get_package_dir("framework-espidf")
sdkconfig_file = os.path.join(
    env.subst("$PROJECT_DIR"), "sdkconfig.%s" % env["PIOENV"]
)

rev = 3  # esp32p4 sdkconfig defaults; revised below if the env pins one
if os.path.isfile(sdkconfig_file):
    match = re.search(
        r"^CONFIG_ESP32P4_REV_MIN_FULL=(\d+)",
        open(sdkconfig_file, encoding="utf-8").read(),
        re.M,
    )
    if match:
        rev = int(match.group(1)) // 100

ld_tpl_dir = os.path.join(
    fw_dir, "components", "bootloader", "subproject", "main", "ld", "esp32p4"
)
ld_in = os.path.join(ld_tpl_dir, "bootloader.rev%d.ld.in" % rev)

if os.path.isfile(ld_in):
    ld_out = os.path.join(
        build_dir, "bootloader", "ld", "bootloader.rev%d.ld" % rev
    )
    cmake = os.path.join(
        env.PioPlatform().get_package_dir("tool-cmake"), "bin", "cmake"
    )
    cc = os.path.join(
        env.PioPlatform().get_package_dir("toolchain-riscv32-esp"),
        "bin",
        "riscv32-esp-elf-gcc",
    )
    include_dirs = [
        os.path.join(build_dir, "bootloader", "config"),
        os.path.join(fw_dir, "components", "esp_system", "ld"),
        ld_tpl_dir,
    ]
    cmd = [
        cmake,
        "-DCC=%s" % cc,
        "-DSOURCE=%s" % ld_in,
        "-DTARGET=%s" % ld_out,
        '"-DCFLAGS=%s"'
        % " ".join('-I\\"%s\\"' % p for p in include_dirs),
        "-P",
        os.path.join(fw_dir, "tools", "cmake", "linker_script_preprocessor.cmake"),
    ]
    ld_node = env.Command(
        ld_out,
        ld_in,
        env.VerboseAction(
            " ".join(cmd),
            "Preprocessing rev%d bootloader linker script" % rev,
        ),
    )
    env.Depends(os.path.join("$BUILD_DIR", "bootloader.elf"), ld_node)
