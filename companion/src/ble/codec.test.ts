import { describe, expect, it } from "vitest";
import {
  decodeConfig,
  decodeStatus,
  decodeTelemetry,
  encodeConfig,
} from "./codec";
import { AgConfig, StatusState } from "./types";

describe("config codec", () => {
  it("round-trips", () => {
    const cfg: AgConfig = {
      version: 1,
      flags: 1,
      sensXMilli: 1400,
      sensYMilli: 1200,
      deadzoneMrad: 80,
      clickMap: 1,
      reserved: 0,
    };
    const dv = new DataView(encodeConfig(cfg).buffer);
    expect(decodeConfig(dv)).toEqual(cfg);
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
