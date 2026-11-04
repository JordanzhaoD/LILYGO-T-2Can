import json
import time
import urllib.request

import serial

PORT = "/dev/cu.usbmodem5CF71553661"
BOARD = "http://192.168.1.146"
SERIAL_OUT = "/tmp/p4_expD_serial.log"
EVENTS_OUT = "/tmp/p4_expD_events.jsonl"

serial_buf = []
trip_seen = False


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
    """Read serial continuously; flag trip markers live."""
    global trip_seen
    end = time.time() + seconds
    while time.time() < end:
        chunk = ser.read(4096)
        if chunk:
            serial_buf.append(chunk)
            text = chunk.decode("utf-8", "replace")
            for line in text.splitlines():
                if ("TCB TRIP" in line or "abort()" in line
                        or "Guru Meditation" in line or "Backtrace:" in line
                        or "ELF file SHA256" in line or "exp_b] done" in line):
                    event("serial-line", line=line.strip()[:200])
                    if "TCB TRIP" in line:
                        trip_seen = True
        time.sleep(0.05)


def arm(ser, name, path):
    global trip_seen
    ok, info = http_get("/reboot")
    event("reboot", arm=name, ok=ok)
    drain(ser, 90)  # blind settle, zero HTTP

    event("flood-start", arm=name)
    ser.write(b"exp_b 200 10 1\n")
    drain(ser, 120)  # 10 s flood + margin, capture [exp_b] done
    event("flood-end", arm=name)

    time.sleep(10)  # post-flood gap
    ok, info = http_get(path)
    event("single-shot", arm=name, path=path, ok=ok, info=None if ok else info)
    drain(ser, 60)
    event("after-single", arm=name, trip=trip_seen)

    if not trip_seen:
        for i in range(30):
            ok, info = http_get(path)
            if i % 10 == 0 or not ok:
                event("burst", arm=name, i=i, ok=ok)
            time.sleep(1.0)
        drain(ser, 60)
    event("arm-end", arm=name, trip=trip_seen)
    trip_seen = False


def main():
    global trip_seen
    ser = serial.Serial(PORT, 115200, timeout=1)
    drain(ser, 5)

    trip_seen = False
    arm(ser, "D1-system_status", "/system_status")
    trip_seen = False
    arm(ser, "D2-status", "/status")

    ok, info = http_get("/ring_status")
    event("ring_status", ok=ok)
    ser.close()
    with open(SERIAL_OUT, "wb") as f:
        f.write(b"".join(serial_buf))
    print("done: %d bytes serial" % sum(len(c) for c in serial_buf))


if __name__ == "__main__":
    main()
