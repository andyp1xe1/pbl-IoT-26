import { describe, expect, it } from "vitest";
import {
  CONFIG_SIZE,
  decodeConfig,
  decodeStatus,
  decodeTelemetry,
  encodeConfig,
} from "./codec";
import {
  AgConfig,
  ClickAction,
  CONFIG_VERSION_V4,
  defaultConfig,
  NO_MODIFIER,
  StatusState,
} from "./types";

describe("config codec (v4)", () => {
  it("round-trips a populated config", () => {
    const cfg: AgConfig = {
      version: CONFIG_VERSION_V4,
      flags: 0,
      sensXMilli: 1400,
      sensYMilli: 1200,
      deadzoneMrad: 80,
      madgwickBetaMilli: 75,
      debounceMs: 25,
      touchThreshold: [500, 600, 700, 800],
      clickAction: [
        ClickAction.None,
        ClickAction.Left,
        ClickAction.Right,
        ClickAction.ScrollMode,
      ],
      modifierPad: 0,
      clickActionAlt: [ClickAction.None, ClickAction.ScrollDown, ClickAction.Middle],
      madgwickEnabled: true,
      mixX: [0, 0, 0, 0, 1000, -1000],
      mixY: [0, 0, 0, 0, -1700, 0],
      wristRollCompMilli: 750,
    };
    const dv = new DataView(encodeConfig(cfg).buffer);
    expect(decodeConfig(dv)).toEqual(cfg);
  });

  it("round-trips arbitrary mix-matrix weights (incl. negatives)", () => {
    const cfg = defaultConfig();
    cfg.mixX = [+500, -500, +1500, -1500, +2000, -2000];
    cfg.mixY = [-1, +1, +0, +777, -777, +1999];
    cfg.madgwickEnabled = false;
    cfg.wristRollCompMilli = 0;
    const dv = new DataView(encodeConfig(cfg).buffer);
    const back = decodeConfig(dv);
    expect(back.mixX).toEqual(cfg.mixX);
    expect(back.mixY).toEqual(cfg.mixY);
    expect(back.madgwickEnabled).toBe(false);
    expect(back.wristRollCompMilli).toBe(0);
  });

  it("clamps out-of-range mix weights to ±2000", () => {
    const cfg = defaultConfig();
    cfg.mixX = [99999, -99999, 0, 0, 0, 0];
    const dv = new DataView(encodeConfig(cfg).buffer);
    const back = decodeConfig(dv);
    expect(back.mixX[0]).toBe(2000);
    expect(back.mixX[1]).toBe(-2000);
  });

  it("clamps wrist-roll compensation to [0, 1000]", () => {
    const cfg = defaultConfig();
    cfg.wristRollCompMilli = 99999;
    const dv = new DataView(encodeConfig(cfg).buffer);
    expect(decodeConfig(dv).wristRollCompMilli).toBe(1000);
  });

  it("emits the v4 version byte", () => {
    const bytes = encodeConfig(defaultConfig());
    expect(bytes[0]).toBe(CONFIG_VERSION_V4);
    expect(bytes.length).toBe(CONFIG_SIZE);
  });

  it("clamps modifier_pad to NO_MODIFIER if out of range", () => {
    const cfg = defaultConfig();
    cfg.modifierPad = 99;
    const dv = new DataView(encodeConfig(cfg).buffer);
    expect(decodeConfig(dv).modifierPad).toBe(NO_MODIFIER);
  });
});

describe("telemetry codec", () => {
  it("decodes a fixed little-endian fixture", () => {
    const bytes = new Uint8Array([
      0x01, 0x2a, // version, seq
      0x10, 0x00, 0xf6, 0xff, 0xe8, 0x03, // accel: 16, -10, 1000
      0x05, 0x00, 0xfb, 0xff, 0x00, 0x00, // gyro: 5, -5, 0
      0x90, 0x01, 0xa9, 0x02, 0xd0, 0x00, 0xab, 0x00, // touch
      0x57, 0x03, // battery 87, flags 3
    ]);
    const t = decodeTelemetry(new DataView(bytes.buffer));
    expect(t.seq).toBe(0x2a);
    expect(t.accel).toEqual([16, -10, 1000]);
    expect(t.gyro).toEqual([5, -5, 0]);
    expect(t.touch).toEqual([0x0190, 0x02a9, 0x00d0, 0x00ab]);
    expect(t.batteryPct).toBe(87);
    expect(t.flags).toBe(3);
  });
});

describe("status codec", () => {
  it("decodes", () => {
    const dv = new DataView(new Uint8Array([0x01, 0x02, 0x64, 0x00]).buffer);
    const s = decodeStatus(dv);
    expect(s.lastOpcode).toBe(1);
    expect(s.state).toBe(StatusState.Success);
    expect(s.progress).toBe(100);
  });
});
