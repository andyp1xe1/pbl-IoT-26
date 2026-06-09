export enum ClickAction {
  None = 0,
  Left = 1,
  Right = 2,
  Middle = 3,
  ScrollUp = 4,
  ScrollDown = 5,
  Clutch = 6,
  ScrollMode = 7,
}

export const CLICK_ACTION_LABELS: Record<ClickAction, string> = {
  [ClickAction.None]: "None",
  [ClickAction.Left]: "Left click",
  [ClickAction.Right]: "Right click",
  [ClickAction.Middle]: "Middle click",
  [ClickAction.ScrollUp]: "Scroll up",
  [ClickAction.ScrollDown]: "Scroll down",
  [ClickAction.Clutch]: "Clutch (hold)",
  [ClickAction.ScrollMode]: "Scroll mode (hold)",
};

/** Actions that aren't allowed in the alt-table (modal/hold gestures). */
export const ALT_FORBIDDEN: ReadonlySet<ClickAction> = new Set([
  ClickAction.Clutch,
  ClickAction.ScrollMode,
]);

/** Sentinel value (matches AG_NO_MODIFIER on the firmware side). */
export const NO_MODIFIER = 0xff;

export const PAD_NAMES: readonly string[] = ["Thumb", "Index", "Middle", "Ring"];

/** Wire format v2 — see docs/plans/11-companion-app-firmware-extensions.md §11.2. */
export interface AgConfig {
  version: number;
  flags: number;
  sensXMilli: number;
  sensYMilli: number;
  deadzoneMrad: number;
  madgwickBetaMilli: number;
  debounceMs: number;
  touchThreshold: [number, number, number, number];
  clickAction: [ClickAction, ClickAction, ClickAction, ClickAction];
  modifierPad: number; // 0..3 or NO_MODIFIER
  clickActionAlt: [ClickAction, ClickAction, ClickAction];
}

export interface AgTelemetry {
  version: number;
  seq: number;
  accel: [number, number, number];
  gyro: [number, number, number];
  touch: [number, number, number, number];
  batteryPct: number;
  flags: number;
}

export interface AgStatus {
  lastOpcode: number;
  state: StatusState;
  progress: number;
  reserved: number;
}

export enum StatusState {
  Idle = 0,
  Running = 1,
  Success = 2,
  Fail = 3,
}

export enum Command {
  CalibrateImu = 0x01,
  RecalibrateTouch = 0x02,
  SaveConfig = 0x03,
  FactoryReset = 0x04,
  Sleep = 0x05,
  Wake = 0x06,
  /** Telemetry rate hints — companion sends one based on which tab is
   *  active so firmware only spends BLE airtime on what's actually
   *  being watched. */
  TelemetryIdle = 0x07,    // ~2 Hz   — Home / About
  TelemetryNormal = 0x08,  // ~10 Hz  — Tune (touch bars)
  TelemetryFast = 0x09,    // ~30 Hz  — Calibrate (live IMU)
}

export const CONFIG_FLAG_DIRTY = 0x01;
export const TELEMETRY_FLAG_HID_CONNECTED = 0x01;
export const TELEMETRY_FLAG_CALIBRATING = 0x02;
export const TELEMETRY_FLAG_SLEEPING = 0x04;

export interface DeviceInfo {
  manufacturer: string;
  model: string;
  firmware: string;
}

export type ConnectionStatus = "disconnected" | "connecting" | "connected";

/** Lightweight identity for a previously-granted device (Web Bluetooth
 *  `getDevices()`). The UI uses this to surface a one-click "Reconnect"
 *  affordance instead of forcing the user back through the chooser. */
export interface KnownDevice {
  id: string;
  name: string;
}

export interface IAirGloveClient {
  readonly kind: "real" | "mock";
  /** The device currently held in hand — populated the moment a pick or
   *  reconnect succeeds, cleared on disconnect. Lets callers paint the
   *  device's advertised name immediately, without waiting for a round
   *  trip to read the GATT Device Info characteristics. */
  readonly currentDevice: KnownDevice | null;
  connect(): Promise<void>;
  /** Connect to a previously-permitted device without showing the chooser.
   *  Pass an id from listKnownDevices(); if omitted, picks the first match. */
  reconnect(id?: string): Promise<void>;
  /** Devices this origin has already been granted permission to. Empty if
   *  the browser doesn't implement getDevices() (Safari/Firefox today). */
  listKnownDevices(): Promise<KnownDevice[]>;
  /** Revoke the origin's permission for the given device. After this, the
   *  device disappears from listKnownDevices() and the user must go through
   *  the chooser again to reconnect. Forgetting an active link also
   *  disconnects it. Pass an id to target a specific device, or omit to
   *  forget the first remembered one. */
  forget(id?: string): Promise<void>;
  disconnect(): Promise<void>;
  readConfig(): Promise<AgConfig>;
  writeConfig(cfg: AgConfig): Promise<void>;
  sendCommand(opcode: Command): Promise<void>;
  /** Force-read the telemetry characteristic over GATT, bypassing the
   *  notification stream. Use when local state and firmware state may
   *  have drifted (e.g. after a sleep/wake command) and we want the
   *  ground truth without waiting for the next 5 Hz notify. */
  readTelemetry(): Promise<AgTelemetry>;
  readDeviceInfo(): Promise<DeviceInfo>;
  readBattery(): Promise<number>;
  onConnectionChange(cb: (s: ConnectionStatus) => void): void;
  onTelemetry(cb: (t: AgTelemetry) => void): void;
  onStatus(cb: (s: AgStatus) => void): void;
}

export const CONFIG_VERSION_V2 = 2;

export function defaultConfig(): AgConfig {
  return {
    version: CONFIG_VERSION_V2,
    flags: 0,
    sensXMilli: 1000,
    sensYMilli: 1000,
    deadzoneMrad: 4,
    madgwickBetaMilli: 50,
    debounceMs: 30,
    touchThreshold: [600, 600, 600, 600],
    clickAction: [
      ClickAction.None,
      ClickAction.Left,
      ClickAction.Right,
      ClickAction.ScrollMode,
    ],
    modifierPad: NO_MODIFIER,
    clickActionAlt: [ClickAction.None, ClickAction.None, ClickAction.None],
  };
}
