import { log } from "./log";
import {
  AgConfig,
  AgStatus,
  AgTelemetry,
  Command,
  ConnectionStatus,
  DeviceInfo,
  IAirGloveClient,
  KnownDevice,
  StatusState,
  TELEMETRY_FLAG_HID_CONNECTED,
  TELEMETRY_FLAG_SLEEPING,
  defaultConfig,
} from "./types";

/** Fake glove for UI work. Drives the same IAirGloveClient surface as the
 *  real BLE client so the React tree can't tell them apart — connect goes
 *  through a short handshake, telemetry streams a synthetic IMU + cycling
 *  touch values, and Sleep/Wake/rate hints behave like the firmware would.
 *  Activated by `VITE_DEMO=1` (see `npm run demo`). */

const DEMO_DEVICE: KnownDevice = { id: "demo", name: "AirGlove Demo" };

/** localStorage key for the "browser remembers this device" flag. Mirrors
 *  Web Bluetooth's getDevices() persistence so the demo's pair → reload →
 *  reconnect flow matches what a real chooser pairing produces. */
const PAIRED_KEY = "airglove.demo.paired";

function loadPaired(): boolean {
  try {
    return typeof localStorage !== "undefined" && localStorage.getItem(PAIRED_KEY) === "1";
  } catch {
    return false;
  }
}

function savePaired(v: boolean) {
  try {
    if (typeof localStorage === "undefined") return;
    if (v) localStorage.setItem(PAIRED_KEY, "1");
    else localStorage.removeItem(PAIRED_KEY);
  } catch {
    /* private mode / quota — silently ignore, demo just won't remember */
  }
}

const RATE_HZ: Record<"idle" | "normal" | "fast", number> = {
  idle: 2,
  normal: 15,
  fast: 30,
};

export class DemoAirGloveClient implements IAirGloveClient {
  readonly kind = "mock" as const;

  private connection: ConnectionStatus = "disconnected";
  private config: AgConfig = defaultConfig();
  private sleeping = false;
  private rate: "idle" | "normal" | "fast" = "idle";
  private timer: ReturnType<typeof setInterval> | null = null;
  private startedAt = 0;
  private seq = 0;
  /** Mirrors the browser's "device is remembered" state. False until the
   *  first connect(); set to true after a successful pair so subsequent
   *  loads land on the paired view (with a Reconnect affordance) instead
   *  of the initial Pair-device CTA. */
  private paired = loadPaired();

  private connectionCbs: ((s: ConnectionStatus) => void)[] = [];
  private telemetryCbs: ((t: AgTelemetry) => void)[] = [];
  private statusCbs: ((s: AgStatus) => void)[] = [];

  get currentDevice(): KnownDevice | null {
    return this.connection === "connected" ? DEMO_DEVICE : null;
  }

  onConnectionChange(cb: (s: ConnectionStatus) => void) {
    this.connectionCbs.push(cb);
  }
  onTelemetry(cb: (t: AgTelemetry) => void) {
    this.telemetryCbs.push(cb);
  }
  onStatus(cb: (s: AgStatus) => void) {
    this.statusCbs.push(cb);
  }

  async connect(): Promise<void> {
    log.info("demo: connect");
    this.emitConnection("connecting");
    await delay(600);
    this.paired = true;
    savePaired(true);
    this.startStream();
    this.emitConnection("connected");
  }

  async reconnect(_id?: string): Promise<void> {
    return this.connect();
  }

  async listKnownDevices(): Promise<KnownDevice[]> {
    return this.paired ? [DEMO_DEVICE] : [];
  }

  async forget(_id?: string): Promise<void> {
    this.paired = false;
    savePaired(false);
    await this.disconnect();
  }

  async disconnect(): Promise<void> {
    this.stopStream();
    this.emitConnection("disconnected");
  }

  async readConfig(): Promise<AgConfig> {
    return { ...this.config };
  }

  async writeConfig(cfg: AgConfig): Promise<void> {
    this.config = { ...cfg };
  }

  async sendCommand(opcode: Command): Promise<void> {
    log.info("demo: command", { opcode });
    switch (opcode) {
      case Command.Sleep:
        this.sleeping = true;
        break;
      case Command.Wake:
        this.sleeping = false;
        break;
      case Command.TelemetryIdle:
        this.setRate("idle");
        break;
      case Command.TelemetryNormal:
        this.setRate("normal");
        break;
      case Command.TelemetryFast:
        this.setRate("fast");
        break;
      case Command.CalibrateImu:
      case Command.RecalibrateTouch:
        // Brief Running → Success status pulse so the calibrate flow has
        // something to react to.
        this.emitStatus({ lastOpcode: opcode, state: StatusState.Running, progress: 0, reserved: 0 });
        setTimeout(() => {
          this.emitStatus({ lastOpcode: opcode, state: StatusState.Success, progress: 100, reserved: 0 });
        }, 800);
        break;
      default:
        break;
    }
  }

  async readTelemetry(): Promise<AgTelemetry> {
    return this.sample();
  }

  async readDeviceInfo(): Promise<DeviceInfo> {
    return { manufacturer: "Demo", model: "AirGlove Demo", firmware: "demo" };
  }

  async readBattery(): Promise<number> {
    return 87;
  }

  /* ── internals ────────────────────────────────────────────── */

  private emitConnection(s: ConnectionStatus) {
    this.connection = s;
    this.connectionCbs.forEach((cb) => cb(s));
  }

  private emitStatus(s: AgStatus) {
    this.statusCbs.forEach((cb) => cb(s));
  }

  private startStream() {
    this.startedAt = performance.now();
    this.seq = 0;
    this.setRate(this.rate);
  }

  private stopStream() {
    if (this.timer) {
      clearInterval(this.timer);
      this.timer = null;
    }
  }

  private setRate(rate: "idle" | "normal" | "fast") {
    this.rate = rate;
    if (this.connection !== "connected") return;
    if (this.timer) clearInterval(this.timer);
    const periodMs = Math.round(1000 / RATE_HZ[rate]);
    this.timer = setInterval(() => {
      this.telemetryCbs.forEach((cb) => cb(this.sample()));
    }, periodMs);
  }

  private sample(): AgTelemetry {
    const t = (performance.now() - this.startedAt) / 1000;
    this.seq = (this.seq + 1) & 0xff;

    // Synthetic IMU: a slow tilt around X plus a faster wobble on Y/Z gives
    // the calibrate read-outs something visibly alive without looking random.
    const accelX = this.sleeping ? 0 : Math.round(2000 * Math.sin(t * 0.6));
    const accelY = this.sleeping ? 0 : Math.round(1500 * Math.sin(t * 1.1 + 0.7));
    const accelZ = this.sleeping ? 16000 : Math.round(16000 + 500 * Math.sin(t * 0.4));
    const gyroX = this.sleeping ? 0 : Math.round(80 * Math.cos(t * 0.6));
    const gyroY = this.sleeping ? 0 : Math.round(60 * Math.cos(t * 1.1 + 0.7));
    const gyroZ = this.sleeping ? 0 : Math.round(40 * Math.sin(t * 1.7));

    // Touch: idle baseline ~120 with one pad lighting up at a time so the
    // Tune bars demonstrably move under a thumb-walk pattern.
    const base = 120;
    const lit = Math.floor(t * 0.8) % 4;
    const touch: [number, number, number, number] = [base, base, base, base];
    if (!this.sleeping) {
      touch[lit] = 1200 + Math.round(400 * Math.abs(Math.sin(t * 3)));
    }

    let flags = TELEMETRY_FLAG_HID_CONNECTED;
    if (this.sleeping) flags |= TELEMETRY_FLAG_SLEEPING;

    return {
      version: 1,
      seq: this.seq,
      accel: [accelX, accelY, accelZ],
      gyro: [gyroX, gyroY, gyroZ],
      touch,
      batteryPct: 87,
      flags,
    };
  }
}

function delay(ms: number): Promise<void> {
  return new Promise((r) => setTimeout(r, ms));
}
