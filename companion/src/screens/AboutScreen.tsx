import { Command } from "../ble/types";
import { Row, Section } from "../ui/Section";
import { Screen } from "../ui/Screen";
import { store, useAppState } from "../state/store";

export function AboutScreen() {
  const s = useAppState();
  const connected = s.status === "connected";
  const info = s.deviceInfo;

  return (
    <Screen title="About">
      <Section title="Device">
        <Row label="Manufacturer" value={info?.manufacturer ?? "—"} />
        <Row label="Model" value={info?.model ?? "—"} />
        <Row label="Firmware" value={info?.firmware ?? "—"} />
        {connected && (
          <div className="card-actions">
            <button
              className="btn btn-ghost-danger"
              onClick={() => void store.sendCommand(Command.FactoryReset)}
            >
              Factory reset
            </button>
          </div>
        )}
      </Section>

      <Section title="App">
        <Row label="Version" value={import.meta.env.VITE_APP_VERSION ?? "0.1.0"} />
      </Section>
    </Screen>
  );
}
