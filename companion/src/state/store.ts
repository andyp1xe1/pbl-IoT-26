import { useSyncExternalStore } from "react";
import {
  AirGloveClient,
  BleConnectError,
  isWebBluetoothAvailable,
} from "../ble/client";
import { DemoAirGloveClient } from "../ble/demo-client";

/** Build-time switch flipped by `npm run demo` (sets VITE_DEMO=1) so a
 *  designer can drive the full UI without a glove or a Web-Bluetooth host. */
const DEMO_MODE =
  (import.meta.env as { VITE_DEMO?: string }).VITE_DEMO === "1";
import {
  AgConfig,
  AgStatus,
  AgTelemetry,
  Command,
  ConnectionStatus,
  DeviceInfo,
  IAirGloveClient,
  KnownDevice,
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
  /** Measured rate at which telemetry notifications are arriving at the UI,
   *  in Hz. Rolling 1-second window. Zero when disconnected or no traffic.
   *  Diagnostic — surface on Calibrate so the user can sanity-check that
   *  the firmware's TELE_FAST (60 Hz target) actually reaches the browser. */
  telemetryHz: number;
  lastStatus: AgStatus | null;
  deviceInfo: DeviceInfo | null;
  /** Remembered devices the browser will let us reconnect to silently
   *  (Web Bluetooth `getDevices()`). Empty on Safari/Firefox. */
  knownDevices: KnownDevice[];
  tab: TabId;
}

class Store {
  private state: AppState = {
    webBluetoothAvailable: DEMO_MODE || isWebBluetoothAvailable(),
    status: "disconnected",
    error: null,
    config: defaultConfig(),
    configDirtyLocal: false,
    telemetry: null,
    telemetryHz: 0,
    lastStatus: null,
    deviceInfo: null,
    knownDevices: [],
    tab: "device",
  };

  private client: IAirGloveClient = this.makeClient();
  private listeners = new Set<() => void>();

  constructor() {
    // Rehydrate the last-opened tab so a refresh doesn't dump the user back
    // on Home. Other slices (config, telemetry, knownDevices) come from the
    // device on (re)connect, so they don't need persisting.
    const savedTab = loadTab();
    if (savedTab) this.state.tab = savedTab;

    // List previously-permitted devices, then attempt a single silent
    // reconnect to the first one. Fire-and-forget — browsers without
    // getDevices() simply yield []; reconnect failures are swallowed so a
    // refresh while the glove is out of range or off doesn't surface an
    // error before the user does anything.
    void this.bootReconnect();
  }

  private async bootReconnect() {
    await this.refreshKnownDevices();
    const id = this.state.knownDevices[0]?.id;
    if (!id || !this.state.webBluetoothAvailable) return;
    try {
      await this.client.reconnect(id);
    } catch {
      /* one-shot — user can flip the switch manually if it fails */
    }
  }

  private makeClient(): IAirGloveClient {
    const client: IAirGloveClient = DEMO_MODE
      ? new DemoAirGloveClient()
      : new AirGloveClient();
    client.onConnectionChange((s) => {
      if (s === "connected") {
        // Pull the device identity off the client *synchronously* with the
        // status flip so the next render already has a name to show. Without
        // this, knownDevices stays empty until refreshKnownDevices() (an
        // async getDevices() call) completes a few ms later, causing the
        // Device title to flash a generic placeholder.
        const d = client.currentDevice;
        this.patch({
          status: s,
          knownDevices: d ? [d] : this.state.knownDevices,
        });
        void this.refreshAfterConnect();
      } else if (s === "disconnected") {
        this.disarmTelemetryWatchdog();
        this.notifyTimes.length = 0;
        this.patch({ status: s, telemetry: null, telemetryHz: 0, deviceInfo: null });
      } else {
        this.patch({ status: s });
      }
    });
    client.onTelemetry((t) => {
      // Rolling 1-second window: arrival count = Hz. Cheap (push + shift),
      // good enough resolution for a diagnostic readout that refreshes
      // every notify anyway.
      const now = performance.now();
      this.notifyTimes.push(now);
      while (this.notifyTimes.length && now - this.notifyTimes[0] > 1000) {
        this.notifyTimes.shift();
      }
      this.patch({ telemetry: t, telemetryHz: this.notifyTimes.length });
      this.armTelemetryWatchdog();
    });
    client.onStatus((st) => this.patch({ lastStatus: st }));
    return client;
  }

  /* ── Telemetry watchdog ────────────────────────────────────────────────
   * The notify stream is the ground truth for live sensor state — it fires
   * at ~5 Hz while the device is awake AND while it's sleeping (the
   * sleeping frame just has TFLAG_SLEEPING set). If frames stop arriving
   * for more than ~2.5 s while we still think we're connected, something
   * is off: subscription dropped, BLE link in a half-state, or firmware
   * stuck in a state we don't know about. Doing a one-shot readTelemetry()
   * forces a GATT read that returns the current value and (on a healthy
   * link) re-opens the dialogue. Only fired *lazily*, when the data
   * actually goes quiet — no eager polling. */
  private telemetryWatchdog: ReturnType<typeof setTimeout> | null = null;
  private notifyTimes: number[] = [];

  private armTelemetryWatchdog() {
    this.disarmTelemetryWatchdog();
    // Generous enough to ride out the IDLE-tab 500 ms period plus a few
    // skipped frames (BLE conn-event jitter) without false-firing. At
    // FAST and NORMAL rates we'll only ever rearm this from the first
    // notify of the interval, so the budget is plenty.
    this.telemetryWatchdog = setTimeout(() => {
      if (this.state.status === "connected") void this.refreshState();
    }, 3500);
  }

  private disarmTelemetryWatchdog() {
    if (this.telemetryWatchdog) {
      clearTimeout(this.telemetryWatchdog);
      this.telemetryWatchdog = null;
    }
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
    saveTab(tab);
  };

  async connect() {
    // Anything that prevents the user from completing a connect here belongs
    // on the Device tab where there's a full error/retry surface. Either
    // case below routes them there.
    if (!this.state.webBluetoothAvailable) {
      this.patch({ tab: "device" });
      return;
    }
    this.patch({ error: null });
    try {
      await this.client.connect();
      void this.refreshKnownDevices();
    } catch (err) {
      this.patch({ error: errorMessage(err), tab: "device" });
    }
  }

  /** Skip the chooser and link to a remembered device. If reconnect fails
   *  (device out of range, browser permission revoked, …) we fall back to
   *  the chooser so the user can pair again. */
  async reconnect(id?: string) {
    if (!this.state.webBluetoothAvailable) return;
    this.patch({ error: null });
    try {
      await this.client.reconnect(id);
    } catch (err) {
      // Common case: BleConnectError "link"/NetworkError — device is asleep
      // or out of range. Surface a short hint; don't auto-open the chooser
      // since that would surprise the user mid-click.
      this.patch({ error: errorMessage(err), tab: "device" });
    }
  }

  /** Wipe everything we know about the device — both the BLE-stack
   *  permission and the in-memory snapshot. After this the screen is in
   *  the same shape as a fresh first-run: switches off, info card
   *  skeletons, title falls back to the generic "Air Glove". The user
   *  has to flip the Connection switch (which opens the chooser) to
   *  pair again, exactly as if they'd never paired before.
   *
   *  Order matters: disconnect first so any in-flight telemetry stops
   *  arriving and the disconnect handler runs cleanly, *then* revoke
   *  the permission, *then* explicitly null the snapshot fields the
   *  disconnect handler doesn't touch (lastStatus, error, config).
   *  Calling forget() on an already-disconnected device is a no-op on
   *  the GATT side, which is what we want. */
  async forget(id?: string) {
    try {
      await this.client.disconnect();
    } catch {
      /* tearing down — ignore */
    }
    try {
      await this.client.forget(id);
    } catch (err) {
      this.patch({ error: errorMessage(err) });
    }
    this.patch({
      status: "disconnected",
      telemetry: null,
      deviceInfo: null,
      lastStatus: null,
      error: null,
      knownDevices: [],
      configDirtyLocal: false,
    });
    // Refresh from the browser in case forget() was a no-op (older Chromium)
    // — the list will then reflect what's truly remembered.
    void this.refreshKnownDevices();
  }

  async refreshKnownDevices() {
    try {
      const knownDevices = await this.client.listKnownDevices();
      this.patch({ knownDevices });
    } catch {
      // Non-fatal: getDevices() may be gated or unimplemented.
    }
  }

  async disconnect() {
    await this.client.disconnect();
  }

  private async refreshAfterConnect() {
    // Sequence the GATT reads — running them in parallel races with the
    // notification setup that just finished in attach() and on some
    // Chromium / BlueZ combos quietly stalls the telemetry subscription
    // afterward. One at a time, patching as each lands so the UI fills
    // in progressively.
    try {
      const config = await this.client.readConfig();
      this.patch({ config, configDirtyLocal: false });
    } catch (err) {
      this.patch({ error: errorMessage(err) });
    }
    try {
      const deviceInfo = await this.client.readDeviceInfo();
      this.patch({ deviceInfo });
    } catch (err) {
      this.patch({ error: errorMessage(err) });
    }
    // Live telemetry arrives via the notify subscription set up in attach().
    // The watchdog below catches the case where it silently stops flowing
    // (e.g. the device entered sleep without us asking) and only THEN
    // does a one-shot read to learn the truth.
    this.armTelemetryWatchdog();
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

  /** Factory reset: tell the firmware to wipe NVS + restore builtin
   *  defaults, then re-read the config characteristic so the Tune sliders
   *  immediately reflect the new values. The firmware reseeds the config
   *  characteristic synchronously inside the command handler, but does not
   *  send a notify — so without this re-read the UI would keep showing
   *  pre-reset state for the lifetime of the connection. */
  async factoryReset() {
    await this.sendCommand(Command.FactoryReset);
    // Tiny settle: the cmd handler runs on t_cfg's next iteration which can
    // be up to one telemetry period away (250 ms at IDLE). 300 ms covers
    // the worst case without making the user wait perceptibly.
    await new Promise((r) => setTimeout(r, 300));
    try {
      const config = await this.client.readConfig();
      this.patch({ config, configDirtyLocal: false });
    } catch (err) {
      this.patch({ error: errorMessage(err) });
    }
  }

  async sendCommand(opcode: Command) {
    try {
      await this.client.sendCommand(opcode);
    } catch (err) {
      this.patch({ error: errorMessage(err) });
    }
  }

  /** Put the device into soft-sleep. Firmware powers the MPU down + stops
   *  emitting HID reports; the BLE link stays up so the next telemetry
   *  frame carries the SLEEPING flag and the Power switch flips. We do
   *  not eager-read the state here — the notify stream is faster than a
   *  GATT read and chaining a read on the heels of a write has been
   *  shown to disrupt the subscription on some Chromium / BlueZ pairs.
   *  The watchdog will catch the case where notifications go quiet. */
  sleep() {
    return this.sendCommand(Command.Sleep);
  }

  wake() {
    return this.sendCommand(Command.Wake);
  }

  /** Hint the firmware which telemetry rate to publish at — sent on tab
   *  changes so each screen gets a stream sized to what it actually
   *  shows. Idempotent: re-sending the same rate is a no-op on firmware.
   *  Silent no-op when not connected. */
  setTelemetryRate(rate: "idle" | "normal" | "fast") {
    if (this.state.status !== "connected") return;
    const op =
      rate === "fast"   ? Command.TelemetryFast :
      rate === "normal" ? Command.TelemetryNormal :
                          Command.TelemetryIdle;
    return this.sendCommand(op);
  }

  /** Read the telemetry characteristic on demand so the app state can be
   *  reconciled with what the firmware *actually* reports — useful when
   *  the notify stream falls behind a command (sleep/wake transition) or
   *  if subscriptions silently drop. Optional delay lets the firmware
   *  finish a transition (e.g. the 35 ms MPU wake settle) before we read. */
  async refreshState(delayMs = 0) {
    if (this.state.status !== "connected") return;
    if (delayMs > 0) await new Promise((r) => setTimeout(r, delayMs));
    try {
      const telemetry = await this.client.readTelemetry();
      this.patch({ telemetry });
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

/* ── Tab persistence ──
 * Only the active tab is mirrored to localStorage; everything else either
 * comes from the device on connect or is ephemeral. */
const TAB_KEY = "airglove.tab";
const TAB_VALUES: ReadonlySet<TabId> = new Set([
  "device",
  "tune",
  "calibrate",
  "about",
]);

function loadTab(): TabId | null {
  try {
    const v = localStorage.getItem(TAB_KEY);
    return v && TAB_VALUES.has(v as TabId) ? (v as TabId) : null;
  } catch {
    return null;
  }
}

function saveTab(tab: TabId) {
  try {
    localStorage.setItem(TAB_KEY, tab);
  } catch {
    /* localStorage may be disabled (private mode, quota); persistence is best-effort */
  }
}

export const store = new Store();

export function useAppState(): AppState {
  return useSyncExternalStore(store.subscribe, store.getState, store.getState);
}
