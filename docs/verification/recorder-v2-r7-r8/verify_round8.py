#!/usr/bin/env/python3
"""Round 7: verify the flush-geometry fix end to end.

Fix under test: flush spans clamped to the entry block + boundary parks
re-anchored on the grid. BEFORE: ~1KB per 64KB block survived (each flush's
erase-ahead destroyed its predecessor's tail); session downloads returned
HTTP 200 + 0 bytes (meta ts-window vs landed bytes desync).

Checks:
  A. mid-flood lock (pre=20) -> session with pre_ms_act ~= 20000
  B. idle lock after drain (pre=60) -> real session (lead-skip regression)
  C. BOTH sessions download > 0 bytes  (the headline fix)
  D. decoder(bin) == board csv byte-identical
  E. flash spot-check: blocks inside session A's bb range are ~FULL of
     frames (non-FF >= 90% of span), no 1.1KB-tail pattern
"""
import json
import subprocess
import time
import urllib.request

import serial

PORT = "/dev/cu.usbmodem5CF71553661"
BASE = "http://192.168.1.146"
ESPTOOL = "/Users/ziwind/.platformio/penv/bin/esptool"
LOG = open("/tmp/verify_round8.log", "w", buffering=1)


def log(*a):
    m = " ".join(str(x) for x in a)
    LOG.write(m + "\n")
    print(m, flush=True)


def http(path, payload=None, timeout=8):
    try:
        if payload is None:
            with urllib.request.urlopen(BASE + path, timeout=timeout) as r:
                return json.loads(r.read().decode())
        req = urllib.request.Request(
            BASE + path, data=json.dumps(payload).encode(),
            method="POST", headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return json.loads(r.read().decode())
    except urllib.error.HTTPError as e:
        return {"http_err": e.code, "body": e.read().decode()[:200]}
    except Exception as e:  # noqa: BLE001
        return {"err": repr(e)}


def status():
    return http("/ring_status")


def dl(seq, fmt):
    """Raw download -> bytes (not JSON)."""
    try:
        with urllib.request.urlopen(
                BASE + "/session_download?seq=%d&fmt=%s" % (seq, fmt),
                timeout=120) as r:
            return r.read()
    except Exception as e:  # noqa: BLE001
        return b""


# 0. board up
for i in range(40):
    time.sleep(4)
    d = status()
    if "err" not in d and "http_err" not in d:
        log("board up after %d polls: %s" % (i + 1, json.dumps(d)[:220]))
        break
else:
    log("BOARD DID NOT COME UP")
    raise SystemExit(1)

ser = serial.Serial(PORT, 115200, timeout=1)
time.sleep(2)
try:
    ser.reset_input_buffer()
except Exception:
    pass

# 1. clean slate: delete every existing session (exercises delete path)
d = http("/sessions")
lst = d if isinstance(d, list) else d.get("sessions", [])
for s in lst:
    seq = s.get("seq")
    if seq is not None:
        log("delete seq %s -> %s" % (seq, json.dumps(
            http("/session_delete", {"seq": seq}))))
log("post-delete sessions: %s" % json.dumps(http("/sessions")))
log("post-delete status: %s" % json.dumps(status()))

# 2. flood + mid-flood lock (session A)
log("\n== flood 6000x45, lock at t+20 (pre=20 post=5) ==")
ser.write(b"rv2_flood 6000 45 7\n")
t0 = time.time()
locked = False
while time.time() - t0 < 160:
    el = time.time() - t0
    st = status()
    log("t+%5.1fs span_s=%-5s lock=%s" %
        (el, st.get("span_s"), st.get("lock", {}).get("state")))
    if not locked and el >= 20:
        log("  -> ring_lock: %s" % json.dumps(
            http("/ring_lock", {"pre_s": 20, "post_s": 5, "bus_mask": 7})
            .get("lock", {}).get("state")))
        locked = True
    if locked and st.get("lock", {}).get("state") == "idle" and el > 100:
        break
    time.sleep(8)

# 3. drain, then idle lock (session B)
for i in range(14):
    time.sleep(5)
    st = status()
    if st.get("span_s") and st.get("fps", {}).get("a") == 0 and i >= 1:
        break
log("\n== idle lock after drain (pre=60 post=5) ==")
log("idle ring_lock: %s" % json.dumps(
    http("/ring_lock", {"pre_s": 60, "post_s": 5, "bus_mask": 7})))
for i in range(20):
    time.sleep(2)
    if status().get("lock", {}).get("state") == "idle":
        break

time.sleep(3)
n = ser.in_waiting
if n:
    txt = ser.read(n).decode(errors="replace")
    LOG.write("[serial]\n" + txt)
    for line in txt.splitlines():
        if "REC2" in line:
            log("[ser] " + line)

# 4. sessions + downloads + decoder compare
d = http("/sessions")
lst = d if isinstance(d, list) else d.get("sessions", [])
metas = {}
for s in sorted(lst, key=lambda x: x.get("seq", 0)):
    log("session: %s" % json.dumps(s))
    metas[s.get("seq")] = s

for seq in sorted(metas):
    b = dl(seq, "bin")
    c = dl(seq, "csv")
    log("download seq %s: bin=%dB csv=%dB" % (seq, len(b), len(c)))
    if b and len(b) % 18 == 0:
        with open("/tmp/r8_sess%s.bin" % seq, "wb") as f:
            f.write(b)
        r = subprocess.run(
            ["/opt/homebrew/bin/python3.13", "-I",
             "scripts/decode_blackbox.py", "/tmp/r8_sess%s.bin" % seq],
            capture_output=True, text=True, timeout=60)
        same = (r.stdout.encode() == c)
        log("decoder seq %s: rc=%d rows=%d csv-identical=%s" %
            (seq, r.returncode, r.stdout.count("\n") - 1, same))
        if not same and r.stdout:
            LOG.write("[decoder mismatch] stdout head:\n" + r.stdout[:300] +
                      "\ncsv head:\n" + c[:300].decode(errors="replace") + "\n")
    elif b:
        log("download seq %s: bin length %% 18 == %d (TORN?)" %
            (seq, len(b) % 18))

# 5. flash spot-check inside session A's bb range (first session with
#    pre_ms_act ~ 20000): expect >= 90% non-FF coverage, on-grid frames
a_seq = None
for seq, m in sorted(metas.items()):
    if m and m.get("pre_ms_act", 0) >= 15000:
        a_seq = seq
        break
if a_seq and metas[a_seq].get("bb_start_off") is not None:
    s0 = metas[a_seq]["bb_start_off"] // 0x10000
    s1 = metas[a_seq]["bb_end_off"] // 0x10000
    log("\n== flash spot-check blocks %d..%d (session %s) ==" %
        (s0, s1 - 1, a_seq))
    for b in ([s0, (s0 + s1) // 2, s1 - 1] if s1 - s0 > 2 else [s0]):
        out = "/tmp/r8_blk%d.bin" % b
        cmd = [ESPTOOL, "--port", PORT, "--chip", "esp32-p4",
               "--baud", "115200", "read_flash",
               hex(0x16E0000 + b * 0x10000), hex(0x10000), out]
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=180)
        if r.returncode != 0:
            log("blk %d read FAILED: %s" % (b, (r.stderr or "")[-120:]))
            continue
        with open(out, "rb") as f:
            data = f.read()
        import struct
        k = (b * 0x10000 - 32 + 17) // 18
        ok = 0
        ts_lo = ts_hi = None
        while 32 + k * 18 + 18 <= (b + 1) * 0x10000:
            loc = 32 + k * 18 - b * 0x10000
            f18 = data[loc:loc + 18]
            if f18 != b"\xff" * 18:
                ts, mid = struct.unpack_from("<II", f18, 0)
                dlc, fl = f18[8], f18[9]
                if mid <= 0x1FFFFFFF and dlc <= 8:
                    ok += 1
                    ts_lo = ts if ts_lo is None else min(ts_lo, ts)
                    ts_hi = ts if ts_hi is None else max(ts_hi, ts)
            k += 1
        log("blk %3d: ongrid=%d nonFF=%d (%.0f%%) ts %.1f..%.1fs" %
            (b, ok, data.count(b"\xff") and 65536 - data.count(b"\xff"),
             100.0 * (65536 - data.count(b"\xff")) / 65536,
             (ts_lo or 0) / 1e6, (ts_hi or 0) / 1e6))
else:
    log("no session A found for flash spot-check")

ser.close()
log("\nverdicts: A pre_ms_act~20000 | B real idle session | C bin>0 | "
    "D decoder==csv | E blocks >=90% full")
log("done -> /tmp/verify_round8.log")
