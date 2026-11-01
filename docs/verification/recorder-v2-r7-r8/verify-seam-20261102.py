#!/usr/bin/env/python3
"""Final-review fix-pass bench v2: C-1 seam wrap + rebuild + I-1 eviction.

Lesson from run 1: console rv2_flood BLOCKS loopTask (console + fps
windows + lock state machine all live there). So: floods run as short
chained console commands, and every lock is issued in the gap AFTER a
flood command returns — the lock walk-back reads the ring's recent past,
it never needs live traffic.

Phases:
  P1 cleanup   delete all -> free 9437184, paused false.
  P2 probe     flood 30s; lock pre=25 -> serial trace gives frontier
               block; bin download len / 25s = true grid byte rate
               (ts-filtered bytes, frame-exact). Delete probe.
  P3 seam      chain 60s floods with ~1s gaps; stop at T_seam+35s
               (frontier ~8 blocks past the seam); lock pre=60 post=10;
               trace MUST show startBlk > frontierBlk (wrapped).
  P4 verify    size non-zero; fmt=bin HTTP 200 (pre-fix: 500);
               len % 18 == 0; decode(bin) == fmt=csv bytes.
  P5 stability 3x60s floods (writer climbs toward the protected front);
               md5 unchanged.
  P6 rebuild   esptool-connect reset; session survives rebuild, md5 same;
               flood 60s more, md5 still same.
  P7 eviction  flood 30s; lock pre=10 mid-fill-range (evicts writer entry
               block); delete it (same-boot re-entry); flood 60s; seam
               md5 unchanged; fresh lock after works.
  P8 cleanup   delete all; free 9437184.
"""
import hashlib
import json
import re
import subprocess
import time
import urllib.error
import urllib.request

import serial

PORT = "/dev/cu.usbmodem5CF71553661"
BASE = "http://192.168.1.146"
ESPTOOL = "/Users/ziwind/.platformio/penv/bin/esptool"
REPO = "/Users/ziwind/my-vibe-project/LILYGO-T-2Can-4.5.0-beta01"
LOG = open("/tmp/verify_seam.log", "w", buffering=1)
GRID = 9437184
FLOOD_S = 60

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
    except urllib.error.HTTPError as e:
        return {"http_err": e.code, "body": e.read().decode()[:200]}
    except Exception as e:  # noqa: BLE001
        return {"err": repr(e)}


def raw(seq, fmt):
    url = "%s/session_download?seq=%d&fmt=%s" % (BASE, seq, fmt)
    try:
        with urllib.request.urlopen(url, timeout=300) as r:
            return r.status, r.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read()[:200]


def md5(seq):
    st, body = raw(seq, "bin")
    if st != 200:
        return "HTTP%d" % st
    return hashlib.md5(body).hexdigest()


def pump():
    global serbuf
    try:
        n = ser.in_waiting
        if n:
            serbuf += ser.read(n).decode(errors="replace")
    except Exception:
        pass


def lock_traces():
    return re.findall(
        r"session (\d+) locked zero-copy.*?pre=(\d+)ms blocks (\d+)\.\.(\d+)",
        serbuf)


def status(timeout=6):
    return http("/ring_status", timeout=timeout)


def ok(d):
    return "err" not in d and "http_err" not in d


def wait_board(limit=40):
    for _ in range(limit):
        pump()
        d = status()
        if ok(d):
            return d
        time.sleep(4)
    raise SystemExit("board did not come up")


def wait_idle(limit=40):
    for _ in range(limit):
        pump()
        d = status()
        if ok(d) and d.get("lock", {}).get("state") == "idle":
            return d
        time.sleep(3)
    log("  lock never returned to idle")
    return status()


def sessions():
    d = http("/sessions", timeout=20)
    return d if isinstance(d, list) else []


def delete_all(tag):
    for s in sessions():
        log("  %s delete %s -> %s" % (tag, s.get("seq"),
                                      json.dumps(http("/session_delete",
                                                      {"seq": s.get("seq")}))))
        pump()


def flood(seconds):
    """One blocking console flood; return when the console is free again."""
    ser.write(b"rv2_flood 6000 %d 7\n" % seconds)
    time.sleep(seconds + 2.5)  # command blocks loopTask exactly `seconds`
    pump()


def flood_until(t_stop):
    while time.time() < t_stop:
        flood(min(FLOOD_S, max(5, t_stop - time.time())))


def do_lock(pre_s, post_s):
    r = http("/ring_lock", {"pre_s": pre_s, "post_s": post_s, "bus_mask": 7})
    log("  lock pre=%d -> %s" % (pre_s, json.dumps(r)[:120]))
    wait_idle()
    time.sleep(1.5)
    pump()
    tr = lock_traces()
    if not tr:
        raise SystemExit("no lock trace on serial")
    seq, pre, a, b = tr[-1]
    log("  session %s: pre=%sms blocks %s..%s" % (seq, pre, a, b))
    return int(seq), int(a), int(b)


# ---------------------------------------------------------------- P1
log("P1 cleanup")
ser = serial.Serial(PORT, 115200, timeout=1)
time.sleep(6)
try:
    ser.reset_input_buffer()
except Exception:
    pass
serbuf = ""
wait_board()
delete_all("P1")
time.sleep(3)
st = status()
log("  paused=%s free=%s (want False / 9437184)" %
    (st.get("paused"), st.get("sessions_space_free")))
if st.get("paused") or st.get("sessions_space_free") != GRID:
    raise SystemExit("P1 FAILED: ring not clean")

# ---------------------------------------------------------------- P2
log("P2 probe: rate + frontier")
flood(30)
seq_p, blk_a, blk_b = do_lock(25, 5)
st, body = raw(seq_p, "bin")
if st != 200:
    raise SystemExit("P2 FAILED: probe download HTTP %d" % st)
rate = len(body) / 25.0
log("  probe bin=%dB -> %.0f B/s (%.2f s/block) frontier blk=%d" %
    (len(body), rate, 65536 / rate, blk_b))
if rate < 5000:
    raise SystemExit("P2 FAILED: implausible rate %.0f B/s" % rate)
http("/session_delete", {"seq": seq_p})
time.sleep(2)

# ---------------------------------------------------------------- P3
# Iterative seam hunt: rate calibration carried ~2x error in run 2 (actual
# write rate ~2x the ts-filtered estimate), so don't project — advance in
# bounded 25s floods and lock a 120s window each round. A window that
# wraps the seam shows startBlk > frontierBlk; any frontier 5..100 blocks
# past the seam wraps with a 100-block window, so this converges in a
# few rounds regardless of rate error. Non-wrapped rounds get deleted.
log("P3 seam hunt from frontier blk=%d (probe rate %.0f B/s)" %
    (blk_b, rate))
seam_seq = a = b = None
for rnd in range(10):
    flood(25)
    seq_i, ai, bi = do_lock(120, 5)
    if ai > bi:
        seam_seq, a, b = seq_i, ai, bi
        log("  WRAPPED RANGE CONFIRMED round %d (start %d > frontier %d)" %
            (rnd + 1, a, b))
        break
    log("  round %d: window %d..%d past the seam — delete, advance" %
        (rnd + 1, ai, bi))
    http("/session_delete", {"seq": seq_i})
if seam_seq is None:
    raise SystemExit("P3 FAILED: seam never wrapped in 10 rounds")

# ---------------------------------------------------------------- P4
log("P4 verify wrapped session %d" % seam_seq)
sess = [s for s in sessions() if s.get("seq") == seam_seq]
if not sess:
    raise SystemExit("P4 FAILED: session not listed")
log("  size=%s frames=%s" % (sess[0].get("size"), sess[0].get("frames")))
if not sess[0].get("size"):
    raise SystemExit("P4 FAILED: wrapped session size is 0")
st, body = raw(seam_seq, "bin")
log("  bin: HTTP %d, %d bytes" % (st, len(body)))
if st != 200:
    raise SystemExit("P4 FAILED: download HTTP %d (C-1 regression)" % st)
if len(body) == 0 or len(body) % 18 != 0:
    raise SystemExit("P4 FAILED: %d bytes, %%18=%d" %
                     (len(body), len(body) % 18))
st2, csv_body = raw(seam_seq, "csv")
bin_path = "/tmp/seam_sess%d.bin" % seam_seq
csv_path = "/tmp/seam_sess%d.csv" % seam_seq
open(bin_path, "wb").write(body)
open(csv_path, "wb").write(csv_body)
dec = subprocess.run(
    ["/opt/homebrew/bin/python3.13", "scripts/decode_blackbox.py", bin_path],
    capture_output=True, cwd=REPO)
log("  csv: HTTP %d, %d bytes; decoder rc=%d" %
    (st2, len(csv_body), dec.returncode))
if dec.returncode != 0:
    raise SystemExit("P4 FAILED: decoder rc=%d %s" %
                     (dec.returncode, dec.stderr[:200]))
if dec.stdout != csv_body:
    n = min(len(dec.stdout), len(csv_body))
    i = next((k for k in range(n) if dec.stdout[k] != csv_body[k]), n)
    raise SystemExit("P4 FAILED: decoder != board csv at byte %d (%d vs %d)"
                     % (i, len(dec.stdout), len(csv_body)))
log("  decode == board csv BYTE-IDENTICAL (%d frames)" % (len(body) // 18))
base_md5 = hashlib.md5(body).hexdigest()
log("  md5 baseline = %s" % base_md5)

# ---------------------------------------------------------------- P5
log("P5 stability: 3 flood cycles toward the protected front")
for i in range(3):
    flood(FLOOD_S)
    d = status()
    log("  cycle %d: free=%s paused=%s" %
        (i + 1, d.get("sessions_space_free"), d.get("paused")))
cur = md5(seam_seq)
log("  md5 after fill = %s unchanged: %s" % (cur, cur == base_md5))
if cur != base_md5:
    raise SystemExit("P5 FAILED: locked seam session mutated")

# ---------------------------------------------------------------- P6
log("P6 reboot + rebuild")
subprocess.run([ESPTOOL, "--port", PORT, "--chip", "esp32-p4",
                "--baud", "115200", "read_flash", "0x0", "0x4",
                "/tmp/seam_boot.bin"], capture_output=True, timeout=180)
serbuf = ""
wait_board()
st = status()
log("  after boot: paused=%s free=%s" %
    (st.get("paused"), st.get("sessions_space_free")))
sess = [s for s in sessions() if s.get("seq") == seam_seq]
if not sess:
    raise SystemExit("P6 FAILED: seam session lost after reboot")
log("  session %d survives rebuild (size=%s)" %
    (seam_seq, sess[0].get("size")))
cur = md5(seam_seq)
log("  md5 after rebuild = %s unchanged: %s" % (cur, cur == base_md5))
if cur != base_md5:
    raise SystemExit("P6 FAILED: rebuild left the session mutated")
flood(FLOOD_S)
cur = md5(seam_seq)
log("  md5 after post-reboot flood = %s unchanged: %s" %
    (cur, cur == base_md5))
if cur != base_md5:
    raise SystemExit("P6 FAILED: post-reboot flood overwrote the seam range")

# ---------------------------------------------------------------- P7
log("P7 I-1 eviction: lock + delete + same-boot re-entry")
flood(30)
cur = md5(seam_seq)
log("  seam md5 before evictor = %s unchanged: %s" %
    (cur, cur == base_md5))
evict_seq, _, _ = do_lock(10, 5)
cur = md5(seam_seq)
log("  seam md5 with evictor locked = %s unchanged: %s" %
    (cur, cur == base_md5))
if cur != base_md5:
    raise SystemExit("P7 FAILED: evictor lock mutated the seam session")
log("  delete evictor (same-boot re-entry over its blocks)")
http("/session_delete", {"seq": evict_seq})
flood(FLOOD_S)
cur = md5(seam_seq)
log("  seam md5 after re-entry flood = %s unchanged: %s" %
    (cur, cur == base_md5))
if cur != base_md5:
    raise SystemExit("P7 FAILED: eviction/delete corrupted the seam session")
do_lock(5, 5)
log("  fresh lock after re-entry OK")

# ---------------------------------------------------------------- P8
log("P8 cleanup")
delete_all("P8")
time.sleep(3)
st = status()
log("  final: paused=%s free=%s" %
    (st.get("paused"), st.get("sessions_space_free")))
log("ALL PHASES PASSED")
ser.close()
