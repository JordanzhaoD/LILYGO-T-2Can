#pragma once

// ─── CAN driver selection ──────────────────────────────────
// Atlas-P4-3CAN board profile. ESP32-P4: primary = native TWAI0 on
// CanA (X179 13/14), secondary = native TWAI1 on CanB (X179 9/10).
// Synced by scripts/platformio_sync_profile.py at build time.
// Copy to platformio_profile_p4.h (git-ignored) and fill real
// credentials. Exactly one driver define may stay enabled.
#define DRIVER_P4_3CAN
// #define DRIVER_TWAI
// #define DRIVER_T2CAN_DUAL

// ─── Vehicle generation ────────────────────────────────────
// Golden vehicle config: Legacy (Jordan's car).
#define LEGACY
// #define HW3
// #define HW4

// ─── Optional features ────────────────────────────────────
#define ISA_SPEED_CHIME_SUPPRESS
#define EMERGENCY_VEHICLE_DETECTION
// #define NAG_KILLER
// #define ENHANCED_AUTOPILOT

// ─── Dashboard credentials ─────────────────────────────────
// Development defaults only. Change before real deployment.
// WiFi hotspot SSID and password (AP gateway is 100.100.1.1).
#define DASH_SSID "Atlas-P4-3CAN"
#define DASH_PASS "12345678"
// OTA update HTTP basic auth.
#define DASH_OTA_USER "admin"
#define DASH_OTA_PASS "changeme"
