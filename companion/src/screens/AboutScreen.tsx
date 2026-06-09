import { Command } from "../ble/types";
import { Row, Section } from "../ui/Section";
import { Screen } from "../ui/Screen";
import { EmptyState } from "../ui/EmptyState";
import { store, useAppState } from "../state/store";

export function AboutScreen() {
  const s = useAppState();
  const connected = s.status === "connected";
  const info = s.deviceInfo;

  if (!connected) {
    return (
      <Screen title="About">
        <EmptyState
          title="No device"
          action={
            <button
              className="btn btn-primary"
              disabled={s.status === "connecting"}
              onClick={() => void store.connect()}
            >
              Connect
            </button>
          }
        />
      </Screen>
    );
  }

  return (
    <Screen title="About">
      <Section title="Device">
        <Row label="Manufacturer" value={info?.manufacturer ?? "—"} />
        <Row label="Model" value={info?.model ?? "—"} />
        <Row label="Firmware" value={info?.firmware ?? "—"} />
        <div className="card-actions">
          <button
            className="btn btn-ghost-danger"
            onClick={() => void store.sendCommand(Command.FactoryReset)}
          >
            Factory reset
          </button>
        </div>
      </Section>
    </Screen>
  );
}
