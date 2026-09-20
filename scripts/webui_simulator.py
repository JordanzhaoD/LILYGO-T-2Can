#!/usr/bin/env python3
"""Local WebUI simulator for the ESP32 dashboard.

It serves the same dashboard HTML source used by firmware and provides a
stateful subset of the ESP32 HTTP API so the UI can be inspected without
flashing or connecting to a car.
"""
from __future__ import annotations

import argparse
import json
import random
import re
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any
from urllib.parse import parse_qs, urlparse


ROOT = Path(__file__).resolve().parent.parent
HTML_SRC = ROOT / "include" / "web" / "mcp2515_dashboard_ui.src.h"
VERSION_FILE = ROOT / "VERSION"


def load_dashboard_html() -> str:
    text = HTML_SRC.read_text(encoding="utf-8")
    match = re.search(r'R"HTML\((.*)\)HTML";', text, re.DOTALL)
    if not match:
        raise RuntimeError(f"HTML payload not found in {HTML_SRC}")
    return match.group(1)


class SimState:
    def __init__(self) -> None:
        self.started = time.monotonic()
        self.hw = 1
        self.sp = 1
        self.sp_auto = True
        self.drive_profile = 0
        self.can = True
        self.injection = True
        # Boot auto-enable preference mirrored from /config bootCan writes.
        self.boot_can = True
        self.ad = False
        self.rx = 2480
        self.tx = 114
        self.txerr = 0
        self.follow_dist = 3
        self.gateway_ap = 1
        self.vehicle_ota = False
        self.ota_confirm_count = 0
        self.ota_clear_count = 0
        # 4.5.0-beta12 (LittleGong port 1): OTA guard override mirror.
        self.ota_guard_override = False
        self.ota_stale_clears = 0
        self.auto_shutdown = False
        self.wifi_auto_off = False
        self.speed_strategy = "auto"
        self.speed_custom = {"manualPct": 0, "autoMinPct": -5, "autoMaxPct": 15, "cp1": 0, "cp2": 0, "cp3": 0, "cp4": 0}
        self.legacy_speed_bus = "Auto"
        self.legacy_speed_smooth = True
        self.legacy_speed_rate = 5
        self.light_enabled = False
        self.light_count = 3
        self.light_frequency = "medium"
        self.rear_fog_value = 0
        self.burst_enabled = False
        self.burst_count = 3
        self.burst_on_ms = 120
        self.burst_off_ms = 120
        self.defense_enabled = True
        self.bionic_steering = False
        self.dnd_volume = True
        self.dnd_speed = True
        self.ap_eap_compatible = False
        self.abort_guard = False
        self.jitter = False
        self.ap_gate = False
        # ap_delay_ms removed in 4.5.0-beta06 (AP-settle delay deleted).
        self.ap_auto_restore = True
        self.legacy_fsd_policy = "legacy_stable"
        self.legacy_fsd_mux1 = False
        self.legacy_fsd_profile = False
        self.legacy_fsd_vision = False
        self.hw3_offset_slew = True
        self.hw3_slew_rate = 5
        self.hw3_offset_target = 0
        self.hw3_offset_last = 0
        self.hw3_slew_count = 0
        self.led_brightness = 120
        self.enable_print = True
        self.ap_ssid = "EV-CAN-WAVESHARE"
        self.ap_hidden = False
        self.ap_clients = 1
        self.wifi_connected = True
        self.wifi_ssid = "Home-WiFi"
        self.wifi_ip = "192.168.1.88"
        self.wifi_active = 0
        self.wifi_networks = [
            {
                "idx": 0,
                "ssid": "Home-WiFi",
                "hasPass": True,
                "static": False,
            }
        ]
        self.rec_active = False
        self.rec_count = 0
        self.rec_saved = False
        self.rec_started = 0.0
        self.rec_duration_s = 120
        self.rec_filter = "045,118,129,186,238,292,370,389,399,3C2,3EE,3FD,47D,47F,488,5D9"
        self.rec_dropped = 0
        self.rec_elapsed_s = 0.0
        self.gateway_enabled = True
        self.gateway_nat = True
        self.gateway_mode = 1
        self.gateway_strict = False
        self.gateway_blacklist = "\n".join(
            [
                "telemetry.vn.teslamotors.com",
                "telemetry-prd.vn.cloud.tesla.cn",
                "owner-api.vn.teslamotors.com",
                "auth.tesla.cn",
            ]
        )
        self.gateway_whitelist = "pool.ntp.org\n*.github.com"
        self.gateway_blocked = [
            {"domain": "telemetry.vn.teslamotors.com", "count": 8},
            {"domain": "owner-api.vn.teslamotors.com", "count": 2},
            {"domain": "maps.example.com", "count": 3},
        ]
        self.beta = True
        self.auto_update = False
        self.log_seq = 4
        self.log_lines = [
            "[BOOT] Waveshare ESP32-S3 RS485/CAN simulator started",
            "[WIFI] AP EV-CAN-WAVESHARE up at 100.100.1.1",
            "[WIFI] STA connected to Home-WiFi at 192.168.1.88",
            "[GW] STA-AP NAT enabled, DNS filter active",
        ]

    def uptime(self) -> int:
        return int(time.monotonic() - self.started)

    def legacy_fsd_config(self) -> dict[str, Any]:
        return {
            "policy": self.legacy_fsd_policy,
            "mux1": self.legacy_fsd_mux1,
            "profile": self.legacy_fsd_profile,
            "vision": self.legacy_fsd_vision,
        }

    def mode_hw_config(self) -> dict[str, Any]:
        options = ["Auto", "legacy", "HW3", "HW4"]
        value = max(0, min(3, int(self.hw)))
        return {"ok": True, "mode": options[value], "value": value, "options": options}

    def drive_profile_config(self) -> dict[str, Any]:
        options = ["Auto", "Sloth", "Chill", "Normal", "Hurry", "MAX"]
        value = max(0, min(5, int(self.drive_profile)))
        effective = 0 if self.sp_auto else self.sp
        return {
            "ok": True,
            "profile": options[value],
            "value": value,
            "speed_profile_auto": self.sp_auto,
            "speed_profile": self.sp,
            "effective_speed_profile": effective,
            "options": options,
        }

    def gear_assist_status(self) -> dict[str, Any]:
        return {
            "ok": True,
            "enabled": False,
            "armed": False,
            "state": "standby",
            "lastAction": "none",
            "reason": "simulator",
        }

    def status(self) -> dict[str, Any]:
        self.rx += random.randint(3, 18)
        if self.injection:
            self.tx += random.randint(0, 3)
        self.ad = (self.uptime() // 12) % 2 == 1
        fps = round(7.5 + random.random() * 3.5, 1)
        return {
            "hw": self.hw,
            "sp": self.sp,
            "spAuto": self.sp_auto,
            "driveProfile": self.drive_profile,
            "driveProfileName": self.drive_profile_config()["profile"],
            "soff": self.hw3_offset_last,
            "gtwap": self.gateway_ap,
            "vehicleOta": self.vehicle_ota,
            "autoShutdown": self.auto_shutdown,
            "wifiAutoOff": self.wifi_auto_off,
            # 5.0-beta01 power observability: the overview power card's
            # read-only idle line reads these from /status. The simulated
            # bus always has traffic and the polling panel counts as web
            # activity, so both idle timers stay small and the STA flag
            # reads "online" (firmware emits the same keys).
            "canIdleMs": (self.uptime() % 30) * 1000 + 250,
            "webIdleMs": (self.uptime() % 3) * 1000 + 600,
            "wifiStaDisabled": False,
            "fogStrategy": self.rear_fog_value,
            "strobeCont": False,
            "dndVolume": self.dnd_volume,
            "dndSpeed": self.dnd_speed,
            # AP gate pair (renderApInjectionState reads these from /status).
            # apDelayMs removed in 4.5.0-beta06 (AP-settle delay deleted).
            "apGateEnabled": self.ap_gate,
            "apAutoRestore": self.ap_auto_restore,
            "apInjectionState": "armed" if self.ap_gate else "blocked",
            "wheelDndDiag": {
                "enabled": self.dnd_volume,
                "armed": self.dnd_volume and self.defense_enabled and self.hw == 0,
                "autoDisarmed": False,
                "native3c2Seen": True,
                "nativeMux1IdleSeen": True,
                "native3c2AgeMs": 38,
                "nativeRejected": 0,
                "apActive": self.ad,
                "handsState": 2,
                "warningActive": False,
                "warningEpisodes": 0,
                "escalatedObservations": 0,
                "triggerCount": 0,
                "actionsThisEpisode": 0,
                "consecutiveTxFailures": 0,
                "cooldownRemainingMs": 0,
                "baseFrameMode": "native_mux1_idle",
                "sequenceState": "idle",
                "sequenceStep": 0,
                "blockedReason": "warning_clear" if self.dnd_volume else "disabled",
            },
            "AD": self.ad,
            "eprn": self.enable_print,
            "ia": self.injection,
            # Poll write-back fields the real firmware emits (handleStatus):
            # updateDefensePage forces def-master-tgl from defenseEnabled and
            # the boot toggle from bootCan — omitting them here makes every
            # UI smoke test on the simulator look like the "switches revert
            # to OFF" bug (checkbox forced false each poll).
            "defenseEnabled": self.defense_enabled,
            "bionicSteering": self.bionic_steering,
            "bootCan": self.boot_can,
            "hw3OffsetSlew": self.hw3_offset_slew,
            "hw3SlewRate": self.hw3_slew_rate,
            "hw3OffsetTarget": self.hw3_offset_target,
            "hw3OffsetLast": self.hw3_offset_last,
            "hw3SlewCount": self.hw3_slew_count,
            "ledB": self.led_brightness,
            "can": self.can,
            "ci": self.injection,
            "rx": self.rx,
            "tx": self.tx,
            "txerr": self.txerr,
            "fd": self.follow_dist,
            "fps": fps,
            "eflg": 0,
            "up": self.uptime(),
            "probe": {
                "active": False,
                "state": 0,
                "id": 1021,
                "mux": 0,
                "txa": 0,
                "rxa": 0,
                "txdlc": 8,
                "rxdlc": 8,
                "hasrx": True,
                "tx": [0, 0, 0, 60, 32, 0, 4, 0],
                "rx": [0, 0, 0, 60, 32, 0, 0, 0],
            },
            "mux": [
                {"rx": self.rx // 3, "tx": self.tx // 3, "err": 0},
                {"rx": self.rx // 4, "tx": self.tx // 4, "err": 0},
                {"rx": self.rx // 5, "tx": self.tx // 5, "err": 0},
            ],
            "legacySmartSpeed": {
                "configured": True,
                "legacyHandler": True,
                "runtimeActive": True,
                "mode": self.speed_strategy,
                "strategy": self.speed_strategy,
                "limitSource": "0x2F8",
                "limitRaw": 16,
                "limitKph": 80,
                "rawTargetKph": 104,
                "smoothedTargetKph": 101,
                "offsetKph": 21,
                "offsetPct": 30,
                "smoothing": self.legacy_speed_smooth,
                "smoothDown": self.legacy_speed_smooth,
                "smoothRateKphS": self.legacy_speed_rate,
                "busMode": self.legacy_speed_bus,
                "targetBus": "CAN2" if self.legacy_speed_bus == "Auto" else self.legacy_speed_bus,
                "busAmbiguous": False,
                "ambiguityCount": 0,
                "rx2F8": self.rx // 4,
                "rx3EE": self.rx // 5,
                "txOk": self.tx,
                "txFail": self.txerr,
                "offsetOnlyTxOk": self.tx,
                "blockedReason": "none",
                "last2F8AgeMs": 42,
                "lastFusedAgeMs": 81,
            },
        }

    def frames(self) -> dict[str, Any]:
        now = self.uptime() * 1000
        base = [
            (1021, "DI_state", [0, 0, 0, 60, 32, 0, 0, 0]),
            (1016, "AP_status", [1, 0, 0, 0, 0, 0, 0, 0]),
            (921, "ISA_chime", [0, 0, 0, 0, 0, 0, 0, 0]),
            (257, "Gateway_heartbeat", [self.gateway_ap, 0, 0, 0, 0, 0, 0, 0]),
            (0x678, "GTW_gearControl", [0, 0, 0, 0, 4, 0, 0, 0]),
            (0x233, "UI_stalklessControl", [0, 0, 2, 0, 0, 0, 0, 0]),
            (0x118, "DI_systemStatus", [0, 4, 3, 0, 0, 0, 0, 0]),
        ]
        frames = []
        for i, (can_id, name, data) in enumerate(base):
            d = list(data)
            d[0] = (d[0] + self.uptime() + i) & 0xFF
            frames.append({"ts": now - i * 87, "id": can_id, "dlc": 8, "data": d, "name": name})
        return {"frames": frames}


HTML = load_dashboard_html()
VERSION = VERSION_FILE.read_text(encoding="utf-8").strip() if VERSION_FILE.exists() else "3.0.0-beta.5"
STATE = SimState()


def _dns_rules(text):
    return [x.strip().lower().removeprefix("*.").rstrip(".") for x in re.split(r"[\s,;]+", text or "") if x.strip()]


def _domain_in_list(domain, rules):
    return _domain_rule_match_len(domain, rules) > 0


def _domain_rule_match_len(domain, rules):
    domain = (domain or "").strip().lower().removeprefix("*.").rstrip(".")
    best = 0
    for rule in rules:
        if domain == rule or domain.endswith("." + rule):
            best = max(best, len(rule))
    return best


def _dns_decision(domain):
    domain = (domain or "").strip().lower().removeprefix("*.").rstrip(".")
    blacklist = _dns_rules(STATE.gateway_blacklist)
    whitelist = _dns_rules(STATE.gateway_whitelist)
    blacklisted = _domain_in_list(domain, blacklist)
    whitelisted = _domain_in_list(domain, whitelist)
    block_len = _domain_rule_match_len(domain, blacklist)
    allow_len = _domain_rule_match_len(domain, whitelist)
    if not STATE.gateway_enabled:
        allowed, reason = True, "gateway disabled"
    elif not domain:
        allowed, reason = False, "empty domain"
    elif allow_len > 0 and allow_len >= block_len:
        allowed, reason = True, "matched whitelist"
    elif block_len > 0:
        allowed, reason = False, "matched blacklist"
    elif STATE.gateway_mode == 0:
        allowed, reason = True, "not in blacklist"
    else:
        allowed, reason = False, "not in whitelist"
    return {
        "domain": domain,
        "enabled": STATE.gateway_enabled,
        "mode": STATE.gateway_mode,
        "allowed": allowed,
        "blocked": (not allowed) and STATE.gateway_enabled and bool(domain),
        "blacklisted": blacklisted,
        "whitelisted": whitelisted,
        "reason": reason,
    }


def _read_bits_le(data, start, length):
    value = 0
    for i in range(length):
        bit = start + i
        if bit // 8 >= len(data):
            break
        if data[bit // 8] & (1 << (bit % 8)):
            value |= 1 << i
    return value


def _gear_label(value, short=False):
    if value == 1:
        return "P" if short else "PARK"
    if value == 2:
        return "R" if short else "REVERSE"
    if value == 3:
        return "N" if short else "NEUTRAL"
    if value == 4:
        return "D" if short else "DRIVE"
    return "IDLE_SNA" if value == 0 else "UNKNOWN"


def _gear_frame(frame_id, name, signal, data, count, short=False):
    cands = []
    for bit in range(0, max(0, len(data) * 8 - 2)):
        v = _read_bits_le(data, bit, 3)
        if v in (2, 4):
            cands.append({"bit": bit, "value": v, "label": _gear_label(v, short)})
    return {
        "seen": True,
        "id": frame_id,
        "name": name,
        "signal": signal,
        "ts": int(STATE.uptime() * 1000),
        "age": 80,
        "count": count,
        "dlc": len(data),
        "data": data,
        "candidates": cands,
    }


class Handler(BaseHTTPRequestHandler):
    server_version = "EVWebUISim/1.0"

    def log_message(self, fmt: str, *args: Any) -> None:
        print(f"{self.address_string()} - {fmt % args}")

    def send_obj(self, obj: Any, status: int = 200) -> None:
        data = json.dumps(obj, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def send_text(self, text: str, content_type: str = "text/plain", status: int = 200) -> None:
        data = text.encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
        self.send_header("Pragma", "no-cache")
        self.end_headers()
        self.wfile.write(data)

    def read_form(self) -> dict[str, str]:
        length = int(self.headers.get("Content-Length", "0") or "0")
        raw = self.rfile.read(length).decode("utf-8", errors="replace") if length else ""
        if self.headers.get("Content-Type", "").startswith("application/json"):
            try:
                return {"plain": raw, **json.loads(raw)}
            except Exception:
                return {"plain": raw}
        parsed = parse_qs(raw, keep_blank_values=True)
        return {k: v[-1] if v else "" for k, v in parsed.items()}

    @staticmethod
    def form_bool(value: Any) -> bool:
        return str(value).lower() in ("1", "true", "yes", "on")

    def do_GET(self) -> None:
        parsed_url = urlparse(self.path)
        path = parsed_url.path
        qs = parse_qs(parsed_url.query, keep_blank_values=True)
        if path == "/":
            self.send_text(HTML, "text/html; charset=utf-8")
        elif path == "/favicon.ico":
            self.send_text("", "image/x-icon")
        elif path == "/status":
            self.send_obj(STATE.status())
        elif path == "/config":
            self.send_obj({"fsdRuntime": {"legacyOffset": 0, "overrideSpeedLimit": False}})
        elif path == "/plugins/status":
            self.send_obj({"installed": [], "replayCount": 1})
        elif path == "/frames":
            self.send_obj(STATE.frames())
        elif path == "/legacy_fsd_config":
            self.send_obj(STATE.legacy_fsd_config())
        elif path == "/mode_hw":
            self.send_obj(STATE.mode_hw_config())
        elif path == "/drive_profile":
            self.send_obj(STATE.drive_profile_config())
        elif path == "/speed_strategy":
            self.send_obj(
                {
                    "ok": True,
                    "strategy": STATE.speed_strategy,
                    "value": {"fixed": 0, "auto": 1, "custom": 2}.get(STATE.speed_strategy, 1),
                    "speed_profile_auto": STATE.sp_auto,
                    "hw3_custom_speed": STATE.speed_strategy == "custom",
                    "legacy_custom_speed": STATE.speed_strategy == "custom",
                    "options": ["fixed", "auto", "custom"],
                }
            )
        elif path == "/speed_custom":
            self.send_obj({"ok": True, **STATE.speed_custom})
        elif path == "/legacy_speed_config":
            self.send_obj(
                {
                    "ok": True,
                    "bus": STATE.legacy_speed_bus,
                    "smooth_down": STATE.legacy_speed_smooth,
                    "smooth_rate_kph_s": STATE.legacy_speed_rate,
                    "source_priority": ["0x2F8", "0x399"],
                    "gps_fresh_ms": 3000,
                    "fused_fresh_ms": 1000,
                }
            )
        elif path == "/power_mgmt":
            self.send_obj({"autoShutdown": STATE.auto_shutdown, "wifiAutoOff": STATE.wifi_auto_off})
        elif path == "/dns_rules":
            blacklist = _dns_rules(STATE.gateway_blacklist)
            whitelist = _dns_rules(STATE.gateway_whitelist)
            self.send_obj(
                {
                    "blacklist": STATE.gateway_blacklist,
                    "whitelist": STATE.gateway_whitelist,
                    "black_count": len(blacklist),
                    "white_count": len(whitelist),
                    "mode": STATE.gateway_mode,
                    "strict": STATE.gateway_strict,
                    "enabled": STATE.gateway_enabled,
                }
            )
        elif path == "/hotspot_config":
            self.send_obj({"ssid": STATE.ap_ssid, "hidden": STATE.ap_hidden, "clients": STATE.ap_clients})
        elif path == "/bus2_ids":
            self.send_obj(
                {
                    "count": 2,
                    "rx_total": STATE.rx // 2,
                    "can2": {"tx": STATE.tx // 2, "txerr": 0, "eflg": 0},
                    "service_mode": False,
                    "ids": [0x339, 0x3C2],
                }
            )
        elif path == "/lighting_config":
            self.send_obj(
                {
                    "enabled": STATE.light_enabled,
                    "count": STATE.light_count,
                    "frequency": STATE.light_frequency,
                    "frequency_value": {"slow": 0, "medium": 1, "fast": 2}.get(STATE.light_frequency, 1),
                    "rear_fog_strategy": ["off", "strobe", "f1", "continuous"][STATE.rear_fog_value],
                    "rear_fog_value": STATE.rear_fog_value,
                }
            )
        elif path == "/burst":
            self.send_obj(
                {
                    "enabled": STATE.burst_enabled,
                    "count": STATE.burst_count,
                    "on_ms": STATE.burst_on_ms,
                    "off_ms": STATE.burst_off_ms,
                    "phases_left": 0,
                    "running": False,
                    "last_trigger_ms": 0,
                }
            )
        elif path == "/defense_config":
            self.send_obj(
                {
                    "enabled": STATE.defense_enabled,
                    "bionic_steering": STATE.bionic_steering,
                    "dnd_volume": STATE.dnd_volume,
                    "legacy_scroll_dnd": STATE.dnd_volume,
                    "dnd_speed": STATE.dnd_speed,
                    "ap_eap_compatible": STATE.ap_eap_compatible,
                    "abort_guard": STATE.abort_guard,
                    "ota_guard_override": STATE.ota_guard_override,
                    "jitter": STATE.jitter,
                    "legacyInjectionSafety": {
                        "abortGuard": {
                            "enabled": STATE.abort_guard,
                            "latched": False,
                            "lastApState": 0,
                            "lastAbortState": 0,
                            "blocks": 0,
                        },
                        # minimalInject / steerDefense removed with the #108
                        # steer-jerk defense (4.5.0-beta02); the 8.3.6
                        # cancel/re-request coordinator was itself replaced
                        # in 4.5.0-beta15 by the JITTER procedure (verbatim
                        # port of the single-CAN v1.19.2 field-frozen code).
                        # Static standby mirror of appendJitterDiagJson's 22
                        # keys; the procedure stays inert (no CAN traffic in
                        # the simulator), apGateOpen tracks the gate toggle.
                        "jitter": {
                            "requested": STATE.jitter,
                            "effective": STATE.jitter and STATE.ap_gate,
                            "apGateOpen": STATE.ap_gate,
                            "permit": True,
                            "phase": "inert",
                            "reason": "apGateOff" if STATE.jitter else "off",
                            "apState": 0,
                            "cycles": 0,
                            "cancels": 0,
                            "requests": 0,
                            "reengagedFsd": 0,
                            "reengagedEap": 0,
                            "bit46Shots": 0,
                            "bit46Refreshes": 0,
                            "pumpFrames": 0,
                            "txOk": 0,
                            "txFail": 0,
                            "steerAborts": 0,
                            "steerResets": 0,
                            "apErrorResets": 0,
                            "timeoutResets": 0,
                            "failedCycles": 0,
                        },
                        # otaWatch / carrier3ee / txAccounting were nested
                        # inside the coordinator object before beta15; the
                        # firmware always emitted them as jitter's siblings
                        # inside legacyInjectionSafety — hoisted to match.
                        "otaWatch": {
                            "seen318": 0,
                            "last318Ms": 0,
                            "confirm": STATE.ota_confirm_count,
                            "clear": STATE.ota_clear_count,
                            "active": STATE.vehicle_ota,
                            # 4.5.0-beta12 (LittleGong port 1): guard
                            # degradation bookkeeping.
                            "staleClears": STATE.ota_stale_clears,
                            "override": STATE.ota_guard_override,
                            "blockedMs": 0,
                        },
                        # 4.5.0-beta12 (LittleGong port 2): 0x3EE mux0
                        # carrier cadence (static mirror).
                        "carrier3ee": {
                            "lastGapMs": 1000,
                            "maxGapMs": 2000,
                            "longGaps": 0,
                            "total": 42,
                            "lastMs": 1000,
                            "ageMs": 0,
                        },
                        # 4.5.0-beta12 (LittleGong port 3): 0x3EE mux0
                        # injection TX accounting (static mirror).
                        "txAccounting": {
                            "attempts": 0,
                            "ok": 0,
                            "fails": 0,
                            "consecutiveFails": 0,
                            "lastOkAgeMs": 0,
                        },
                    },
                }
            )
        elif path == "/fog_light":
            self.send_obj({"ok": True, "active": False, "reason": "idle", "strategy": STATE.rear_fog_value})
        elif path == "/vehicle_ota_status":
            self.send_obj(
                {
                    "vehicleOta": STATE.vehicle_ota,
                    "otaConfirmCount": STATE.ota_confirm_count,
                    "otaClearCount": STATE.ota_clear_count,
                    # 4.5.0-beta12: synced with otaWatch.
                    "staleClears": STATE.ota_stale_clears,
                    "override": STATE.ota_guard_override,
                    "blockedMs": 0,
                }
            )
        elif path == "/gear_assist_status":
            self.send_obj(STATE.gear_assist_status())
        elif path == "/gear_status":
            self.send_obj(
                {
                    "source": "mcu3",
                    "readOnly": True,
                    "note": "MCU3 JSON provides enum values but no bit offsets; candidates are 3-bit raw D/R windows.",
                    "gtw": _gear_frame(0x678, "GTW_gearControl", "GTW_gearShiftRequest", [0, 0, 0, 0, 4, 0, 0, 0], STATE.rx // 7 + 1),
                    "ui": _gear_frame(0x233, "UI_stalklessControl", "UI_gearRequest", [0, 0, 2, 0, 0, 0, 0, 0], STATE.rx // 9 + 1),
                    "di": _gear_frame(0x118, "DI_systemStatus", "DI_gear / DI_shiftRequestType", [0, 4, 3, 0, 0, 0, 0, 0], STATE.rx // 8 + 1, True),
                }
            )
        elif path == "/log":
            self.send_obj({"seq": STATE.log_seq, "lines": STATE.log_lines[-20:]})
        elif path == "/rec_status":
            elapsed = max(0.0, time.monotonic() - STATE.rec_started) if STATE.rec_active and STATE.rec_started else STATE.rec_elapsed_s
            if STATE.rec_active:
                STATE.rec_count = int(elapsed * 42)
                if elapsed >= STATE.rec_duration_s:
                    STATE.rec_active = False
                    STATE.rec_saved = True
                    STATE.rec_elapsed_s = float(STATE.rec_duration_s)
                    elapsed = STATE.rec_elapsed_s
            self.send_obj({
                "active": STATE.rec_active,
                "finalizing": False,
                "count": STATE.rec_count,
                "queued": STATE.rec_count,
                "dropped": STATE.rec_dropped,
                "file_bytes": STATE.rec_count * 19,
                "max_file_bytes": 7_000_000,
                "elapsed_ms": int(elapsed * 1000),
                "duration_ms": STATE.rec_duration_s * 1000,
                "saved": STATE.rec_saved,
                "storage_full": False,
                "stop_reason": "time_limit" if STATE.rec_saved and elapsed >= STATE.rec_duration_s else ("manual" if STATE.rec_saved else "none"),
                "filter": len([x for x in STATE.rec_filter.split(",") if x.strip()]),
                "can2_rx_3c2": STATE.rec_count // 20,
                "can2_rx_3c2_mux1": STATE.rec_count // 24,
                "can2_rx_3c2_mux1_idle": STATE.rec_count // 30,
                "can2_rx_3c2_volume_up": 1 if STATE.rec_count else 0,
                "can2_rx_3c2_volume_down": 1 if STATE.rec_count else 0,
                "can2_rx_3c2_speed_nonzero": 0,
                "can2_tx_3c2": 0,
                "can2_tx_error_3c2": 0,
                "can1_rx_3c2": 0,
                "can1_tx_3c2": 0,
                "can1_tx_error_3c2": 0,
                "can1_rx_399": STATE.rec_count // 10,
                "can1_rx_045": STATE.rec_count // 18,
                "can1_tx_045": 0,
                "can1_tx_error_045": 0,
                "can2_rx_045": STATE.rec_count // 22,
                "can2_tx_045": 0,
                "can2_tx_error_045": 0,
                "wheel_045_lever_up": 1 if STATE.rec_count else 0,
                "wheel_045_lever_down": 1 if STATE.rec_count else 0,
                "wheel_045_lever_forward": 0,
                "wheel_045_lever_back": 0,
                "wheel_045_cond_up": 0,
                "wheel_045_cond_down": 0,
                "wheel_045_switch_active": 0,
                "wheel_045_last_bus": 1,
                "wheel_045_last_counter": 3,
                "wheel_045_last_crc": 0xA5,
            })
        elif path == "/rec_download":
            self.send_text("ts,id,dlc,data\n0,1021,8,00 00 00 3c 20 00 00 00\n", "text/csv")
        elif path == "/rec_meta":
            self.send_obj({
                "version": VERSION,
                "duration_ms": STATE.rec_duration_s * 1000,
                "elapsed_ms": int(STATE.rec_elapsed_s * 1000),
                "frames": STATE.rec_count,
                "dropped": STATE.rec_dropped,
                "gear_gate_required": False,
                "ap_context_status_min": 3,
                "ap_context_status_max": 6,
                "filter_ids": [x for x in STATE.rec_filter.split(",") if x],
                "wheel_3c2": {
                    "can2_native_visible": STATE.rec_count > 0,
                    "can2_mux1_visible": STATE.rec_count > 0,
                    "can2_mux1_idle_visible": STATE.rec_count > 0,
                    "can2_rx": STATE.rec_count // 20,
                    "can2_mux1": STATE.rec_count // 24,
                    "can2_mux1_idle": STATE.rec_count // 30,
                    "volume_up": 1 if STATE.rec_count else 0,
                    "volume_down": 1 if STATE.rec_count else 0,
                    "speed_nonzero": 0,
                    "tx_ok": 0,
                    "tx_error": 0,
                    "can1_rx": 0,
                    "can1_399_rx": STATE.rec_count // 10,
                },
                "wheel_045": {
                    "receive_only": True,
                    "can1_rx": STATE.rec_count // 18,
                    "can2_rx": STATE.rec_count // 22,
                    "can1_tx": 0,
                    "can2_tx": 0,
                    "lever_up": 1 if STATE.rec_count else 0,
                    "lever_down": 1 if STATE.rec_count else 0,
                    "last_bus": 1,
                    "last_counter": 3,
                    "last_crc": 0xA5,
                },
                "nag_start": {"valid": True, "state": "armed", "ap_state": 6, "tx370": 0},
                "nag_end": {"valid": STATE.rec_saved, "state": "support", "ap_state": 6, "tx370": 42},
            })
        elif path == "/ap_status":
            self.send_obj(
                {
                    "ssid": STATE.ap_ssid,
                    "ip": "100.100.1.1",
                    "clients": STATE.ap_clients,
                    "stored": True,
                    "hidden": STATE.ap_hidden,
                }
            )
        elif path == "/wifi_status":
            self.send_obj(
                {
                    "connected": STATE.wifi_connected,
                    "ssid": STATE.wifi_ssid,
                    "stored": bool(STATE.wifi_networks),
                    "count": len(STATE.wifi_networks),
                    "active": STATE.wifi_active,
                    "ip": STATE.wifi_ip,
                    "static": False,
                }
            )
        elif path == "/system_status":
            uptime = STATE.uptime()
            heap_total = 327680
            heap_free = 214000 - (uptime % 9000)
            self.send_obj(
                {
                    "chip": "ESP32-S3",
                    "target": "esp32s3",
                    "cores": 2,
                    "revision": 2,
                    "cpu_mhz": 160,
                    "idf": "6.0.1",
                    "firmware": VERSION,
                    "mac": "A4:CB:8F:DA:D5:44",
                    "reset": "software",
                    "uptime": uptime,
                    "tasks": 18,
                    "core": uptime % 2,
                    "heap_total": heap_total,
                    "heap_free": heap_free,
                    "heap_min": 188000,
                    "heap_largest": 128000,
                    "internal_free": heap_free,
                    "psram_total": 0,
                    "psram_free": 0,
                    "flash_size": 16 * 1024 * 1024,
                    "flash_speed": 80_000_000,
                    "app_addr": 0x10000,
                    "app_size": 4 * 1024 * 1024,
                    "app_used": 1_025_343,
                    "app_label": "app0",
                    "spiffs_ok": True,
                    "spiffs_total": 0x7E0000,
                    "spiffs_used": 128 * 1024,
                    "wifi_connected": STATE.wifi_connected,
                    "wifi_rssi": -43 if STATE.wifi_connected else None,
                    "ap_clients": STATE.ap_clients,
                    "temp_c": 41.6,
                }
            )
        elif path == "/wifi_networks":
            self.send_obj(
                {
                    "count": len(STATE.wifi_networks),
                    "max": 4,
                    "active": STATE.wifi_active,
                    "networks": STATE.wifi_networks,
                }
            )
        elif path == "/wifi_scan":
            self.send_obj(
                {
                    "networks": [
                        {"ssid": "Home-WiFi", "rssi": -42, "ch": 6, "enc": True},
                        {"ssid": "Tesla-Service", "rssi": -67, "ch": 11, "enc": True},
                        {"ssid": "EV-CAN-WAVESHARE", "rssi": -31, "ch": 1, "enc": True},
                    ]
                }
            )
        elif path == "/gateway_status":
            self.send_obj(
                {
                    "enabled": STATE.gateway_enabled,
                    "nat": STATE.gateway_nat and STATE.wifi_connected,
                    "mode": STATE.gateway_mode,
                    "strict": STATE.gateway_strict,
                    "blocked": sum(x["count"] for x in STATE.gateway_blocked),
                    "upstream": "8.8.8.8",
                }
            )
        elif path == "/gateway_dns":
            self.send_obj(
                {
                    "enabled": STATE.gateway_enabled,
                    "mode": STATE.gateway_mode,
                    "strict": STATE.gateway_strict,
                    "blacklist": STATE.gateway_blacklist,
                    "whitelist": STATE.gateway_whitelist,
                }
            )
        elif path == "/gateway_dns_test":
            domain = qs.get("domain", [""])[0]
            self.send_obj(_dns_decision(domain))
        elif path == "/gateway_blocked":
            rows = []
            for item in STATE.gateway_blocked:
                dom = item["domain"].lower()
                decision = _dns_decision(dom)
                blacklisted = decision["blacklisted"]
                whitelisted = decision["whitelisted"]
                rows.append({**item, "blacklisted": blacklisted, "whitelisted": whitelisted, "canWhitelist": not blacklisted})
            self.send_obj(rows)
        elif path == "/can_pins":
            self.send_obj({"tx": 15, "rx": 16, "customized": False})
        elif path == "/settings_export":
            self.send_obj(
                {
                    "version": VERSION,
                    "ap": {"ssid": STATE.ap_ssid, "pass": "", "hidden": STATE.ap_hidden},
                    "wifi": {"ssid": STATE.wifi_ssid, "pass": "", "static": False, "ip": "", "gw": "", "mask": "", "dns": ""},
                    "hw3": {"offsetSlew": STATE.hw3_offset_slew, "slewRate": STATE.hw3_slew_rate},
                    "can": {"tx": 15, "rx": 16},
                    "beta": STATE.beta,
                }
            )
        elif path == "/update_check":
            self.send_obj(
                {
                    "ok": True,
                    "current": VERSION,
                    "latest": VERSION,
                    "update": False,
                    "prerelease": True,
                    "artifact": "waveshare_ESP32_S3_RS485_CAN",
                    "tag": "local-sim",
                    "url": "",
                }
            )
        elif path == "/update_beta":
            self.send_obj({"beta": STATE.beta})
        elif path == "/auto_update":
            self.send_obj({"enabled": STATE.auto_update})
        elif path == "/sim/state":
            self.send_obj(
                {
                    "can": STATE.can,
                    "injection": STATE.injection,
                    "wifi_connected": STATE.wifi_connected,
                    "gateway_enabled": STATE.gateway_enabled,
                    "gateway_nat": STATE.gateway_nat,
                }
            )
        else:
            self.send_text("not found", status=404)

    def do_POST(self) -> None:
        path = urlparse(self.path).path
        form = self.read_form()
        if path == "/config":
            if "hw" in form:
                STATE.hw = max(0, min(2, int(form["hw"] or 0)))
            if "sp" in form:
                STATE.sp = max(0, min(4, int(form["sp"] or 0)))
            if "spa" in form:
                STATE.sp_auto = form["spa"] == "1"
            if "can" in form:
                STATE.injection = form["can"] == "1"
            if "bootCan" in form:
                STATE.boot_can = form["bootCan"] == "1"
            if "hw3OffsetSlew" in form:
                STATE.hw3_offset_slew = form["hw3OffsetSlew"] == "1"
            if "hw3SlewRate" in form:
                STATE.hw3_slew_rate = max(1, min(25, int(form["hw3SlewRate"] or 5)))
            # AP gate controls (saveApGateControls posts these to /config).
            # ap_delay_ms silently ignored: removed in 4.5.0-beta06.
            if "apg" in form:
                STATE.ap_gate = self.form_bool(form["apg"])
            if "ap_auto_restore" in form:
                STATE.ap_auto_restore = self.form_bool(form["ap_auto_restore"])
            self.send_obj({"ok": True})
        elif path == "/legacy_fsd_config":
            if "policy" in form:
                policy = str(form.get("policy") or "legacy_stable")
                if policy in ("legacy_tesla_parity", "tesla_parity", "2"):
                    STATE.legacy_fsd_policy = "legacy_tesla_parity"
                elif policy in ("experimental", "legacy_experimental", "1"):
                    STATE.legacy_fsd_policy = "legacy_experimental"
                elif policy in ("stable", "legacy_stable", "0"):
                    STATE.legacy_fsd_policy = "legacy_stable"
                else:
                    STATE.legacy_fsd_policy = "legacy_stable"
            if "mux1" in form:
                STATE.legacy_fsd_mux1 = self.form_bool(form["mux1"])
            if "profile" in form:
                STATE.legacy_fsd_profile = self.form_bool(form["profile"])
            if "vision" in form:
                STATE.legacy_fsd_vision = self.form_bool(form["vision"])
            self.send_obj(STATE.legacy_fsd_config())
        elif path == "/mode_hw":
            if "mode" in form:
                mode = str(form["mode"])
                if mode in ("Auto", "auto"):
                    STATE.hw = 0
                elif mode in ("legacy", "Legacy"):
                    STATE.hw = 1
                elif mode.upper() == "HW3":
                    STATE.hw = 2
                elif mode.upper() == "HW4":
                    STATE.hw = 3
            if "value" in form:
                STATE.hw = max(0, min(3, int(form["value"] or 0)))
            if "hw" in form:
                STATE.hw = max(0, min(3, int(form["hw"] or 0)))
            self.send_obj(STATE.mode_hw_config())
        elif path == "/drive_profile":
            if "profile" in form or "mode" in form:
                profile = str(form.get("profile") or form.get("mode") or "Auto").lower()
                profiles = {"auto": 0, "sloth": 1, "chill": 2, "normal": 3, "hurry": 4, "max": 5}
                STATE.drive_profile = profiles.get(profile, 0)
            if "value" in form:
                STATE.drive_profile = max(0, min(5, int(form["value"] or 0)))
            STATE.sp_auto = STATE.drive_profile == 0
            if STATE.drive_profile in (1, 2):
                STATE.sp = 0
            elif STATE.drive_profile in (4, 5):
                STATE.sp = 2
            elif STATE.drive_profile == 3:
                STATE.sp = 1
            self.send_obj(STATE.drive_profile_config())
        elif path == "/speed_strategy":
            if "strategy" in form:
                strategy = str(form["strategy"])
                STATE.speed_strategy = strategy if strategy in ("fixed", "auto", "custom") else "auto"
            self.send_obj({"ok": True, "strategy": STATE.speed_strategy})
        elif path == "/speed_custom":
            for key in ("manualPct", "autoMinPct", "autoMaxPct", "cp1", "cp2", "cp3", "cp4"):
                if key in form:
                    STATE.speed_custom[key] = int(form[key] or 0)
            self.send_obj({"ok": True, **STATE.speed_custom})
        elif path == "/legacy_speed_config":
            if "bus" in form and str(form["bus"]) in ("Auto", "CAN1", "CAN2"):
                STATE.legacy_speed_bus = str(form["bus"])
            if "smooth_down" in form:
                STATE.legacy_speed_smooth = self.form_bool(form["smooth_down"])
            if "smooth_rate_kph_s" in form:
                STATE.legacy_speed_rate = max(1, min(20, int(form["smooth_rate_kph_s"] or 5)))
            self.send_obj(
                {
                    "ok": True,
                    "bus": STATE.legacy_speed_bus,
                    "smooth_down": STATE.legacy_speed_smooth,
                    "smooth_rate_kph_s": STATE.legacy_speed_rate,
                }
            )
        elif path == "/power_mgmt":
            if "autoShutdown" in form:
                STATE.auto_shutdown = self.form_bool(form["autoShutdown"])
            if "wifiAutoOff" in form:
                STATE.wifi_auto_off = self.form_bool(form["wifiAutoOff"])
            self.send_obj({"autoShutdown": STATE.auto_shutdown, "wifiAutoOff": STATE.wifi_auto_off})
        elif path == "/dns_rules":
            if "enabled" in form:
                STATE.gateway_enabled = self.form_bool(form["enabled"])
            if "mode" in form:
                STATE.gateway_mode = int(form.get("mode") or STATE.gateway_mode)
            if "strict" in form:
                STATE.gateway_strict = self.form_bool(form["strict"])
            if "blacklist" in form:
                STATE.gateway_blacklist = str(form["blacklist"])
            if "whitelist" in form:
                STATE.gateway_whitelist = str(form["whitelist"])
            self.send_obj({"ok": True})
        elif path == "/hotspot_config":
            if form.get("ssid"):
                STATE.ap_ssid = str(form["ssid"])[:32]
            if "hidden" in form:
                STATE.ap_hidden = self.form_bool(form["hidden"])
            self.send_obj({"ok": True, "msg": "Saved in simulator"})
        elif path == "/lighting_config":
            if "enabled" in form:
                STATE.light_enabled = self.form_bool(form["enabled"])
            if "count" in form:
                STATE.light_count = max(1, min(20, int(form["count"] or STATE.light_count)))
            if "frequency" in form:
                STATE.light_frequency = str(form["frequency"])
            if "rear_fog_strategy" in form:
                strategy = str(form["rear_fog_strategy"])
                STATE.rear_fog_value = {"off": 0, "strobe": 1, "f1": 2, "continuous": 3}.get(strategy, int(strategy or 0) if strategy.isdigit() else 0)
            self.send_obj({"ok": True})
        elif path == "/burst":
            if "enabled" in form:
                STATE.burst_enabled = self.form_bool(form["enabled"])
            if "count" in form:
                STATE.burst_count = max(1, min(20, int(form["count"] or STATE.burst_count)))
            if "on_ms" in form:
                STATE.burst_on_ms = max(10, int(form["on_ms"] or STATE.burst_on_ms))
            if "off_ms" in form:
                STATE.burst_off_ms = max(10, int(form["off_ms"] or STATE.burst_off_ms))
            self.send_obj({"ok": True, "enabled": STATE.burst_enabled, "phases_left": 0})
        elif path == "/defense_config":
            if "enabled" in form:
                STATE.defense_enabled = self.form_bool(form["enabled"])
            if "bionic_steering" in form:
                STATE.bionic_steering = self.form_bool(form["bionic_steering"])
            if "dnd_volume" in form:
                STATE.dnd_volume = self.form_bool(form["dnd_volume"])
            if "legacy_scroll_dnd" in form:
                STATE.dnd_volume = self.form_bool(form["legacy_scroll_dnd"])
            if "dnd_speed" in form:
                STATE.dnd_speed = self.form_bool(form["dnd_speed"])
            if "ap_eap_compatible" in form:
                STATE.ap_eap_compatible = self.form_bool(form["ap_eap_compatible"])
            if "abort_guard" in form:
                STATE.abort_guard = self.form_bool(form["abort_guard"])
            if "ota_guard_override" in form:
                STATE.ota_guard_override = self.form_bool(form["ota_guard_override"])
                if STATE.ota_guard_override:
                    STATE.vehicle_ota = False
            if "jitter" in form:
                STATE.jitter = self.form_bool(form["jitter"])
            self.send_obj({"ok": True})
        elif path == "/fog_light":
            if "fogStrategy" in form:
                STATE.rear_fog_value = max(0, min(3, int(form["fogStrategy"] or 0)))
            self.send_obj({"ok": True, "active": False, "reason": "idle"})
        elif path == "/strobe_cont":
            self.send_obj({"ok": True, "active": False})
        elif path == "/led_brightness":
            STATE.led_brightness = max(0, min(255, int(form.get("b", STATE.led_brightness) or 0)))
            self.send_obj({"ok": True})
        elif path == "/logging":
            if "eprn" in form:
                STATE.enable_print = form["eprn"] == "1"
            self.send_obj({"ok": True})
        elif path == "/disable":
            STATE.injection = False
            self.send_text("Injection stopped.")
        elif path == "/reset_stats":
            STATE.rx = 0
            STATE.tx = 0
            STATE.txerr = 0
            self.send_obj({"ok": True})
        elif path == "/rec_start":
            STATE.rec_active = True
            STATE.rec_saved = False
            STATE.rec_count = 0
            STATE.rec_dropped = 0
            STATE.rec_started = time.monotonic()
            STATE.rec_elapsed_s = 0.0
            STATE.rec_duration_s = int(form.get("duration_s", "120"))
            STATE.rec_filter = form.get("ids", "045,118,129,186,238,292,370,389,399,3C2,3EE,3FD,47D,47F,488,5D9")
            if STATE.rec_duration_s not in (60, 90, 120):
                STATE.rec_active = False
                self.send_obj({"ok": False, "error": "duration_s must be 60, 90, or 120"}, 400)
                return
            self.send_obj({"ok": True, "duration_s": STATE.rec_duration_s})
        elif path == "/rec_stop":
            if STATE.rec_active:
                elapsed = max(0.0, time.monotonic() - STATE.rec_started)
                STATE.rec_elapsed_s = min(elapsed, float(STATE.rec_duration_s))
                STATE.rec_count = max(STATE.rec_count, int(STATE.rec_elapsed_s * 42))
                STATE.rec_active = False
                STATE.rec_saved = True
            self.send_obj({"ok": True, "finalizing": False})
        elif path == "/ap_config":
            if form.get("ssid"):
                STATE.ap_ssid = form["ssid"][:32]
            STATE.ap_hidden = form.get("hidden") in ("1", "true")
            self.send_obj({"ok": True, "msg": "Saved in simulator"})
        elif path == "/wifi_config":
            ssid = form.get("ssid", "Sim-WiFi")[:32]
            idx = int(form.get("idx", -1) or -1)
            item = {"idx": idx if idx >= 0 else len(STATE.wifi_networks), "ssid": ssid, "hasPass": bool(form.get("pass")), "static": form.get("static") == "1"}
            if idx >= 0 and idx < len(STATE.wifi_networks):
                STATE.wifi_networks[idx] = item
            else:
                STATE.wifi_networks.append(item)
            STATE.wifi_ssid = ssid
            STATE.wifi_connected = True
            STATE.wifi_active = item["idx"]
            self.send_obj({"ok": True})
        elif path == "/wifi_delete":
            idx = int(form.get("idx", -1) or -1)
            STATE.wifi_networks = [n for n in STATE.wifi_networks if n.get("idx") != idx]
            STATE.wifi_active = STATE.wifi_networks[0]["idx"] if STATE.wifi_networks else -1
            self.send_obj({"ok": True})
        elif path == "/gateway_dns":
            STATE.gateway_enabled = form.get("enabled", "0") in ("1", "true")
            STATE.gateway_mode = int(form.get("mode", STATE.gateway_mode) or 0)
            STATE.gateway_strict = form.get("strict", "0") in ("1", "true")
            STATE.gateway_blacklist = form.get("blacklist", STATE.gateway_blacklist)
            STATE.gateway_whitelist = form.get("whitelist", STATE.gateway_whitelist)
            self.send_obj({"ok": True})
        elif path == "/gateway_blocked_clear":
            STATE.gateway_blocked = []
            self.send_obj({"ok": True})
        elif path == "/gateway_whitelist_add":
            domain = form.get("domain", "").strip().lower()
            blacklist = _dns_rules(STATE.gateway_blacklist)
            if _domain_rule_match_len(domain, blacklist) > 0:
                self.send_obj({"ok": False, "error": "domain is blacklisted"}, status=409)
                return
            whitelist = _dns_rules(STATE.gateway_whitelist)
            if _domain_rule_match_len(domain, whitelist) > 0:
                self.send_obj({"ok": True, "already": True})
                return
            STATE.gateway_whitelist = (STATE.gateway_whitelist.rstrip() + "\n" + domain).strip()
            self.send_obj({"ok": True})
        elif path == "/can_pins":
            self.send_obj({"ok": True, "reboot": True})
        elif path in ("/settings_import", "/update_install", "/reboot"):
            self.send_obj({"ok": True, "reboot": path in ("/settings_import", "/reboot")})
        elif path == "/update_beta":
            if "beta" in form:
                STATE.beta = form["beta"] == "1"
            self.send_obj({"beta": STATE.beta})
        elif path == "/auto_update":
            if "enabled" in form:
                STATE.auto_update = form["enabled"] == "1"
            self.send_obj({"enabled": STATE.auto_update})
        else:
            self.send_text("not found", status=404)


def main() -> int:
    parser = argparse.ArgumentParser(description="Run the local WebUI simulator.")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8088)
    args = parser.parse_args()
    httpd = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"EV WebUI simulator: http://{args.host}:{args.port}")
    print("Press Ctrl+C to stop.")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
