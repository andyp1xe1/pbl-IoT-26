import { AgConfig, AgStatus, AgTelemetry, StatusState } from "./types";

export const CONFIG_SIZE = 10;
export const TELEMETRY_SIZE = 24;
export const STATUS_SIZE = 4;

const LE = true;

export function encodeConfig(cfg: AgConfig): Uint8Array<ArrayBuffer> {
  const bytes = new Uint8Array(CONFIG_SIZE);
  const dv = new DataView(bytes.buffer);
  dv.setUint8(0, cfg.version);
  dv.setUint8(1, cfg.flags);
  dv.setUint16(2, cfg.sensXMilli, LE);
  dv.setUint16(4, cfg.sensYMilli, LE);
  dv.setUint16(6, cfg.deadzoneMrad, LE);
  dv.setUint8(8, cfg.clickMap);
  dv.setUint8(9, cfg.reserved);
  return bytes;
}

export function decodeConfig(dv: DataView): AgConfig {
  return {
    version: dv.getUint8(0),
    flags: dv.getUint8(1),
    sensXMilli: dv.getUint16(2, LE),
    sensYMilli: dv.getUint16(4, LE),
    deadzoneMrad: dv.getUint16(6, LE),
    clickMap: dv.getUint8(8),
    reserved: dv.getUint8(9),
  };
}

export function decodeTelemetry(dv: DataView): AgTelemetry {
  return {
    version: dv.getUint8(0),
    seq: dv.getUint8(1),
    accel: [dv.getInt16(2, LE), dv.getInt16(4, LE), dv.getInt16(6, LE)],
    gyro: [dv.getInt16(8, LE), dv.getInt16(10, LE), dv.getInt16(12, LE)],
    touch: [
      dv.getUint16(14, LE),
      dv.getUint16(16, LE),
      dv.getUint16(18, LE),
      dv.getUint16(20, LE),
    ],
    batteryPct: dv.getUint8(22),
    flags: dv.getUint8(23),
  };
}

export function decodeStatus(dv: DataView): AgStatus {
  return {
    lastOpcode: dv.getUint8(0),
    state: dv.getUint8(1) as StatusState,
    progress: dv.getUint8(2),
    reserved: dv.getUint8(3),
  };
}
