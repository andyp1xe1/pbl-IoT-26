import {
  AgConfig,
  AgStatus,
  AgTelemetry,
  Command,
  ConnectionStatus,
  CONFIG_FLAG_DIRTY,
  DeviceInfo,
  IAirGloveClient,
  StatusState,
  TELEMETRY_FLAG_CALIBRATING,
  TELEMETRY_FLAG_HID_CONNECTED,
  defaultConfig,
} from "./types";

export class MockAirGloveClient implements IAirGloveClient {
  readonly kind = "mock" as const;

  private config = defaultConfig();
  private seq = 0;
  private calibrating = false;
  private telemetryTimer: ReturnType<typeof setInterval> | null = null;

  private connectionCbs: ((s: ConnectionStatus) => void)[] = [];
  private telemetryCbs: ((t: AgTelemetry) => void)[] = [];
  private statusCbs: ((s: AgStatus) => void)[] = [];

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
    this.connectionCbs.forEach((cb) => cb("connecting"));
    await delay(400);
    this.connectionCbs.forEach((cb) => cb("connected"));
    this.startTelemetry();
  }

  async disconnect(): Promise<void> {
    if (this.telemetryTimer) clearInterval(this.telemetryTimer);
    this.telemetryTimer = null;
    this.connectionCbs.forEach((cb) => cb("disconnected"));
  }

  async readConfig(): Promise<AgConfig> {
    return { ...this.config };
  }

  async writeConfig(cfg: AgConfig): Promise<void> {
    this.config = { ...cfg, flags: cfg.flags | CONFIG_FLAG_DIRTY };
  }

  async sendCommand(opcode: Command): Promise<void> {
    switch (opcode) {
      case Command.SaveConfig:
        this.config.flags &= ~CONFIG_FLAG_DIRTY;
        this.emitStatus(opcode, StatusState.Success, 100);
        break;
      case Command.FactoryReset:
        this.config = defaultConfig();
        this.emitStatus(opcode, StatusState.Success, 100);
        break;
      case Command.CalibrateImu:
      case Command.RecalibrateTouch:
        await this.runCalibration(opcode);
        break;
    }
  }

  async readDeviceInfo(): Promise<DeviceInfo> {
    return {
      manufacturer: "AirGlove",
      model: "AG-DEV (mock)",
      firmware: "0.4.2-mock",
    };
  }

  async readBattery(): Promise<number> {
    return 87;
  }

  private async runCalibration(opcode: Command) {
    this.calibrating = true;
    for (let p = 0; p <= 100; p += 20) {
      this.emitStatus(opcode, StatusState.Running, p);
      await delay(300);
    }
    this.calibrating = false;
    this.emitStatus(opcode, StatusState.Success, 100);
  }

  private emitStatus(lastOpcode: number, state: StatusState, progress: number) {
    this.statusCbs.forEach((cb) =>
      cb({ lastOpcode, state, progress, reserved: 0 }),
    );
  }

  private startTelemetry() {
    if (this.telemetryTimer) clearInterval(this.telemetryTimer);
    this.telemetryTimer = setInterval(() => {
      const t = Date.now() / 1000;
      let flags = TELEMETRY_FLAG_HID_CONNECTED;
      if (this.calibrating) flags |= TELEMETRY_FLAG_CALIBRATING;
      this.telemetryCbs.forEach((cb) =>
        cb({
          version: 1,
          seq: this.seq++ & 0xff,
          accel: [
            Math.round(Math.sin(t) * 30),
            Math.round(Math.cos(t * 1.3) * 30),
            Math.round(1000 + Math.sin(t * 2) * 8),
          ],
          gyro: [
            Math.round(Math.sin(t * 3) * 4),
            Math.round(Math.cos(t * 2.1) * 4),
            Math.round(Math.sin(t * 1.7) * 3),
          ],
          touch: [
            noisy(140, 8),
            this.calibrating ? noisy(680, 40) : noisy(620, 90),
            noisy(208, 12),
            noisy(171, 10),
          ],
          batteryPct: 87,
          flags,
        }),
      );
    }, 66);
  }
}

function noisy(base: number, amp: number): number {
  return Math.max(0, Math.round(base + (Math.random() - 0.5) * amp));
}

function delay(ms: number): Promise<void> {
  return new Promise((r) => setTimeout(r, ms));
}
