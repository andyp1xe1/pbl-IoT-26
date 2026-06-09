import {
  decodeConfig,
  decodeStatus,
  decodeTelemetry,
  encodeConfig,
} from "./codec";
import { log } from "./log";

/** Phase of the connect handshake — used by the store to pick a friendly
 *  message that actually matches what failed. NotFoundError alone is
 *  ambiguous: the spec uses it for scan-cancelled, scan-empty,
 *  service-missing, and characteristic-missing. */
export type ConnectPhase =
  | "scan"           // navigator.bluetooth.requestDevice()
  | "link"           // device.gatt.connect()
  | "service"        // server.getPrimaryService()
  | "characteristic" // svc.getCharacteristic()
  | "notify";        // startNotifications()

export class BleConnectError extends Error {
  constructor(
    public phase: ConnectPhase,
    public cause: Error,
  ) {
    super(cause.message);
    this.name = "BleConnectError";
  }
}

async function withPhase<T>(phase: ConnectPhase, fn: () => Promise<T>): Promise<T> {
  try {
    return await fn();
  } catch (err) {
    throw new BleConnectError(phase, err as Error);
  }
}
import {
  AgConfig,
  AgStatus,
  AgTelemetry,
  Command,
  ConnectionStatus,
  DeviceInfo,
  IAirGloveClient,
  KnownDevice,
} from "./types";
import {
  BATTERY_LEVEL,
  BATTERY_SERVICE,
  COMMAND_CHAR,
  CONFIG_CHAR,
  CONFIG_SERVICE,
  DIS_FIRMWARE,
  DIS_MANUFACTURER,
  DIS_MODEL,
  DIS_SERVICE,
  STATUS_CHAR,
  TELEMETRY_CHAR,
} from "./uuids";

export function isWebBluetoothAvailable(): boolean {
  return typeof navigator !== "undefined" && !!navigator.bluetooth;
}

export class AirGloveClient implements IAirGloveClient {
  readonly kind = "real" as const;

  private device: BluetoothDevice | null = null;
  private server: BluetoothRemoteGATTServer | null = null;
  private configChar: BluetoothRemoteGATTCharacteristic | null = null;
  private commandChar: BluetoothRemoteGATTCharacteristic | null = null;
  private telemetryChar: BluetoothRemoteGATTCharacteristic | null = null;

  get currentDevice(): import("./types").KnownDevice | null {
    if (!this.device) return null;
    return { id: this.device.id, name: this.device.name ?? "AirGlove" };
  }

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

  private emitConnection(s: ConnectionStatus) {
    this.connectionCbs.forEach((cb) => cb(s));
  }

  async connect(): Promise<void> {
    if (!isWebBluetoothAvailable()) {
      log.error("navigator.bluetooth is undefined — browser lacks Web Bluetooth");
      throw new Error("Web Bluetooth is not available in this browser.");
    }
    this.emitConnection("connecting");
    try {
      // Filter by name, not service UUID: the glove advertises as a HID mouse
      // and does not put the 128-bit config service UUID in its (size-limited)
      // advertisement. The custom service is discovered after connecting.
      log.step("requestDevice", { namePrefix: "AirGlove" });
      const device = await withPhase("scan", () =>
        navigator.bluetooth.requestDevice({
          filters: [{ namePrefix: "AirGlove" }],
          optionalServices: [CONFIG_SERVICE, DIS_SERVICE, BATTERY_SERVICE],
        }),
      );
      log.info("device picked", { name: device.name, id: device.id });
      await this.attach(device);
    } catch (err) {
      this.handleConnectError(err);
      throw err;
    }
  }

  async reconnect(id?: string): Promise<void> {
    if (!isWebBluetoothAvailable()) {
      throw new Error("Web Bluetooth is not available in this browser.");
    }
    this.emitConnection("connecting");
    try {
      const known = await this.knownDevices();
      const device = id ? known.find((d) => d.id === id) : known[0];
      if (!device) {
        // No remembered device — caller should fall back to connect().
        // Throw a recognisable phase so the store can surface a sensible message.
        throw new BleConnectError(
          "scan",
          Object.assign(new Error("No known device"), { name: "NotFoundError" }),
        );
      }
      log.info("reconnecting to remembered device", { name: device.name, id: device.id });
      await this.attach(device);
    } catch (err) {
      this.handleConnectError(err);
      throw err;
    }
  }

  async listKnownDevices(): Promise<KnownDevice[]> {
    const devices = await this.knownDevices();
    return devices.map((d) => ({ id: d.id, name: d.name ?? "AirGlove" }));
  }

  async forget(id?: string): Promise<void> {
    const known = await this.knownDevices();
    const device = id ? known.find((d) => d.id === id) : known[0];
    if (!device) return;
    const f = (device as BluetoothDevice & { forget?: () => Promise<void> }).forget;
    if (typeof f !== "function") {
      log.warn("BluetoothDevice.forget() unavailable in this browser");
      return;
    }
    try {
      await f.call(device);
      log.info("forgot device", { name: device.name, id: device.id });
    } catch (err) {
      log.error("forget failed", err as Error);
      throw err;
    }
  }

  /** Browser-permission list, filtered to AirGlove devices. Empty if the
   *  browser doesn't implement getDevices() (Safari, Firefox today). */
  private async knownDevices(): Promise<BluetoothDevice[]> {
    if (!isWebBluetoothAvailable()) return [];
    const bt = navigator.bluetooth as Bluetooth & {
      getDevices?: () => Promise<BluetoothDevice[]>;
    };
    if (typeof bt.getDevices !== "function") return [];
    try {
      const all = await bt.getDevices();
      return all.filter((d) => (d.name ?? "").startsWith("AirGlove"));
    } catch (err) {
      log.warn("getDevices() failed", err as Error);
      return [];
    }
  }

  /** Shared post-pick flow: open GATT, discover, subscribe. */
  private async attach(device: BluetoothDevice): Promise<void> {
    this.device = device;
    device.addEventListener("gattserverdisconnected", () => {
      log.warn("gattserverdisconnected — link dropped");
      this.emitConnection("disconnected");
    });

    log.step("gatt.connect");
    const server = await withPhase("link", () => device.gatt!.connect());
    this.server = server;
    log.info("GATT connected");

    log.step("getPrimaryService", CONFIG_SERVICE);
    const svc = await withPhase("service", () =>
      server.getPrimaryService(CONFIG_SERVICE),
    );

    log.step("getCharacteristic", "config + command");
    this.configChar = await withPhase("characteristic", () =>
      svc.getCharacteristic(CONFIG_CHAR),
    );
    this.commandChar = await withPhase("characteristic", () =>
      svc.getCharacteristic(COMMAND_CHAR),
    );

    log.step("subscribe telemetry");
    const telemetry = await withPhase("characteristic", () =>
      svc.getCharacteristic(TELEMETRY_CHAR),
    );
    this.telemetryChar = telemetry;
    telemetry.addEventListener("characteristicvaluechanged", (e) => {
      const dv = (e.target as BluetoothRemoteGATTCharacteristic).value;
      if (dv) this.telemetryCbs.forEach((cb) => cb(decodeTelemetry(dv)));
    });
    await withPhase("notify", () => telemetry.startNotifications());

    log.step("subscribe status");
    const status = await withPhase("characteristic", () =>
      svc.getCharacteristic(STATUS_CHAR),
    );
    status.addEventListener("characteristicvaluechanged", (e) => {
      const dv = (e.target as BluetoothRemoteGATTCharacteristic).value;
      if (dv) this.statusCbs.forEach((cb) => cb(decodeStatus(dv)));
    });
    await withPhase("notify", () => status.startNotifications());

    log.info("connect complete");
    this.emitConnection("connected");
  }

  private handleConnectError(err: unknown) {
    if (err instanceof BleConnectError) {
      log.error(`connect failed at "${err.phase}": ${err.cause.name}`, err.cause);
    } else {
      log.error("connect failed", err as Error);
    }
    this.emitConnection("disconnected");
  }

  async disconnect(): Promise<void> {
    this.server?.disconnect();
    this.server = null;
    this.device = null;
    this.telemetryChar = null;
    this.configChar = null;
    this.commandChar = null;
    this.emitConnection("disconnected");
  }

  async readConfig(): Promise<AgConfig> {
    const dv = await this.requireConfigChar().readValue();
    return decodeConfig(dv);
  }

  async writeConfig(cfg: AgConfig): Promise<void> {
    await this.requireConfigChar().writeValue(encodeConfig(cfg));
  }

  async sendCommand(opcode: Command): Promise<void> {
    if (!this.commandChar) throw new Error("Not connected.");
    await this.commandChar.writeValue(new Uint8Array([opcode]));
  }

  async readTelemetry(): Promise<AgTelemetry> {
    if (!this.telemetryChar) throw new Error("Not connected.");
    const dv = await this.telemetryChar.readValue();
    return decodeTelemetry(dv);
  }

  async readDeviceInfo(): Promise<DeviceInfo> {
    const fallback = { manufacturer: "—", model: "—", firmware: "—" };
    if (!this.server) return fallback;
    try {
      const dis = await this.server.getPrimaryService(DIS_SERVICE);
      const dec = new TextDecoder();
      const read = async (uuid: string) => {
        try {
          const c = await dis.getCharacteristic(uuid);
          return dec.decode(await c.readValue()).trim();
        } catch {
          return "—";
        }
      };
      return {
        manufacturer: await read(DIS_MANUFACTURER),
        model: await read(DIS_MODEL),
        firmware: await read(DIS_FIRMWARE),
      };
    } catch {
      return fallback;
    }
  }

  async readBattery(): Promise<number> {
    if (!this.server) return 0;
    try {
      const svc = await this.server.getPrimaryService(BATTERY_SERVICE);
      const c = await svc.getCharacteristic(BATTERY_LEVEL);
      return (await c.readValue()).getUint8(0);
    } catch {
      return 0;
    }
  }

  private requireConfigChar(): BluetoothRemoteGATTCharacteristic {
    if (!this.configChar) throw new Error("Not connected.");
    return this.configChar;
  }
}
