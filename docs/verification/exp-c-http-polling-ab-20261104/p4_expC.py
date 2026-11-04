import json
import time
import urllib.request

import serial

PORT = "/dev/cu.usbmodem5CF71553661"
BOARD = "http://192.168.1.146"
SERIAL_OUT = "/tmp/p4_expC_serial.log"
EVENTS_OUT = "/tmp/p4_expC_events.jsonl"

ARM_SECONDS = 600      # 10 min per arm
SETTLE_SECONDS = 90    # blind settle after each reboot
POLL_HZ = 1            # 1 Hz, matches round-8 browser tab

serial_buf = []
events = []


def event(kind, **kw):
    rec = {"t": round(time.time(), 1), "kind": kind, **kw}
    events.append(rec)
    with open(EVENTS_OUT, "a") as f:
        f.write(json.dumps(rec) + "\n")


def http_get(path, timeout=4):
    try:
        r = urllib.request.urlopen(BOARD + path, timeout=timeout)
        body = r.read()
        return True, body.decode("utf-8", "replace")
    except Exception as e:
        return False, repr(e)


def drain(ser, seconds, poll_path=None):
    """Read serial continuously for `seconds`; optionally poll path at POLL_HZ."""
    end = time.time() + seconds
    next_poll = time.time()
    last_poll_bad = None
    while time.time() < end:
        chunk = ser.read(4096)
        if chunk:
            serial_buf.append(chunk)
            # live tail marker: log abort/panic lines to events
            text = chunk.decode("utf-8", "replace")
            for line in text.splitlines():
                if ("abort()" in line or "Guru Meditation" in line
                        or "dashHg" in line or "Backtrace:" in line
                        or "ELF file SHA256" in line):
                    event("serial-line", line=line.strip()[:200])
        if poll_path and time.time() >= next_poll:
            ok, info = http_get(poll_path)
            if ok != last_poll_bad or not ok:
                # log transitions and every failure; successes logged sparsely
                event("poll", ok=ok, info=info if not ok else None)
            last_poll_bad = ok
            next_poll += 1.0 / POLL_HZ
            if next_poll < time.time() - 5:
                next_poll = time.time()
        time.sleep(0.05)


def reboot_separator(ser):
    ok, info = http_get("/reboot")
    event("reboot", ok=ok, info=None if ok else info)
    drain(ser, SETTLE_SECONDS)  # blind settle, zero HTTP during it


def main():
    ser = serial.Serial(PORT, 115200, timeout=1)
    drain(ser, 5)  # let port settle; may or may not reset the board

    reboot_separator(ser)   # fresh boot for arm A
    event("arm-start", arm="A-silent")
    drain(ser, ARM_SECONDS, poll_path=None)
    event("arm-end", arm="A-silent")

    reboot_separator(ser)   # fresh boot for arm B
    event("arm-start", arm="B-status-1hz")
    drain(ser, ARM_SECONDS, poll_path="/status")
    event("arm-end", arm="B-status-1hz")

    reboot_separator(ser)   # fresh boot for arm C
    event("arm-start", arm="C-system_status-1hz")
    drain(ser, ARM_SECONDS, poll_path="/system_status")
    event("arm-end", arm="C-system_status-1hz")

    ok, info = http_get("/ring_status")
    event("ring_status", ok=ok, info=None if ok else info,
          body=info if ok else None)
    ser.close()
    with open(SERIAL_OUT, "wb") as f:
        f.write(b"".join(serial_buf))
    print("done: %d bytes serial, %d events" % (sum(len(c) for c in serial_buf), len(events)))


if __name__ == "__main__":
    main()
