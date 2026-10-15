#!/usr/bin/env python3
"""
Trigger and capture a 10-second recording from the Daisy Seed over USB CDC.

Usage:
    python3 tools/grab_recording.py [output.wav] [--port /dev/tty.usbmodemXXXX]

The Daisy must be running the noise-capture firmware (src/main.cpp). On the Mac
the Daisy enumerates as /dev/tty.usbmodem* — by default this script picks the
first matching port automatically.
"""

import argparse
import glob
import os
import sys
import time

try:
    import serial  # type: ignore
except ImportError:
    sys.exit("pyserial not installed. Run: pip3 install pyserial")


SAMPLE_RATE = 48000
RECORD_SECS = 10
EXPECTED_BYTES = 44 + SAMPLE_RATE * RECORD_SECS * 2  # WAV header + int16 mono


def find_daisy_port() -> str:
    candidates = sorted(glob.glob("/dev/tty.usbmodem*"))
    if not candidates:
        sys.exit("No /dev/tty.usbmodem* found. Is the Daisy plugged in and running this firmware?")
    if len(candidates) > 1:
        print(f"Multiple candidates found, using {candidates[0]}", file=sys.stderr)
        print("(use --port to override)", file=sys.stderr)
    return candidates[0]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("output", nargs="?", default="recording.wav")
    ap.add_argument("--port", default=None)
    args = ap.parse_args()

    port = args.port or find_daisy_port()
    print(f"Opening {port} ...")

    with serial.Serial(port, baudrate=115200, timeout=2) as s:
        time.sleep(0.3)         # let port settle
        s.reset_input_buffer()

        print("Sending 'g' to trigger recording. Play your guitar now (or stay silent for noise floor).")
        s.write(b"g")
        s.flush()

        # Daisy records 10 s, then dumps. Allow ~25 s total budget.
        deadline = time.time() + 30.0
        buf = bytearray()
        s.timeout = 1.0
        while len(buf) < EXPECTED_BYTES and time.time() < deadline:
            chunk = s.read(min(8192, EXPECTED_BYTES - len(buf)))
            if chunk:
                buf.extend(chunk)
                pct = 100 * len(buf) // EXPECTED_BYTES
                print(f"\r  received {len(buf)}/{EXPECTED_BYTES} ({pct}%)", end="", flush=True)
        print()

    if len(buf) < EXPECTED_BYTES:
        print(f"WARNING: short read ({len(buf)} bytes). File may be truncated.", file=sys.stderr)
        if len(buf) < 44:
            sys.exit("No WAV header received. Is the firmware running?")

    out_path = os.path.abspath(args.output)
    with open(out_path, "wb") as f:
        f.write(buf)

    duration = (len(buf) - 44) / 2 / SAMPLE_RATE
    print(f"Wrote {out_path} ({len(buf)} bytes, ~{duration:.2f} s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
