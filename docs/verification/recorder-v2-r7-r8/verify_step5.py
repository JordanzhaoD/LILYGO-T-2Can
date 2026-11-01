#!/usr/bin/env/python3
"""Task 7 Step 5: A-fix bench — sta_drop must NOT reboot the board.

Protocol (Mac never joins the board AP — that leg is deferred to the field;
this bench proves the no-panic half over serial + LAN):
  1. LAN /ring_status resets baseline (serial port not yet open).
  2. Open serial at 115200 (accept a possible port-open reset), settle.
  3. LAN baseline2 after settle.
  4. Serial `sta_drop`; capture 20s of console — any rst:/backtrace/
     Guru Meditation fails; LAN polls during the drop record the outage.
  5. LAN /ring_status after recovery — resets must equal baseline2.
"""
import json
import time
import urllib.request

import serial

PORT = "/dev/cu.usbmodem5CF71553661"
BASE = "http://192.168.1.146"
LOG = open("/tmp/verify_step5.log", "w", buffering=1)


def log(*a):
    m = " ".join(str(x) for x in a)
    LOG.write(m + "\n")
    print(m, flush=True)


def status(timeout=6):
    try:
        with urllib.request.urlopen(BASE + "/ring_status", timeout=timeout) as r:
            return json.loads(r.read().decode())
    except Exception as e:  # noqa: BLE001
        return {"err": repr(e)}


def resets(d):
    return d.get("resets") if "err" not in d and "http_err" not in d else d


log("baseline (LAN, serial closed):", json.dumps(resets(status())))
ser = serial.Serial(PORT, 115200, timeout=1)
time.sleep(6)  # settle: boot banner or silent
try:
    ser.reset_input_buffer()
except Exception:
    pass
base2 = resets(status())
log("baseline2 (serial open):", json.dumps(base2))

log("\n== sta_drop ==")
ser.write(b"sta_drop\n")
t0 = time.time()
out = b""
polls = []
while time.time() - t0 < 24:
    n = ser.in_waiting
    if n:
        out += ser.read(n)
    d = status(timeout=3)
    polls.append((round(time.time() - t0, 1), "err" in d))
    time.sleep(2)
txt = out.decode(errors="replace")
LOG.write("[console]\n" + txt + "\n")

bad = [ln for ln in txt.splitlines() if ("Guru" in ln or "backtrace" in ln
        or ln.startswith("rst:") or "WDT" in ln and "REC2" not in ln)]
log("panic lines: %s" % (bad or "NONE"))
log("LAN polls during drop (t, err): %s" % polls[-6:])

after = resets(status())
log("\nafter (LAN):", json.dumps(after))
same = resets(base2) == after if "resets" in base2 else "base2-err"
log("VERDICT resets unchanged: %s" % same)

ser.close()
log("done -> /tmp/verify_step5.log")
