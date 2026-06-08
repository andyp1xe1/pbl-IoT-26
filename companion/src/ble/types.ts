export interface AgConfig {
  version: number;
  flags: number;
  sensXMilli: number;
  sensYMilli: number;
  deadzoneMrad: number;
  clickMap: number;
  reserved: number;
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
}

export const CONFIG_FLAG_DIRTY = 0x01;
export const TELEMETRY_FLAG_HID_CONNECTED = 0x01;
export const TELEMETRY_FLAG_CALIBRATING = 0x02;

export interface DeviceInfo {
  manufacturer: string;
  model: string;
  firmware: string;
}

export type ConnectionStatus = "disconnected" | "connecting" | "connected";

export interface IAirGloveClient {
  readonly kind: "real" | "mock";
  connect(): Promise<void>;
  disconnect(): Promise<void>;
  readConfig(): Promise<AgConfig>;
  writeConfig(cfg: AgConfig): Promise<void>;
  sendCommand(opcode: Command): Promise<void>;
  readDeviceInfo(): Promise<DeviceInfo>;
  readBattery(): Promise<number>;
  onConnectionChange(cb: (s: ConnectionStatus) => void): void;
  onTelemetry(cb: (t: AgTelemetry) => void): void;
  onStatus(cb: (s: AgStatus) => void): void;
}

export function defaultConfig(): AgConfig {
  return {
    version: 1,
    flags: 0,
    sensXMilli: 1400,
    sensYMilli: 1200,
    deadzoneMrad: 80,
    clickMap: 0,
    reserved: 0,
  };
}
