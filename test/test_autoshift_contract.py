"""Contract tests for the autoshift engine core (include/dash_autoshift.h).

House style: token-pinning on source + a real native compile-and-run of
test/test_native_autoshift_main.cpp via clang++. The -DDRIVER_P4_3CAN
compile flag is pinned deliberately — the header lives inside the P4
guard and a native compile without the define preprocesses to nothing,
making every assertion pass vacuously (the guard-totality test below
pins the flip side).
"""

import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "include" / "dash_autoshift.h"
NATIVE_MAIN = ROOT / "test" / "test_native_autoshift_main.cpp"

# The 16 desk-verified SBW CRC vectors (stalk state 40 C0, counter 0..F).
PINNED_VECTOR_TOKENS = [
    "{0x00, 0xB2}", "{0x10, 0x7F}", "{0x20, 0x35}", "{0x30, 0xF8}",
    "{0x40, 0xA1}", "{0x50, 0x6C}", "{0x60, 0x26}", "{0x70, 0xEB}",
    "{0x80, 0x94}", "{0x90, 0x59}", "{0xA0, 0x13}", "{0xB0, 0xDE}",
    "{0xC0, 0x87}", "{0xD0, 0x4A}", "{0xE0, 0x00}", "{0xF0, 0xCD}",
]

CLANG = "clang++"


class AutoshiftContractTests(unittest.TestCase):
    def test_header_is_gated_by_p4_guard(self):
        source = HEADER.read_text()
        self.assertIn("#if defined(DRIVER_P4_3CAN)", source)

    def test_profile_carries_unfrozen_sentinel_until_task2(self):
        """While Task 2 (field freeze) has not landed, the profile must
        advertise itself: sentinel marker in the header, 0xFF stalk bytes
        (pinned by the native run). When the freeze lands this test is
        replaced by frozen-value assertions in the same RED->GREEN."""
        source = HEADER.read_text()
        self.assertIn("PROFILE UNFROZEN", source)

    def test_native_main_pins_all_16_crc_vectors(self):
        source = NATIVE_MAIN.read_text()
        for token in PINNED_VECTOR_TOKENS:
            self.assertIn(token, source)

    def test_native_main_compiles_and_passes_with_p4_define(self):
        with tempfile.TemporaryDirectory() as tmp:
            binary = Path(tmp) / "autoshift_native"
            result = subprocess.run(
                [CLANG, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                 "-DDRIVER_P4_3CAN", "-I", str(ROOT / "include"),
                 str(NATIVE_MAIN), "-o", str(binary)],
                capture_output=True, text=True, timeout=120)
            self.assertEqual(result.returncode, 0,
                             "compile failed:\n" + result.stderr)
            run = subprocess.run([str(binary)], capture_output=True,
                                 text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("ALL PASS", run.stdout)

    def test_gate_eval_order_is_pinned(self):
        """The gate chain evaluates master -> stale -> speed -> brake ->
        cooldown -> count -> obstacle -> pending -> target -> submit.
        Pinned by source ordering inside dashAsGateEval's body (scoped
        extraction: the enum declaration lists the same tokens)."""
        import re

        source = HEADER.read_text()
        match = re.search(
            r"DashAsVerdict dashAsGateEval\(uint32_t nowMs.*?\n\}",
            source, re.DOTALL)
        self.assertIsNotNone(match, "dashAsGateEval body not found")
        body = match.group(0)
        tokens = ["AS_BLOCK_MASTER", "AS_BLOCK_STALE_118", "AS_BLOCK_SPEED",
                  "AS_BLOCK_BRAKE", "AS_BLOCK_COOLDOWN", "AS_BLOCK_COUNT",
                  "AS_BLOCK_OBSTACLE", "AS_BLOCK_PENDING", "AS_ALREADY_TARGET",
                  "AS_SUBMIT"]
        positions = [body.find(tok) for tok in tokens]
        for tok, pos in zip(tokens, positions):
            self.assertGreater(pos, -1, f"{tok} missing from eval body")
        self.assertEqual(positions, sorted(positions),
                         f"eval order drifted: {list(zip(tokens, positions))}")

    def test_gate_inputs_struct_fields(self):
        source = HEADER.read_text()
        for field in ("bool master;", "bool torque2Fresh;", "uint8_t speedKph;",
                      "bool brakeApplied;", "uint8_t gearNow;",
                      "uint8_t gearTarget;", "bool obstacleFreshAndClear;",
                      "uint32_t lastShiftMs;", "uint32_t nowCount;"):
            self.assertIn(field, source)

    def test_profile_carries_session_shift_count_limit(self):
        source = HEADER.read_text()
        self.assertIn("shiftCountMax", source)

    def test_native_main_covers_gate_matrix_and_close_loop(self):
        source = NATIVE_MAIN.read_text()
        for token in ("AS_BLOCK_MASTER", "AS_BLOCK_STALE_118", "AS_BLOCK_SPEED",
                      "AS_BLOCK_BRAKE", "AS_BLOCK_OBSTACLE", "AS_BLOCK_COOLDOWN",
                      "AS_BLOCK_COUNT", "AS_BLOCK_PENDING", "AS_ALREADY_TARGET",
                      "AS_CONFIRM_COMPLETE", "AS_CONFIRM_RETRY",
                      "AS_CONFIRM_ABANDONED", "dashAsLastAbandonReason"):
            self.assertIn(token, source)

    DASH_HEADER = ROOT / "include" / "web" / "mcp2515_dashboard.h"
    MAIN_SRC = ROOT / "src" / "main.cpp"

    def test_dashboard_includes_autoshift_header_behind_p4_guard(self):
        source = self.DASH_HEADER.read_text()
        self.assertIn('#include "dash_autoshift.h"', source)

    def test_cana_drain_feeds_autoshift_decoders(self):
        """mcpDashOnFrame (the CanA/CanB RX path) feeds 0x118 torque2 and
        0x045 wheel decoders inside a DRIVER_P4_3CAN guard, on the
        vehicle-bus (never the PARTY mirror)."""
        import re

        source = self.DASH_HEADER.read_text()
        match = re.search(
            r"static void mcpDashOnFrame\(const CanFrame &f\).*?\n\}",
            source, re.DOTALL)
        self.assertIsNotNone(match, "mcpDashOnFrame body not found")
        body = match.group(0)
        self.assertIn("#if defined(DRIVER_P4_3CAN)", body)
        self.assertIn("f.id == 0x118", body)
        self.assertIn("dashAsOnTorque2(", body)
        self.assertIn("f.id == 0x045", body)
        self.assertIn("dashAsOnWheel045(", body)

    def test_canc_drain_feeds_angle_and_obstacle_decoders(self):
        """mcpDashSniffCan3 (the CanC/PT RX path) feeds the 0x00E angle
        decoder and the 0x23E/0x20E obstacle decoder inside a
        DRIVER_P4_3CAN guard."""
        import re

        source = self.DASH_HEADER.read_text()
        match = re.search(
            r"static void mcpDashSniffCan3\(const CanFrame &f\).*?\n\}",
            source, re.DOTALL)
        self.assertIsNotNone(match, "mcpDashSniffCan3 body not found")
        body = match.group(0)
        self.assertIn("#if defined(DRIVER_P4_3CAN)", body)
        self.assertIn("0x00E", body)
        self.assertIn("dashAsOnAngle00E(", body)
        self.assertIn("dashAsOnObstacle(", body)
        self.assertIn("0x23E", body)
        self.assertIn("0x20E", body)

    # ---- Task 6: permit gate, authorized submit, DIAG, NVS ----

    def test_native_main_covers_permit_and_snapshot(self):
        source = NATIVE_MAIN.read_text()
        for token in ("kAsArmDeniedMaster", "kAsArmOpened", "kAsArmEnabled",
                      "kAsArmAlreadyArmed", "kAsArmExpired", "dashAsOnSbw0228",
                      "dashAsSbwSource", "dashAsLastSbwTxMs",
                      "kAsAbandonRetryExhausted", "dashAsRuntime"):
            self.assertIn(token, source)

    def test_permit_constants_are_pinned(self):
        source = HEADER.read_text()
        self.assertIn("kAsArmWindowMs = 5000", source)
        self.assertIn("kAsOwnEchoWindowMs = 30", source)

    def test_tick_submits_only_inside_permit_and_submit_verdict(self):
        """t2canAutoshiftTick is the ONLY submit site: the send call must
        sit after BOTH the master/armed guard and the AS_SUBMIT verdict
        (source-pin, order-pinned, single occurrence)."""
        import re

        source = self.MAIN_SRC.read_text()
        match = re.search(r"static void t2canAutoshiftTick\(.*?\n\}",
                          source, re.DOTALL)
        self.assertIsNotNone(match, "t2canAutoshiftTick body not found")
        body = match.group(0)
        guard = body.find("dashAsPermit.master")
        verdict = body.find("AS_SUBMIT")
        send = body.find("t2canAutoshiftSendCan3(")
        for name, pos in (("master/armed guard", guard),
                          ("AS_SUBMIT check", verdict),
                          ("send call", send)):
            self.assertGreater(pos, -1, f"{name} missing from tick body")
        self.assertLess(guard, send, "send must sit inside the permit guard")
        self.assertLess(verdict, send, "send must follow the AS_SUBMIT verdict")
        self.assertEqual(body.count("t2canAutoshiftSendCan3("), 1,
                         "the tick must have exactly one submit site")

    def test_autoshift_test_command_composes_without_transmit(self):
        import re

        source = self.DASH_HEADER.read_text()
        match = re.search(r"static void mcpDashAutoshiftTestFrame\(.*?\n\}",
                          source, re.DOTALL)
        self.assertIsNotNone(match, "mcpDashAutoshiftTestFrame not found")
        body = match.group(0)
        self.assertIn("dashAsSbwCompose(", body)
        for banned in ("->send(", "t2canAutoshiftSendCan3",
                       "t2canSendSecondary"):
            self.assertNotIn(banned, body,
                             f"autoshift_test must never transmit: {banned}")

    def test_diag_registers_autoshift_commands(self):
        import re

        source = self.DASH_HEADER.read_text()
        match = re.search(
            r"static void dashSerialRunCommand\(char \*cmd\).*?\n\}",
            source, re.DOTALL)
        self.assertIsNotNone(match, "dashSerialRunCommand body not found")
        body = match.group(0)
        self.assertIn("#if defined(DRIVER_P4_3CAN)", body)
        for token in ('"autoshift_status"', '"autoshift_arm"',
                      '"autoshift_disarm"', '"autoshift_master"',
                      '"autoshift_test"'):
            self.assertIn(token, body)

    def test_help_lists_autoshift_commands(self):
        import re

        source = self.DASH_HEADER.read_text()
        match = re.search(r"static void dashSerialPrintHelp\(.*?\n\}",
                          source, re.DOTALL)
        self.assertIsNotNone(match, "help body not found")
        body = match.group(0)
        self.assertIn("autoshift_status", body)
        self.assertIn("autoshift_master", body)

    def test_nvs_persists_autoshift_permit_keys(self):
        import re

        source = self.MAIN_SRC.read_text()
        for fn in ("t2canAutoshiftMasterGet", "t2canAutoshiftArmedGet"):
            match = re.search(r"uint8_t %s\(void\).*?\n\}" % fn,
                              source, re.DOTALL)
            self.assertIsNotNone(match, f"{fn} not found")
            body = match.group(0)
            self.assertIn("= 0", body, "NVS default must be 0 (off)")
            self.assertIn("nvs_get_u8", body)
        self.assertIn('nvs_set_u8(handle, "autoshift_master"', source)
        self.assertIn('nvs_set_u8(handle, "autoshift_armed"', source)

    def test_tick_is_scheduled_in_can_task_loop(self):
        source = self.MAIN_SRC.read_text()
        self.assertIn("t2canAutoshiftTick();", source)

    def test_canc_drain_snapshots_sbw_0228(self):
        import re

        source = self.DASH_HEADER.read_text()
        match = re.search(
            r"static void mcpDashSniffCan3\(const CanFrame &f\).*?\n\}",
            source, re.DOTALL)
        self.assertIsNotNone(match)
        body = match.group(0)
        self.assertIn("0x228", body)
        self.assertIn("dashAsOnSbw0228(", body)

    def test_arm_mode_flip_is_can_task_owned(self):
        """Arming must move TWAI2 out of listen-only, but only from can_rt
        (the task that owns the driver): DIAG/boot set a dirty flag; the
        flip reconstructs the driver TWAI_MODE_NORMAL <-> LISTEN_ONLY and
        re-binds the RX consumer."""
        import re

        source = self.MAIN_SRC.read_text()
        match = re.search(r"static void t2canAutoshiftApplyBusMode\(.*?\n\}",
                          source, re.DOTALL)
        self.assertIsNotNone(match, "t2canAutoshiftApplyBusMode not found")
        body = match.group(0)
        self.assertIn("TWAI_MODE_NORMAL", body)
        self.assertIn("TWAI_MODE_LISTEN_ONLY", body)
        self.assertIn("setRxConsumer", body)
        self.assertIn("t2canAutoshiftModeDirty", source)

    def test_status_json_carries_runtime_fields(self):
        import re

        source = self.DASH_HEADER.read_text()
        match = re.search(r"static String mcpDashAutoshiftStatusJson\(.*?\n\}",
                          source, re.DOTALL)
        self.assertIsNotNone(match, "mcpDashAutoshiftStatusJson not found")
        body = match.group(0)
        for token in ("sourceAgeMs", "lastBlock", "verdict", "pending"):
            self.assertIn(token, body)

    def test_dashboard_conditionals_balance(self):
        """The dashboard header's #if/#endif tree must balance to zero.
        Pinned after the Task-6 DIAG insertion silently consumed
        bus_enable's closing #endif (regex-pinned tokens stayed green
        while the preprocessor would have failed at EOF)."""
        import re

        depth = 0
        for i, line in enumerate(
                self.DASH_HEADER.read_text().splitlines(), 1):
            s = line.strip()
            if re.match(r"#\s*(if|ifdef|ifndef)\b", s):
                depth += 1
            elif re.match(r"#\s*endif\b", s):
                depth -= 1
                self.assertGreaterEqual(depth, 0,
                                        f"#endif underflow at line {i}")
        self.assertEqual(depth, 0, "unclosed #if remains at EOF")

    # ---- Task 7: shared UI section, capability gate, web endpoints ----

    UI_SRC = ROOT / "include" / "web" / "mcp2515_dashboard_ui.src.h"
    UI_GEN = ROOT / "include" / "web" / "mcp2515_dashboard_ui.h"

    def test_ui_src_has_capability_gated_autoshift_section(self):
        """The v1 section lives on the pg-shift page (the placeholder page
        that promises this feature) inside a data-cap="autoshift" wrapper:
        hidden on every board whose firmware lacks the capability."""
        src = self.UI_SRC.read_text()
        self.assertIn('<div data-cap="autoshift">', src)
        for tid in ('id="as-state"', 'id="as-gear"', 'id="as-verdict"',
                    'id="as-src-age"', 'id="as-counts"'):
            self.assertIn(tid, src)
        # bilingual labels via the house i18n dictionary
        self.assertIn('data-i18n="自动换挡 v1"', src)
        self.assertIn('data-i18n="总开关"', src)
        self.assertIn("'自动换挡 v1':'Auto Shift v1'", src)
        self.assertIn("'总开关':'Master Switch'", src)

    def test_ui_master_toggle_posts_to_master_endpoint(self):
        """Master toggle follows the defense-page toggle pattern: checkbox
        onchange handler posts {enabled:0|1} to /autoshift_master."""
        import re

        src = self.UI_SRC.read_text()
        self.assertIn('id="as-master-tgl"', src)
        self.assertIn('onchange="saveAsMaster()"', src)
        match = re.search(r"async function saveAsMaster\(\).*?\n\}",
                          src, re.DOTALL)
        self.assertIsNotNone(match, "saveAsMaster not found")
        body = match.group(0)
        self.assertIn("postForm('/autoshift_master'", body)

    def test_ui_polls_status_and_maps_runtime_fields(self):
        import re

        src = self.UI_SRC.read_text()
        match = re.search(r"async function pollAutoshift\(\).*?\n\}",
                          src, re.DOTALL)
        self.assertIsNotNone(match, "pollAutoshift not found")
        body = match.group(0)
        self.assertIn("fetchJson('/autoshift_status')", body)
        # wired at both pg-shift activation sites (the definition is
        # pinned separately above; it has no trailing semicolon)
        self.assertEqual(src.count("pollAutoshift();"), 2,
                         "pollAutoshift must be called at exactly the two "
                         "pg-shift activation sites")

    def test_ui_capability_default_off_and_mapping(self):
        """Non-P4 boards default the capability false and capEnabled maps
        the name — the section never renders there (house can3 pattern)."""
        src = self.UI_SRC.read_text()
        self.assertIn("autoshiftAvailable:false", src)
        self.assertIn(
            "if(name==='autoshift')return !!CAP.autoshiftAvailable;", src)

    def test_dashboard_emits_autoshift_capability(self):
        source = self.DASH_HEADER.read_text()
        self.assertIn(
            "#if defined(DRIVER_P4_3CAN)\n"
            "static constexpr bool kDashAutoshiftAvailable = true;\n"
            "#else\n"
            "static constexpr bool kDashAutoshiftAvailable = false;\n"
            "#endif",
            source,
        )
        # emission is unconditional so older UIs just see false
        self.assertIn('j += "autoshiftAvailable";', source)
        self.assertIn('j += kDashAutoshiftAvailable ? "true" : "false";',
                      source)

    def test_dashboard_registers_autoshift_endpoints_p4_only(self):
        source = self.DASH_HEADER.read_text()
        self.assertIn(
            "#if defined(DRIVER_P4_3CAN)\n"
            '    server.on("/autoshift_status", HTTP_GET,'
            " handleAutoshiftStatus);\n"
            '    server.on("/autoshift_master", HTTP_POST,'
            " handleAutoshiftMaster);\n"
            "#endif",
            source,
        )

    def test_status_endpoint_serves_runtime_json(self):
        import re

        source = self.DASH_HEADER.read_text()
        match = re.search(r"static void handleAutoshiftStatus\(\).*?\n\}",
                          source, re.DOTALL)
        self.assertIsNotNone(match, "handleAutoshiftStatus not found")
        self.assertIn("mcpDashAutoshiftStatusJson()", match.group(0))

    def test_master_endpoint_persists_and_cascades(self):
        """The web toggle only ever moves the master bit: it must persist
        NVS master, clear NVS armed when switching off (else a later
        master-on would skip the two-step arm ritual), mark the bus-mode
        dirty flag — and must expose NO arm or transmit path."""
        import re

        source = self.DASH_HEADER.read_text()
        match = re.search(r"static void handleAutoshiftMaster\(\).*?\n\}",
                          source, re.DOTALL)
        self.assertIsNotNone(match, "handleAutoshiftMaster not found")
        body = match.group(0)
        for token in ("dashAsSetMaster(", "t2canAutoshiftMasterSet(",
                      "t2canAutoshiftArmedSet(0)", "t2canAutoshiftModeDirty()"):
            self.assertIn(token, body)
        for banned in ("autoshift_arm", "t2canAutoshiftSendCan3",
                       "dashAsSbwCompose"):
            self.assertNotIn(banned, body,
                             f"web master endpoint must stay {banned}-free")

    def test_generated_ui_blob_carries_autoshift_tokens(self):
        """src/ui sync: the regenerated ui.h plaintext fallback carries
        the new section tokens (regeneration discipline, minify script)."""
        gen = self.UI_GEN.read_text()
        # the minifier strips attribute quotes: data-cap=autoshift
        for token in ("data-cap=autoshift", "as-master-tgl",
                      "/autoshift_status", "/autoshift_master"):
            self.assertIn(token, gen)

    def test_guard_is_total_without_define(self):
        """Without DRIVER_P4_3CAN the header must expose NOTHING: a TU
        referencing dashAsCrc8J1850 must fail to compile (undeclared).
        Together with the with-define test this pins that the guard is
        real and that the green run above is not vacuous."""
        with tempfile.TemporaryDirectory() as tmp:
            tu = Path(tmp) / "no_define.cpp"
            tu.write_text(
                '#include "dash_autoshift.h"\n'
                "uint8_t (*probe)(const uint8_t *, size_t) ="
                " dashAsCrc8J1850;\n"
                "int main() { return 0; }\n")
            result = subprocess.run(
                [CLANG, "-std=c++17", "-fsyntax-only", "-I",
                 str(ROOT / "include"), str(tu)],
                capture_output=True, text=True, timeout=120)
            self.assertNotEqual(result.returncode, 0,
                                "symbol leaked outside the P4 guard")


if __name__ == "__main__":
    unittest.main()
