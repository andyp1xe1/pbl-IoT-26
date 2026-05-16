#!/usr/bin/env python3
"""Glove → mouse daemon. Reads MPU CSV from serial, injects relative mouse
motion via /dev/uinput."""
import argparse, sys, collections
import serial
from evdev import UInput, ecodes as e

GYRO_LSB_PER_DPS = 32.8   # ±1000 dps in the sketch
GAIN     = 0.06           # px per (°/s) at low speed
EXPO     = 1.4            # acceleration exponent
DEADBAND = 1.5            # °/s
BIAS_WIN = 200
BIAS_TH  = 8.0            # °/s

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--debug", action="store_true")
    a = ap.parse_args()

    ui = UInput({e.EV_REL: [e.REL_X, e.REL_Y],
                 e.EV_KEY: [e.BTN_LEFT, e.BTN_RIGHT]},
                name="glove-mouse")

    bias = [0.0, 0.0, 0.0]
    recent = collections.deque(maxlen=BIAS_WIN)
    accum_x = accum_y = 0.0

    def curve(v):
        if v == 0: return 0
        s = 1 if v > 0 else -1
        m = abs(v) - DEADBAND
        return 0 if m <= 0 else s * GAIN * (m ** EXPO)

    with serial.Serial(a.port, a.baud, timeout=1) as s:
        for raw in s:
            line = raw.decode("ascii", errors="ignore").strip()
            if not line or line.startswith("#"): continue
            parts = line.split(",")
            if len(parts) < 7: continue
            try:
                gx = int(parts[4]) / GYRO_LSB_PER_DPS
                gy = int(parts[5]) / GYRO_LSB_PER_DPS
                gz = int(parts[6]) / GYRO_LSB_PER_DPS
            except ValueError:
                continue

            cx, cy, cz = gx-bias[0], gy-bias[1], gz-bias[2]
            recent.append((gx, gy, gz))
            if max(abs(cx), abs(cy), abs(cz)) < BIAS_TH and len(recent) == recent.maxlen:
                bx = sum(p[0] for p in recent) / len(recent)
                by = sum(p[1] for p in recent) / len(recent)
                bz = sum(p[2] for p in recent) / len(recent)
                bias[0] = 0.95*bias[0] + 0.05*bx
                bias[1] = 0.95*bias[1] + 0.05*by
                bias[2] = 0.95*bias[2] + 0.05*bz

            ax_in = cy - cz
            ay_in = cx
            accum_x += curve(ax_in)
            accum_y += curve(ay_in)
            ix = int(accum_x); accum_x -= ix
            iy = int(accum_y); accum_y -= iy
            if ix: ui.write(e.EV_REL, e.REL_X, ix)
            if iy: ui.write(e.EV_REL, e.REL_Y, iy)
            if ix or iy: ui.syn()

            if a.debug:
                print(f"({cx:+6.1f},{cy:+6.1f},{cz:+6.1f})  px({ix:+3d},{iy:+3d})",
                      file=sys.stderr)

if __name__ == "__main__":
    try: main()
    except KeyboardInterrupt: pass
    except PermissionError as ex:
        print(f"/dev/uinput: {ex}\n"
              "NixOS: hardware.uinput.enable = true; add user to 'input','uinput'.",
              file=sys.stderr); sys.exit(1)
