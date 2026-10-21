#!/usr/bin/env python3
"""Positive contract for the Legacy Continuous Absolute v4 implementation."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent.parent
HANDLERS = (ROOT / "include" / "handlers.h").read_text()
DASH = (ROOT / "include" / "web" / "mcp2515_dashboard.h").read_text()
UI = (ROOT / "include" / "web" / "mcp2515_dashboard_ui.src.h").read_text()
ENGINE = (ROOT / "include" / "dash_reactive_nag.h").read_text()
V4_ENGINE = (ROOT / "include" / "dash_legacy_nag_v4.h").read_text()

def test_legacy_owns_continuous_absolute_v4_subfeature():
    legacy = re.search(r"struct LegacyHandler.*?^\};", HANDLERS, re.S | re.M)
    assert legacy
    body = legacy.group(0)
    assert "DashLegacyNagV4 legacyNagV4" in body
    assert "frame.id == 880" in body
    assert "frame.id == 921" in body
    assert "driver.send(echo)" in body


def test_runtime_is_default_off_and_layered():
    assert "Shared<bool> bionicSteering{false}" in HANDLERS
    assert "nagKillerRuntime = canActive;" in DASH
    assert "nagKillerRuntime = canActive && dashDefenseEnabled" not in DASH
    assert "legacyNagConfigured()" in HANDLERS
    assert "legacyNagApContextActive" in HANDLERS
    assert "currentGear != 4" not in HANDLERS
    assert "kNagContextStaleMs" in HANDLERS
    assert "legacyNagLastTxData" in HANDLERS
    assert "CounterCollision" in HANDLERS
    assert "DasSatisfied" in ENGINE
    # 5.0.0-beta04 task 7: dashboard defense-diag mirror removed with the
    # Legacy NAG UI; the engine counters stay observable in HANDLERS only.
    assert "counterCollisionObserved" not in DASH
    assert "expectedNativeFollowerObserved" in HANDLERS
    assert "kNagNativeFollowerMinMs" in HANDLERS


def test_v4_engine_is_bounded_and_same_frame_based():
    assert "kTorqueRawBase{2050}" in V4_ENGINE
    assert "kEncoderMinCentiNm{-270}" in V4_ENGINE
    assert "kEncoderMaxCentiNm{450}" in V4_ENGINE
    assert "onSteeringRequest" in V4_ENGINE
    assert "kSteeringFreshMs{250}" in V4_ENGINE
    assert "kMildMaxCentiNm{180}" in V4_ENGINE
    assert "kStrongMaxCentiNm{210}" in V4_ENGINE
    assert "kPreserveNativeHandsOnLevel{true}" in V4_ENGINE
    assert "echo.data[4] = frame.data[4];" in HANDLERS
    assert "handsOnState == 0 || handsOnState == 8" in V4_ENGINE
    assert "CanFrame echo = frame" in HANDLERS
    assert "legacyNagBus.accept(frame.bus" in HANDLERS
    assert "legacyNagChecksum(echo)" in HANDLERS


def test_api_nvs_and_ui_contract():
    # 5.0.0-beta04 task 7: the dashboard /defense_config bionic branch, the
    # /status legacyNag JSON blob, and the Legacy NAG UI card were removed;
    # the engine (handlers.h / dash_legacy_nag_v4.h) stays dormant but intact.
    assert 'prefs.putBool("def_bio", dashBionicSteering)' not in DASH
    assert 'prefs.getBool("def_bio", false)' not in DASH
    assert '"legacyNag"' not in DASH
    assert 'legacy_nag_strategy' not in DASH
    assert "Legacy 0x370 NAG 主路径" not in UI
    assert "updateLegacyNagStatus" not in UI
    assert 'id="def-epnag-tgl"' not in UI
    assert 'id="legacy-nag-strategy"' not in UI
    assert 'id="legacy-nag-arm-tgl"' not in UI
