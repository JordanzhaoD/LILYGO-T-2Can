// Recorder v2 — always-on PSRAM ring buffer (blackbox) for the P4-3CAN board.
//
// Design: docs/superpowers/specs/2026-11-01-can-recorder-v2-design.md (D1/D6).
// The ring ingests ALL frames from all three buses, un-gated, from boot until
// power-off; per-bus switches only mask what a lock exports (D1 ruling).
// Timestamps are esp_timer_get_time() microseconds (32-bit, wraps every
// 71.6 min); every window computation must use unsigned subtraction, never
// ordered compares. The whole engine is macro-gated so the shared dashboard
// header stays byte-identical on every non-P4 board (T2CAN red line, D8).
#ifndef DASH_RECORDER_V2_H
#define DASH_RECORDER_V2_H

#if defined(DASH_RECORDER_V2)

// Serial/String are provided by the including translation unit: the
// dashboard header pulls platform/espidf_runtime.h (ESP-IDF builds) or
// Arduino.h (native) before this file.
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <string.h>

#include "can_frame_types.h"

// 18-byte packed ring record (spec D6 field list; the "16B" label in the
// spec prose was an arithmetic slip — 4+4+1+1+8 = 18). flags: bit0-1 bus
// (1=A/2=B/3=C), bit2-3 dir (0=R/1=T/2=E), bit4-7 reserved zero.
struct RecRingFrame
{
    uint32_t tsUs;
    uint32_t id;
    uint8_t dlc;
    uint8_t flags;
    uint8_t data[8];
} __attribute__((packed));
static_assert(sizeof(RecRingFrame) == 18, "ring record layout is frozen");

static constexpr uint8_t kRecV2BusA = 1;
static constexpr uint8_t kRecV2BusB = 2;
static constexpr uint8_t kRecV2BusC = 3;

// Ring storage. Single array; head = oldest live frame, count = live frames.
// RX frames arrive from one task (app_can_task drains all three buses) and
// TX frames from sender contexts, so submit guards the update with a short
// critical section — cheaper than lock-free indexing and enough here.
static RecRingFrame *ring = nullptr;
static uint32_t ringCapacity = 0;
static volatile uint32_t ringHead = 0;
static volatile uint32_t ringCount = 0;
static volatile uint32_t ringOverflow = 0;
static portMUX_TYPE recV2Mux = portMUX_INITIALIZER_UNLOCKED;
static bool recV2Initialized = false;

static uint8_t recV2DirBits(char dir)
{
    switch (dir)
    {
    case 'T':
        return 1;
    case 'E':
        return 2;
    default:
        return 0; // 'R'
    }
}

// Idempotent: called once from dashboard setup after the SPIFFS mount.
// 20MB ≈ 9.7 min of full-rate three-bus history (1800fps × 600s × 18B ≈
// 19.4MB); falls back to 10MB, then to disabled (ring == nullptr: submit
// becomes a no-op and the status API reports the engine as disabled).
static void dashRecV2Init()
{
    if (recV2Initialized)
        return;
    recV2Initialized = true;
    const size_t kPrimaryBytes = 20u * 1024u * 1024u;
    const size_t kFallbackBytes = 10u * 1024u * 1024u;
    size_t bytes = kPrimaryBytes;
    void *mem = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    if (!mem)
    {
        bytes = kFallbackBytes;
        mem = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    }
    if (!mem)
    {
        Serial.println("[REC2] no PSRAM for ring, recorder v2 disabled");
        return;
    }
    ring = static_cast<RecRingFrame *>(mem);
    ringCapacity = bytes / sizeof(RecRingFrame);
    ringHead = 0;
    ringCount = 0;
    ringOverflow = 0;
    Serial.printf("[REC2] ring ready: %u frames (%.1f MB)\n",
                  static_cast<unsigned>(ringCapacity),
                  static_cast<double>(bytes) / (1024.0 * 1024.0));
}

// Hot path: one frame into the ring. Never blocks, never drops the caller's
// frame silently — a full ring overwrites the oldest entry and counts it.
static void dashRecV2Submit(const CanFrame &f, uint8_t busTag, char dir)
{
    if (!ring)
        return;
    RecRingFrame r;
    r.tsUs = static_cast<uint32_t>(esp_timer_get_time());
    r.id = f.id;
    r.dlc = (f.dlc <= 8) ? f.dlc : 8;
    r.flags = static_cast<uint8_t>((busTag & 0x03) | (recV2DirBits(dir) << 2));
    memcpy(r.data, f.data, r.dlc);
    if (r.dlc < 8)
        memset(r.data + r.dlc, 0, 8 - r.dlc);
    portENTER_CRITICAL(&recV2Mux);
    if (ringCount >= ringCapacity)
    {
        // Full: overwrite the oldest slot and advance head (rolling loss,
        // by design — overflow counter lands in every session meta).
        ring[ringHead] = r;
        ringHead = (ringHead + 1 < ringCapacity) ? ringHead + 1 : 0;
        ringOverflow = ringOverflow + 1;
    }
    else
    {
        uint32_t idx = ringHead + ringCount;
        if (idx >= ringCapacity)
            idx -= ringCapacity;
        ring[idx] = r;
        ringCount = ringCount + 1;
    }
    portEXIT_CRITICAL(&recV2Mux);
}

// Oldest→newest duration in µs. Unsigned subtraction is wrap-safe: when
// tsUs wraps mid-window the difference is still the true elapsed time.
static uint32_t dashRecV2SpanUs()
{
    if (!ring || ringCount == 0)
        return 0;
    portENTER_CRITICAL(&recV2Mux);
    uint32_t oldest = ring[ringHead].tsUs;
    uint32_t lastIdx = ringHead + ringCount - 1;
    if (lastIdx >= ringCapacity)
        lastIdx -= ringCapacity;
    uint32_t newest = ring[lastIdx].tsUs;
    portEXIT_CRITICAL(&recV2Mux);
    return (uint32_t)(newest - oldest);
}

#endif // DASH_RECORDER_V2
#endif // DASH_RECORDER_V2_H
