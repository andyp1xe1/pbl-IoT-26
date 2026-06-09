import { useSyncExternalStore } from "react";
import { AirGloveClient, isWebBluetoothAvailable } from "../ble/client";
import {
  AgConfig,
  AgStatus,
  AgTelemetry,
  Command,
  ConnectionStatus,
  DeviceInfo,
  IAirGloveClient,
  defaultConfig,
} from "../ble/types";

export interface AppState {
  webBluetoothAvailable: boolean;
  status: ConnectionStatus;
  error: string | null;
  config: AgConfig;
  configDirtyLocal: boolean;
  telemetry: AgTelemetry | null;
  lastStatus: AgStatus | null;
  deviceInfo: DeviceInfo | null;
}

class Store {
  private state: AppState = {
    webBluetoothAvailable: isWebBluetoothAvailable(),
    status: "disconnected",
    error: null,
    config: defaultConfig(),
    configDirtyLocal: false,
    telemetry: null,
    lastStatus: null,
    deviceInfo: null,
  };

  private client: IAirGloveClient = this.makeClient();
  private listeners = new Set<() => void>();

  private makeClient(): IAirGloveClient {
    const client = new AirGloveClient();
    client.onConnectionChange((s) => {
      this.patch({ status: s });
      if (s === "connected") void this.refreshAfterConnect();
      if (s === "disconnected")
        this.patch({ telemetry: null, deviceInfo: null });
    });
    client.onTelemetry((t) => this.patch({ telemetry: t }));
    client.onStatus((st) => this.patch({ lastStatus: st }));
    return client;
  }

  getState = (): AppState => this.state;

  subscribe = (fn: () => void): (() => void) => {
    this.listeners.add(fn);
    return () => this.listeners.delete(fn);
  };

  private patch(next: Partial<AppState>) {
    this.state = { ...this.state, ...next };
    this.listeners.forEach((fn) => fn());
  }

  async connect() {
    this.patch({ error: null });
    try {
      await this.client.connect();
    } catch (err) {
      this.patch({ error: errorMessage(err) });
    }
  }

  async disconnect() {
    await this.client.disconnect();
  }

  private async refreshAfterConnect() {
    try {
      const [config, deviceInfo] = await Promise.all([
        this.client.readConfig(),
        this.client.readDeviceInfo(),
      ]);
      this.patch({ config, deviceInfo, configDirtyLocal: false });
    } catch (err) {
      this.patch({ error: errorMessage(err) });
    }
  }

  updateConfigLocal(patch: Partial<AgConfig>) {
    this.patch({
      config: { ...this.state.config, ...patch },
      configDirtyLocal: true,
    });
    this.scheduleConfigWrite();
  }

  private writeTimer: ReturnType<typeof setTimeout> | null = null;
  private scheduleConfigWrite() {
    if (this.writeTimer) clearTimeout(this.writeTimer);
    this.writeTimer = setTimeout(() => void this.flushConfig(), 150);
  }

  private async flushConfig() {
    if (this.state.status !== "connected") return;
    try {
      await this.client.writeConfig(this.state.config);
    } catch (err) {
      this.patch({ error: errorMessage(err) });
    }
  }

  async save() {
    await this.sendCommand(Command.SaveConfig);
    this.patch({ configDirtyLocal: false });
  }

  async sendCommand(opcode: Command) {
    try {
      await this.client.sendCommand(opcode);
    } catch (err) {
      this.patch({ error: errorMessage(err) });
    }
  }
}

function errorMessage(err: unknown): string {
  if (err instanceof Error) {
    if (err.name === "NotFoundError") return "No device selected.";
    return err.message;
  }
  return String(err);
}

export const store = new Store();

export function useAppState(): AppState {
  return useSyncExternalStore(store.subscribe, store.getState, store.getState);
}
