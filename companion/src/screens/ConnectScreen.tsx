import { Row, Section } from "../ui/Section";
import { Screen } from "../ui/Screen";
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
        <Section>
          <Row label="Web Bluetooth" value="Not available" />
          <p className="section-footer">Open this page in Chrome or Edge.</p>
        </Section>
      </Screen>
    );
  }

  return (
    <Screen title="Connect">
      <Section title="Device">
        <Row
          label="Air Glove"
          value={
            <StatusPill
              status={
                s.status === "connecting"
                  ? "Connecting"
                  : connected
                    ? "Connected"
                    : "Not connected"
              }
              tone={connected ? "good" : s.status === "connecting" ? "warn" : "off"}
            />
          }
        />
        {connected && (
          <Row
            label="Mouse"
            value={
              <StatusPill
                status={hidActive ? "Active" : "Idle"}
                tone={hidActive ? "good" : "off"}
              />
            }
          />
        )}
        <div className="card-actions">
          {!connected ? (
            <button
              className="btn btn-primary"
              disabled={s.status === "connecting"}
              onClick={() => void store.connect()}
            >
              Connect
            </button>
          ) : (
            <button
              className="btn btn-ghost-danger"
              onClick={() => void store.disconnect()}
            >
              Disconnect
            </button>
          )}
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
