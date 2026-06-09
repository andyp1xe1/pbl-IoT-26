import { Row, Section } from "../ui/Section";
import { Screen } from "../ui/Screen";
import { EmptyState } from "../ui/EmptyState";
import { store, useAppState } from "../state/store";
import { TELEMETRY_FLAG_HID_CONNECTED } from "../ble/types";

export function ConnectScreen() {
  const s = useAppState();
  const connected = s.status === "connected";
  const hidActive =
    s.telemetry != null &&
    (s.telemetry.flags & TELEMETRY_FLAG_HID_CONNECTED) !== 0;

  if (!s.webBluetoothAvailable) {
    return (
      <Screen title="Connect">
        <EmptyState
          title="Unsupported browser"
          caption="Use Chrome, Edge, Brave, or another Chromium-based browser."
        />
      </Screen>
    );
  }

  if (!connected) {
    const failed = s.error != null && s.status !== "connecting";
    return (
      <Screen title="Connect">
        <EmptyState
          title={
            s.status === "connecting"
              ? "Connecting…"
              : failed
                ? "Couldn't connect"
                : "Not connected"
          }
          error={failed ? s.error : null}
          action={
            <button
              className="btn btn-primary"
              disabled={s.status === "connecting"}
              onClick={() => void store.connect()}
            >
              {failed ? "Try again" : "Connect"}
            </button>
          }
        />
      </Screen>
    );
  }

  return (
    <Screen title="Connect">
      <Section title="Device">
        <Row
          label="Air Glove"
          value={<StatusPill status="Connected" tone="good" />}
        />
        <Row
          label="Mouse"
          value={
            <StatusPill
              status={hidActive ? "Active" : "Idle"}
              tone={hidActive ? "good" : "off"}
            />
          }
        />
        <div className="card-actions">
          <button
            className="btn btn-ghost-danger"
            onClick={() => void store.disconnect()}
          >
            Disconnect
          </button>
        </div>
        {s.error && <p className="section-footer error-text">{s.error}</p>}
      </Section>
    </Screen>
  );
}

function StatusPill({
  status,
  tone,
}: {
  status: string;
  tone: "good" | "warn" | "off";
}) {
  return <span className={`pill pill-${tone}`}>{status}</span>;
}
