import {
  decodeConfig,
  decodeStatus,
  decodeTelemetry,
  encodeConfig,
} from "./codec";
import {
  AgConfig,
  AgStatus,
  AgTelemetry,
  Command,
  ConnectionStatus,
  DeviceInfo,
  IAirGloveClient,
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
      throw new Error("Web Bluetooth is not available in this browser.");
    }
    this.emitConnection("connecting");
    try {
      this.device = await navigator.bluetooth.requestDevice({
        filters: [{ services: [CONFIG_SERVICE] }],
        optionalServices: [CONFIG_SERVICE, DIS_SERVICE, BATTERY_SERVICE],
      });
      this.device.addEventListener("gattserverdisconnected", () =>
        this.emitConnection("disconnected"),
      );

      const server = await this.device.gatt!.connect();
      this.server = server;

      const svc = await server.getPrimaryService(CONFIG_SERVICE);
      this.configChar = await svc.getCharacteristic(CONFIG_CHAR);
      this.commandChar = await svc.getCharacteristic(COMMAND_CHAR);

      const telemetry = await svc.getCharacteristic(TELEMETRY_CHAR);
      telemetry.addEventListener("characteristicvaluechanged", (e) => {
        const dv = (e.target as BluetoothRemoteGATTCharacteristic).value;
        if (dv) this.telemetryCbs.forEach((cb) => cb(decodeTelemetry(dv)));
      });
      await telemetry.startNotifications();

      const status = await svc.getCharacteristic(STATUS_CHAR);
      status.addEventListener("characteristicvaluechanged", (e) => {
        const dv = (e.target as BluetoothRemoteGATTCharacteristic).value;
        if (dv) this.statusCbs.forEach((cb) => cb(decodeStatus(dv)));
      });
      await status.startNotifications();

      this.emitConnection("connected");
    } catch (err) {
      this.emitConnection("disconnected");
      throw err;
    }
  }

  async disconnect(): Promise<void> {
    this.server?.disconnect();
    this.server = null;
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
