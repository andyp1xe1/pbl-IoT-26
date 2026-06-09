import { useSyncExternalStore } from "react";
import {
  AirGloveClient,
  BleConnectError,
  isWebBluetoothAvailable,
} from "../ble/client";
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
import type { TabId } from "../ui/TabBar";

export interface AppState {
  webBluetoothAvailable: boolean;
  status: ConnectionStatus;
  error: string | null;
  config: AgConfig;
  configDirtyLocal: boolean;
  telemetry: AgTelemetry | null;
  lastStatus: AgStatus | null;
  deviceInfo: DeviceInfo | null;
  tab: TabId;
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
    tab: "connect",
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

  setTab = (tab: TabId) => {
    this.patch({ tab });
  };

  async connect() {
    // Anything that prevents the user from completing a connect here belongs
    // on the Connect tab where there's a full error/retry surface. Either
    // case below routes them there.
    if (!this.state.webBluetoothAvailable) {
      this.patch({ tab: "connect" });
      return;
    }
    this.patch({ error: null });
    try {
      await this.client.connect();
    } catch (err) {
      this.patch({ error: errorMessage(err), tab: "connect" });
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

/** Map a connect-time failure to a short user-facing line. The raw error
 *  is logged separately via the ble/log helper. NotFoundError is heavily
 *  overloaded by the Web Bluetooth spec — its meaning depends on *which*
 *  step rejected, hence the phase-aware branches. */
function errorMessage(err: unknown): string {
  if (err instanceof BleConnectError) {
    const cause = err.cause;
    const name = cause.name;
    const msg = cause.message || "";

    switch (err.phase) {
      case "scan":
        if (name === "NotFoundError") {
          if (/cancel/i.test(msg)) return "Cancelled.";
          return "No glove found. Make sure it's powered on and nearby.";
        }
        if (name === "SecurityError") return "Blocked by the browser.";
        break;

      case "link":
        if (name === "NetworkError") {
          return "Couldn't link. Move closer, check the battery, or disconnect it from another host.";
        }
        return `Couldn't link: ${msg || name}.`;

      case "service":
        if (name === "NotFoundError") {
          return "This device's firmware is too old.";
        }
        return msg || name;

      case "characteristic":
        if (name === "NotFoundError") {
          return "This device's firmware is incomplete.";
        }
        return msg || name;

      case "notify":
        return `Couldn't subscribe: ${msg || name}.`;
    }
    return msg || name;
  }

  if (err instanceof Error) {
    if (err.name === "InvalidStateError")
      return "Bluetooth is off. Turn it on and retry.";
    if (err.name === "AbortError") return "Cancelled.";
    return err.message || err.name;
  }
  return String(err);
}

export const store = new Store();

export function useAppState(): AppState {
  return useSyncExternalStore(store.subscribe, store.getState, store.getState);
}
