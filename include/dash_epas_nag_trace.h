#pragma once

// ──────────────────────────────────────────────────────────────
// dash_epas_nag_trace.h — EPAS-faithful nag diagnostic trace ring
//
// Read-only observability for the EPAS nag chain. Mirrors the compact
// ring shape of include/log_buffer.h (LogRingBuffer): fixed-width
// integer entries (no string pointers), Shared<uint32_t> head for
// single-producer (CAN task) / single-consumer (HTTP dump) safety.
//
// This struct holds NO behaviour beyond push/readSince — it only
// records what the nag engine already decided, so it can never alter
// nag/injection behaviour. Default-off (only armed when dashEpasNagEnabled).
//
// Spec/plan: docs/superpowers/plans/2026-06-17-epas-faithful-nag-unified.md
//            /Users/ziwind/.claude/plans/quirky-rolling-lampson.md
// ──────────────────────────────────────────────────────────────

#include <cstdint>
#include <cstring>

#include "shared_types.h"

// decision: outcome of tryBuildEcho for this 0x370 frame
enum class EpasNagDecision : uint8_t
{
    Suppress = 0, // gate closed OR invalid DLC -> no echo built
    Inject = 1,   // echo built (send result tracked separately via txOk)
};

// gate: mirrors DashEpasNagDiag.lastGate string as a compact enum
enum class EpasNagGate : uint8_t
{
    Idle = 0,
    InvalidDlc = 1,
    Gated = 2,
    Inject = 3,
};

struct EpasNagTraceEntry
{
    uint32_t ts_ms = 0;
    uint8_t decision = 0; // EpasNagDecision
    uint8_t gate = 0;     // EpasNagGate (lastGate snapshot)
    uint8_t handsOn = 0;  // 0x370 byte4 bits[7:6]
    uint8_t dasHandsOn = 0xFF;
    uint8_t apActive = 0;
    uint8_t apBlackout = 0;
    uint16_t realTorque = 0; // 0x370 bytes[2:3] raw
    uint8_t realCnt = 0;     // 0x370 byte6 & 0x0F
    uint16_t echoTorque = 0; // torque the echo used (nextTorque return)
    uint8_t echoCnt = 0;     // (realCnt + 1) & 0x0F
    uint32_t txAttempt = 0;  // diag.tx.txOk + diag.tx.txErr
    uint32_t txOk = 0;
    uint16_t twaiTec = 0; // TWAIDriver::snapshotDiag().txErrorCounter
    uint32_t swTxErr = 0; // dashboard txErrCount (software)
    uint32_t framesSent = 0; // cross-task send counter (P3: bit-19 path signal)
};

struct EpasNagTraceRing
{
    static constexpr size_t kCap = 2048; // 10Hz × 2048 ≈ 3.4 min before wrap

    EpasNagTraceEntry entries[kCap] = {};
    Shared<uint32_t> head{0};
    bool armed{false};  // seen at least one 0x399/0x39B DAS_status frame
    bool active{false}; // armed && seen first 0x370 -> recording

    void push(const EpasNagTraceEntry &e)
    {
        uint32_t h = head;
        entries[h % kCap] = e;
        head = h + 1;
    }

    uint32_t currentHead() const { return head; }

    const EpasNagTraceEntry &at(uint32_t index) const
    {
        return entries[index % kCap];
    }

    // Mirror LogRingBuffer::readSince semantics: returns up to maxOut entries
    // with head index >= since (clamped to the oldest still-held entry).
    int readSince(uint32_t since, EpasNagTraceEntry *out, int maxOut) const
    {
        uint32_t h = head;
        uint32_t oldest = (h > kCap) ? (h - kCap) : 0;
        uint32_t start = (since > oldest) ? since : oldest;

        int count = 0;
        for (uint32_t i = start; i < h && count < maxOut; i++)
        {
            out[count++] = entries[i % kCap];
        }
        return count;
    }
};
