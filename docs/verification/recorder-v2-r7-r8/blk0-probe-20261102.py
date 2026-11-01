#!/usr/bin/env/python3
"""Block-0 summary hole: discriminator A (wide-window lock) + B (flash dump).

A. Lock with pre=295s (ring capacity) post=5. If the walk-back STILL
   reports start block 1, slot 0 is EMPTY (lastTsUs==0). If it reports
   0 or 143..., slot 0 holds a stale value instead.
B. esptool dump blocks 0,1,143; decode the 18-byte frame grid (global
   grid anchored at partition offset 32; per-block anchors rotate:
   block0=32, block1=16, block143=12); report per-block live frame
   counts, tsUs span, 0xFF runs, and 143->0 continuity.
"""
import json
import re
import struct
import subprocess
import time
import urllib.request

import serial

PORT = "/dev/cu.usbmodem5CF71553661"
BASE = "http://192.168.1.146"
ESPTOOL = "/Users/ziwind/.platformio/penv/bin/esptool"
LOG = open("/tmp/blk0_probe.log", "w", buffering=1)

ser = None
serbuf = ""


def log(*a):
    m = " ".join(str(x) for x in a)
    LOG.write(m + "\n")
    print(m, flush=True)


def http(path, payload=None, timeout=15):
    try:
        if payload is None:
            with urllib.request.urlopen(BASE + path, timeout=timeout) as r:
                return json.loads(r.read().decode())
        req = urllib.request.Request(
            BASE + path, data=json.dumps(payload).encode(),
            method="POST", headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return json.loads(r.read().decode())
    except Exception as e:  # noqa: BLE001
        return {"err": repr(e)}


def pump():
    global serbuf
    try:
        n = ser.in_waiting
        if n:
            serbuf += ser.read(n).decode(errors="replace")
    except Exception:
        pass


log("A: wide-window lock discriminator")
ser = serial.Serial(PORT, 115200, timeout=1)
time.sleep(5)
try:
    ser.reset_input_buffer()
except Exception:
    pass
serbuf = ""
d = {}
for _ in range(10):
    d = http("/ring_status")
    if "err" not in d:
        break
    time.sleep(3)
log("  status: lock=%s" % json.dumps(d.get("lock"))[:80])
r = http("/ring_lock", {"pre_s": 295, "post_s": 5, "bus_mask": 7})
log("  lock -> %s" % json.dumps(r)[:100])
t0 = time.time()
while time.time() - t0 < 40:
    pump()
    d = http("/ring_status")
    if "err" in d:
        time.sleep(2)
        continue
    if d.get("lock", {}).get("state") == "idle":
        break
    time.sleep(2)
time.sleep(2)
pump()
tr = re.findall(
    r"session (\d+) locked zero-copy.*?pre=(\d+)ms blocks (\d+)\.(\d+)", serbuf)
if tr:
    seq, pre, a, b = tr[-1]
    log("  WIDE WINDOW: session %s blocks %s..%s" % (seq, a, b))
    wide = (int(a), int(b))
    http("/session_delete", {"seq": int(seq)})
else:
    log("  no lock trace captured; serial tail:\n" + serbuf[-600:])
    wide = None
log("  verdict: %s" % (
    "SLOT0 EMPTY (stopped at 1 even at 295s)" if wide and wide[0] == 1
    else "slot0 has a value (walk reached %s)" % (wide,)))

# ------------------------------------------------------------------ B
log("B: flash dump blocks 0,1,143")
ser.close()
time.sleep(2)
for addr, length, path in (
        (0x16E0000, 0x20000, "/tmp/blk_0_1.bin"),
        (0x1FD0000, 0x10000, "/tmp/blk_143.bin")):
    cp = subprocess.run(
        [ESPTOOL, "--port", PORT, "--chip", "esp32-p4", "--baud", "115200",
         "read_flash", hex(addr), hex(length), path],
        capture_output=True, timeout=300)
    ok = cp.returncode == 0
    log("  dump %s -> rc=%d" % (path, cp.returncode))
    if not ok:
        log(cp.stderr.decode()[-300:])
        raise SystemExit(1)
    time.sleep(3)

FSZ = 18


def grid_anchor(block):
    """First grid point offset inside this block (grid = 32+k*18)."""
    base = block * 65536
    k = -(-max(base - 32, 0) // 18)  # ceil
    return 32 + k * 18 - base


def scan_grid(seg, anchor):
    out = []
    pos = anchor
    while pos + FSZ <= len(seg):
        ts, fid, ln, bus = struct.unpack_from("<IIBB", seg, pos)
        out.append((pos, ts, fid, ln, bus))
        pos += FSZ
    return out


def erased_runs(seg):
    runs = []
    i = 0
    while i < len(seg):
        if seg[i] == 0xFF:
            j = i
            while j < len(seg) and seg[j] == 0xFF:
                j += 1
            if j - i >= 256:
                runs.append((i, j))
            i = j
        else:
            i += 1
    return runs


data01 = open("/tmp/blk_0_1.bin", "rb").read()
d143 = open("/tmp/blk_143.bin", "rb").read()

results = {}
for name, seg, blk in (("block0", data01[0:0x10000], 0),
                       ("block1", data01[0x10000:0x20000], 1),
                       ("block143", d143, 143)):
    anc = grid_anchor(blk)
    fr = scan_grid(seg, anc)
    live = [f for f in fr if f[1] not in (0, 0xFFFFFFFF)]
    results[name] = (live, seg)
    if live:
        tss = [f[1] for f in live]
        gap = [(fr[i][0], fr[i + 1][0]) for i in range(len(fr) - 1)
               if fr[i][1] not in (0, 0xFFFFFFFF)
               and fr[i + 1][1] not in (0, 0xFFFFFFFF)]
        log("  %s (anchor %d): %d slots, live=%d, tsUs %d..%d span %.1fs, "
            "id first=0x%X last=0x%X len last=%d bus last=%d" %
            (name, anc, len(fr), len(live), min(tss), max(tss),
             (max(tss) - min(tss)) / 1e6, live[0][2], live[-1][2],
             live[-1][3], live[-1][4]))
    else:
        log("  %s (anchor %d): NO live frames" % (name, anc))
    runs = erased_runs(seg)
    log("    erased>=256B runs: %s" %
        (["%d..%d" % r for r in runs[:6]] or "none"))

# continuity: last live ts in 143 vs first live ts in 0
l143 = results.get("block143", ([], b""))[0]
l0 = results.get("block0", ([], b""))[0]
if l143 and l0:
    d_ts = l0[0][1] - l143[-1][1]
    log("  continuity 143->0: last143 ts=%d first0 ts=%d delta=%+.3fs" %
        (l143[-1][1], l0[0][1], d_ts / 1e6))
log("B done")
