#!/usr/bin/env python3
"""Stream serial -> stdout + CSV file. Ctrl-C to stop."""
import argparse, sys, time
import serial

ap = argparse.ArgumentParser()
ap.add_argument("--port", default="/dev/ttyUSB0")
ap.add_argument("--baud", type=int, default=115200)
ap.add_argument("--out", required=True)
args = ap.parse_args()

with serial.Serial(args.port, args.baud, timeout=1) as s, open(args.out, "w") as f:
    print(f"logging {args.port}@{args.baud} -> {args.out}", file=sys.stderr)
    t0 = time.time()
    n = 0
    try:
        while True:
            line = s.readline().decode("ascii", errors="replace").rstrip()
            if not line:
                continue
            f.write(line + "\n"); f.flush()
            print(line)
            n += 1
            if n % 100 == 0:
                print(f"# {n} lines, {n/(time.time()-t0):.1f} Hz", file=sys.stderr)
    except KeyboardInterrupt:
        print(f"\nstopped after {n} lines", file=sys.stderr)
