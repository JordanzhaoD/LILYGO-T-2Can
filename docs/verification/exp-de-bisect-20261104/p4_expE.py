import json
import time
import urllib.request

import serial

PORT = "/dev/cu.usbmodem5CF71553661"
BOARD = "http://192.168.1.146"
SERIAL_OUT = "/tmp/p4_expE_serial.log"
EVENTS_OUT = "/tmp/p4_expE_events.jsonl"

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
    """Read serial continuously; flag trip + flood-done markers live."""
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


def reboot_settle(ser, arm):
    global flood_done
    ok, info = http_get("/reboot")
    event("reboot", arm=arm, ok=ok)
    flood_done = False
    drain(ser, 90)


def e1(ser):
    """Concurrency arm: /system_status probes DURING an active 30 s flood."""
    global trip_seen
    reboot_settle(ser, "E1")
    ser.write(b"exp_b 200 30 1\n")
    event("flood-start", arm="E1")
    # probe every 6 s until flood done seen (or 90 s cap)
    t0 = time.time()
    n = 0
    while not flood_done and time.time() - t0 < 90:
        ok, info = http_get("/system_status")
        event("probe", arm="E1", n=n, ok=ok, err=None if ok else info)
        n += 1
        drain(ser, 6)
    # post-flood window (panic #2 tripped 2 s after flood end)
    for i in range(10):
        if trip_seen:
            break
        ok, info = http_get("/system_status")
        event("probe-post", arm="E1", n=i, ok=ok, err=None if ok else info)
        drain(ser, 6)
    event("arm-end", arm="E1", trip=trip_seen)


def e2(ser):
    """Delay arm, full panic-#1 boot history: flood 10 s -> 18-shot rapid
    burst -> 7 min silent idle -> single shot."""
    global trip_seen
    reboot_settle(ser, "E2")
    ser.write(b"exp_b 200 10 1\n")
    drain(ser, 130)  # flood + margin, wait for [exp_b] done
    event("flood-end", arm="E2", done=flood_done)

    for i in range(18):  # rapid burst, no sleep (replicates the probe script)
        ok, info = http_get("/system_status")
        if i < 3 or not ok:
            event("burst", arm="E2", i=i, ok=ok, err=None if ok else info)
    event("burst-end", arm="E2", trip=trip_seen)

    drain(ser, 420)  # 7 min silent idle, zero HTTP
    event("idle-end", arm="E2", trip=trip_seen)

    ok, info = http_get("/system_status")
    event("single-shot", arm="E2", ok=ok, err=None if ok else info)
    drain(ser, 60)
    event("arm-end", arm="E2", trip=trip_seen)


def main():
    global trip_seen
    ser = serial.Serial(PORT, 115200, timeout=1)
    drain(ser, 5)
    trip_seen = False
    e1(ser)
    trip_seen = False
    e2(ser)
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
