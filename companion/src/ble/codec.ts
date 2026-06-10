import {
  AgConfig,
  AgStatus,
  AgTelemetry,
  ClickAction,
  CONFIG_VERSION_V4,
  MIX_AXIS_COUNT,
  MixVector,
  NO_MODIFIER,
  StatusState,
} from "./types";

/** Wire format v4. v3 head (offsets [0..28]) unchanged; new tail:
 *    [29..40]  mixX[6]               6 × i16 little-endian
 *    [41..52]  mixY[6]               6 × i16 little-endian
 *    [53..54]  wristRollCompMilli    u16
 */
export const CONFIG_SIZE = 55;
export const TELEMETRY_SIZE = 24;
export const STATUS_SIZE = 4;

const LE = true;

function clampU16(v: number, lo: number, hi: number): number {
  if (!Number.isFinite(v)) return lo;
  const r = Math.round(v);
  return r < lo ? lo : r > hi ? hi : r;
}

function clampI16(v: number, lo: number, hi: number): number {
  if (!Number.isFinite(v)) return 0;
  const r = Math.round(v);
  return r < lo ? lo : r > hi ? hi : r;
}

function clampAction(a: number): ClickAction {
  return a >= ClickAction.None && a <= ClickAction.ScrollMode ? a : ClickAction.None;
}

export function encodeConfig(cfg: AgConfig): Uint8Array<ArrayBuffer> {
  const bytes = new Uint8Array(CONFIG_SIZE);
  const dv = new DataView(bytes.buffer);
  dv.setUint8(0, CONFIG_VERSION_V4);
  dv.setUint8(1, cfg.flags & 0xff);
  dv.setUint16(2, clampU16(cfg.sensXMilli, 100, 5000), LE);
  dv.setUint16(4, clampU16(cfg.sensYMilli, 100, 5000), LE);
  dv.setUint16(6, clampU16(cfg.deadzoneMrad, 0, 1000), LE);
  dv.setUint16(8, clampU16(cfg.madgwickBetaMilli, 0, 1000), LE);
  dv.setUint16(10, clampU16(cfg.debounceMs, 5, 200), LE);
  for (let i = 0; i < 4; i++) {
    dv.setUint16(12 + i * 2, clampU16(cfg.touchThreshold[i], 1, 4095), LE);
  }
  for (let i = 0; i < 4; i++) {
    dv.setUint8(20 + i, clampAction(cfg.clickAction[i]));
  }
  const mod = cfg.modifierPad;
  dv.setUint8(24, mod === NO_MODIFIER || (mod >= 0 && mod < 4) ? mod : NO_MODIFIER);
  for (let i = 0; i < 3; i++) {
    dv.setUint8(25 + i, clampAction(cfg.clickActionAlt[i]));
  }
  dv.setUint8(28, cfg.madgwickEnabled ? 1 : 0);
  for (let i = 0; i < MIX_AXIS_COUNT; i++) {
    dv.setInt16(29 + i * 2, clampI16(cfg.mixX[i], -2000, 2000), LE);
    dv.setInt16(41 + i * 2, clampI16(cfg.mixY[i], -2000, 2000), LE);
  }
  dv.setUint16(53, clampU16(cfg.wristRollCompMilli, 0, 1000), LE);
  return bytes;
}

function readMixVector(dv: DataView, baseOffset: number): MixVector {
  const out: number[] = new Array(MIX_AXIS_COUNT);
  for (let i = 0; i < MIX_AXIS_COUNT; i++) {
    out[i] = dv.getInt16(baseOffset + i * 2, LE);
  }
  return out as MixVector;
}

export function decodeConfig(dv: DataView): AgConfig {
  const version = dv.getUint8(0);
  return {
    version,
    flags: dv.getUint8(1),
    sensXMilli: dv.getUint16(2, LE),
    sensYMilli: dv.getUint16(4, LE),
    deadzoneMrad: dv.getUint16(6, LE),
    madgwickBetaMilli: dv.getUint16(8, LE),
    debounceMs: dv.getUint16(10, LE),
    touchThreshold: [
      dv.getUint16(12, LE),
      dv.getUint16(14, LE),
      dv.getUint16(16, LE),
      dv.getUint16(18, LE),
    ],
    clickAction: [
      clampAction(dv.getUint8(20)),
      clampAction(dv.getUint8(21)),
      clampAction(dv.getUint8(22)),
      clampAction(dv.getUint8(23)),
    ],
    modifierPad: dv.getUint8(24),
    clickActionAlt: [
      clampAction(dv.getUint8(25)),
      clampAction(dv.getUint8(26)),
      clampAction(dv.getUint8(27)),
    ],
    madgwickEnabled: dv.getUint8(28) !== 0,
    mixX: readMixVector(dv, 29),
    mixY: readMixVector(dv, 41),
    wristRollCompMilli: dv.getUint16(53, LE),
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
