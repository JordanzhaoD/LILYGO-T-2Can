import json
import time
import urllib.request

import serial

PORT = "/dev/cu.usbmodem5CF71553661"
BOARD = "http://192.168.1.146"
SERIAL_OUT = "/tmp/p4_expE3_serial.log"
EVENTS_OUT = "/tmp/p4_expE3_events.jsonl"

serial_buf = []
trip_seen = False
flood_done = False


def event(kind, **kw):
    rec = {"t": round(time.time(), 1), "kind": kind, **kw}
    with open(EVENTS_OUT, "a") as f:
        f.write(json.dumps(rec) + "\n")
    print(json.dumps(rec), flush=True)


def http_get(path, timeout=4):
    try:
        r = urllib.request.urlopen(BOARD + path, timeout=timeout)
        return True, len(r.read())
    except Exception as e:
        return False, repr(e)


def drain(ser, seconds):
    global trip_seen, flood_done
    end = time.time() + seconds
    while time.time() < end:
        chunk = ser.read(4096)
        if chunk:
            serial_buf.append(chunk)
            text = chunk.decode("utf-8", "replace")
            for line in text.splitlines():
                if ("TCB TRIP" in line or "abort()" in line
                        or "Guru Meditation" in line or "Backtrace:" in line
                        or "ELF file SHA256" in line):
                    event("serial-line", line=line.strip()[:200])
                if "TCB TRIP" in line:
                    trip_seen = True
                if "exp_b] done" in line:
                    flood_done = True
        time.sleep(0.05)


def dual_poll(i):
    ok1, e1_ = http_get("/status")
    ok2, e2_ = http_get("/system_status")
    event("dual", i=i, status_ok=ok1, sys_ok=ok2,
          err=None if (ok1 and ok2) else [None if ok1 else e1_, None if ok2 else e2_])


def main():
    global trip_seen, flood_done
    ser = serial.Serial(PORT, 115200, timeout=1)
    drain(ser, 5)

    ok, _ = http_get("/reboot")
    event("reboot", ok=ok)
    drain(ser, 17)  # EARLY-BOOT settle, exactly the trip-#2 recipe

    ser.write(b"exp_b 200 30 1\n")
    event("flood-start")
    t0 = time.time()
    i = 0
    while time.time() - t0 < 100:
        if not trip_seen:
            dual_poll(i)
            i += 1
        drain(ser, 1)
        if trip_seen:
            break
        if flood_done and time.time() - t0 > 40 + 60:
            break
    event("poll-end", n=i, trip=trip_seen)
    drain(ser, 45)  # capture abort + reboot tail if tripped

    try:
        r = urllib.request.urlopen(BOARD + "/ring_status", timeout=4)
        event("ring_status", body=r.read().decode("utf-8", "replace")[:600])
    except Exception as e:
        event("ring_status", err=repr(e))
    ser.close()
    with open(SERIAL_OUT, "wb") as f:
        f.write(b"".join(serial_buf))
    print("done: %d bytes serial" % sum(len(c) for c in serial_buf))


if __name__ == "__main__":
    main()
