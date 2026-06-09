import { Command } from "../ble/types";
import { Row, Section } from "../ui/Section";
import { Screen } from "../ui/Screen";
import { store, useAppState } from "../state/store";

export function AboutScreen() {
  const s = useAppState();
  const connected = s.status === "connected";
  const info = s.deviceInfo;

  return (
    <Screen title="About" subtitle="Device and app information.">
      <Section title="Device">
        <Row label="Manufacturer" value={info?.manufacturer ?? "—"} />
        <Row label="Model" value={info?.model ?? "—"} />
        <Row label="Firmware" value={info?.firmware ?? "—"} />
        <Row
          label="Connection"
          value={s.useMock ? "Mock client" : "Web Bluetooth"}
        />
        {connected && (
          <div className="card-actions">
            <button
              className="btn btn-ghost-danger"
              onClick={() => void store.sendCommand(Command.FactoryReset)}
            >
              Factory-reset config
            </button>
          </div>
        )}
      </Section>

      <Section title="App">
        <Row label="Version" value={import.meta.env.VITE_APP_VERSION ?? "0.1.0"} />
        <Row label="Transport" value="BLE GATT (custom service)" />
      </Section>

      <Section title="Known limitations">
        <ul className="bullets">
          <li>Web Bluetooth is unsupported on iOS/Safari.</li>
          <li>One configuration host at a time.</li>
          <li>Mouse pairing is handled by the operating system, not this app.</li>
        </ul>
      </Section>
    </Screen>
  );
}
