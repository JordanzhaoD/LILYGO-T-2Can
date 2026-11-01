#!/usr/bin/env/python3
"""Task 7 Steps 7+8: fill-to-refusal, no-overwrite hash, reboot rebuild.

  A. md5 baseline of seq42 bin (post step-5 board).
  B. Fill loop: flood -> drain -> lock(pre=110) until paused:true (max 6).
  C. Lock while paused -> refused "no space"; no overwrite: md5(seq42) same.
  D. Reboot (esptool connect); metas survive; protection honored (still
     paused); md5(seq42) same again.
  E. Cleanup: delete every session -> paused clears, full 9MB free.
"""
import hashlib
import json
import subprocess
import time
import urllib.request

import serial

PORT = "/dev/cu.usbmodem5CF71553661"
BASE = "http://192.168.1.146"
ESPTOOL = "/Users/ziwind/.platformio/penv/bin/esptool"
LOG = open("/tmp/verify_step78.log", "w", buffering=1)


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


def dl(seq):
    with urllib.request.urlopen(
            BASE + "/session_download?seq=%d&fmt=bin" % seq, timeout=180) as r:
        return r.read()


def md5(seq):
    return hashlib.md5(dl(seq)).hexdigest()


def wait_board():
    for _ in range(40):
        time.sleep(4)
        d = http("/ring_status")
        if "err" not in d and "http_err" not in d:
            return d
    raise SystemExit("board did not come up")


def drain(ser):
    for i in range(14):
        time.sleep(5)
        st = http("/ring_status")
        fps = st.get("fps", {}).get("a", -1)
        if fps == 0 and i >= 1:
            return
    log("  drain: fps never hit 0 (starved counters?) continuing")


st = wait_board()
log("board up: paused=%s free=%s" % (st.get("paused"),
                                     st.get("sessions_space_free")))
base42 = md5(42)
log("A. md5(seq42) baseline =", base42)

ser = serial.Serial(PORT, 115200, timeout=1)
time.sleep(2)
try:
    ser.reset_input_buffer()
except Exception:
    pass

paused = False
for cyc in range(6):
    log("\nB. fill cycle %d: flood 45s" % cyc)
    ser.write(b"rv2_flood 6000 45 7\n")
    t0 = time.time()
    while time.time() - t0 < 60:
        time.sleep(5)
        st = http("/ring_status")
        if st.get("fps", {}).get("a", 0) == 0 and time.time() - t0 > 20:
            break
    drain(ser)
    r = http("/ring_lock", {"pre_s": 110, "post_s": 5, "bus_mask": 7})
    log("  lock -> %s" % json.dumps(r)[:160])
    for _ in range(40):
        time.sleep(3)
        st = http("/ring_status")
        if st.get("lock", {}).get("state") == "idle":
            break
    st = http("/ring_status")
    log("  after: paused=%s free=%s seq=%s" %
        (st.get("paused"), st.get("sessions_space_free"), st.get("seq")))
    if st.get("paused"):
        paused = True
        log("  grid full: writer paused after %d cycles" % (cyc + 1))
        break
if not paused:
    log("C-SKIP: never paused within 6 cycles (record state and stop)")

log("\nC. lock while full:")
r = http("/ring_lock", {"pre_s": 30, "post_s": 5, "bus_mask": 7})
log("  refused response: %s" % json.dumps(r)[:200])
log("  md5(seq42) after fill =", md5(42), "unchanged:", md5(42) == base42)

ser.close()
log("\nD. reboot via esptool connect...")
subprocess.run([ESPTOOL, "--port", PORT, "--chip", "esp32-p4",
                "--baud", "115200", "read_flash", "0x0", "0x10",
                "/tmp/step8_boot.bin"], capture_output=True, timeout=180)
st = wait_board()
log("  after reboot: paused=%s free=%s" %
    (st.get("paused"), st.get("sessions_space_free")))
sess = http("/sessions")
lst = sess if isinstance(sess, list) else sess.get("sessions", [])
log("  metas survive: %s sessions: %s" %
    (len(lst), [s.get("seq") for s in lst]))
r = http("/ring_lock", {"pre_s": 30, "post_s": 5, "bus_mask": 7})
log("  protection honored (still refused): %s" % json.dumps(r)[:160])
log("  md5(seq42) after reboot =", md5(42), "unchanged:", md5(42) == base42)

log("\nE. cleanup: delete all sessions")
for s in lst:
    log("  delete %s -> %s" % (s.get("seq"), json.dumps(
        http("/session_delete", {"seq": s.get("seq")}))))
st = http("/ring_status")
log("  final: paused=%s free=%s" %
    (st.get("paused"), st.get("sessions_space_free")))
log("done -> /tmp/verify_step78.log")
