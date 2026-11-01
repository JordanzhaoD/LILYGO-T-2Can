#!/usr/bin/env python3
"""Decode a blackbox session bin download into the board's exact CSV.

Input: a `sess_N.bin` downloaded from /session_download?fmt=bin — the raw
18-byte RecRingFrame stream the recorder locked (tsUs u32 LE, id u32 LE,
dlc u8, flags u8, data[8]; flags bits 0-1 = bus 1=A/2=B/3=C, bits 2-3 =
dir 0=R/1=T/2=E).

Output: byte-identical to /session_download?fmt=csv for the same session —
same frozen header, same row format — so a bin/csv pair from the field can
be diffed to prove the streaming filter and this decoder agree.

Usage: decode_blackbox.py IN.bin [-o OUT.csv]   (default: stdout)

A file whose length is not a whole multiple of 18 is refused: a torn tail
must never decode as if it were a frame.
"""

import argparse
import struct
import sys
from pathlib import Path

FRAME = struct.Struct("<IIBB8s")  # tsUs, id, dlc, flags, data[8]
assert FRAME.size == 18
CHUNK = 4096
HEADER = "ts_us,bus,dir,id,dlc,b0,b1,b2,b3,b4,b5,b6,b7\n"
DIR_CHARS = {0: "R", 1: "T", 2: "E"}


def decode(data: bytes) -> str:
    """Whole-frame decode of a bin payload; ValueError on a torn tail."""
    if len(data) % FRAME.size != 0:
        raise ValueError(
            f"input is {len(data)} bytes — not a whole multiple of "
            f"{FRAME.size}; torn or foreign file, refusing to decode")
    out = [HEADER]
    for ts_us, can_id, dlc, flags, payload in FRAME.iter_unpack(data):
        bus = flags & 0x03
        dir_char = DIR_CHARS.get((flags >> 2) & 0x03, "R")
        row = ",".join(str(v) for v in (ts_us, bus, dir_char, can_id, dlc,
                                        *payload))
        out.append(row + "\n")
    return "".join(out)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description="Decode a blackbox sess_N.bin download to board CSV.")
    ap.add_argument("input", help="session .bin downloaded from the board")
    ap.add_argument("-o", "--output",
                    help="output .csv path (default: stdout)")
    args = ap.parse_args(argv)

    path = Path(args.input)
    try:
        data = path.read_bytes()
    except OSError as exc:
        print(f"error: cannot read {path}: {exc}", file=sys.stderr)
        return 1

    # Read path stays chunked (the buffer carries frames across chunk edges)
    # even though read_bytes already gave us the whole file: keep one code
    # path exercised by tests that split frames across CHUNK boundaries.
    buf = bytearray()
    rows: list[str] = []
    try:
        for off in range(0, len(data), CHUNK):
            buf += data[off:off + CHUNK]
            whole = len(buf) - len(buf) % FRAME.size
            if whole:
                rows.append(decode(bytes(buf[:whole])).split(HEADER, 1)[1])
                del buf[:whole]
        if buf:
            raise ValueError(
                f"{len(buf)} trailing byte(s) — not a whole multiple of "
                f"{FRAME.size}")
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    text = HEADER + "".join(rows)
    if args.output:
        Path(args.output).write_text(text, encoding="ascii")
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
