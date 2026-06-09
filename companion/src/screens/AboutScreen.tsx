import { Command } from "../ble/types";
import { Row, Section } from "../ui/Section";
import { WorkScreen } from "../ui/WorkScreen";
import { store, useAppState } from "../state/store";

const CAPTION = "See device firmware info and reset stored configuration.";

export function AboutScreen() {
  return (
    <WorkScreen title="About" caption={CAPTION}>
      <AboutBody />
    </WorkScreen>
  );
}

function AboutBody() {
  const info = useAppState().deviceInfo;
  return (
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
  );
}
