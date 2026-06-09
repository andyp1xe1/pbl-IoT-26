import { Row, Section } from "../ui/Section";
import { Screen } from "../ui/Screen";
import { store, useAppState } from "../state/store";
import { TELEMETRY_FLAG_HID_CONNECTED } from "../ble/types";

export function ConnectScreen() {
  const s = useAppState();
  const connected = s.status === "connected";
  const hidConnected =
    s.telemetry != null &&
    (s.telemetry.flags & TELEMETRY_FLAG_HID_CONNECTED) !== 0;

  return (
    <Screen
      title="Connect"
      subtitle="Link this app to your glove to tune, calibrate, and watch live sensor data."
    >
      <Section title="Connection">
        <Row
          label="Air Glove"
          value={
            <StatusPill
              status={
                s.status === "connecting"
                  ? "Connecting…"
                  : connected
                    ? "Connected"
                    : "Not connected"
              }
              tone={connected ? "good" : s.status === "connecting" ? "warn" : "off"}
            />
          }
        />
        {connected && (
          <>
            <Row
              label="Battery"
              value={s.battery != null ? `${s.battery}%` : "—"}
            />
            <Row
              label="Mouse (HID)"
              value={
                <StatusPill
                  status={hidConnected ? "Active" : "Idle"}
                  tone={hidConnected ? "good" : "off"}
                />
              }
            />
          </>
        )}
        <div className="card-actions">
          {!connected ? (
            <button
              className="btn btn-primary"
              disabled={s.status === "connecting"}
              onClick={() => void store.connect()}
            >
              {s.status === "connecting" ? "Connecting…" : "Connect to Air Glove"}
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

      <Section title="First time?">
        <ol className="steps steps-numbered">
          <li>Open your OS Bluetooth settings and pair “AirGlove” (it appears as a mouse).</li>
          <li>Come back here and tap Connect to adjust settings and calibrate.</li>
        </ol>
      </Section>

      <Section
        title="How it works"
        footer="The glove pairs as a normal Bluetooth mouse in your operating system. This app connects separately to its configuration service — it does not handle mouse pairing."
      >
        <Row label="Pairing" value="Owned by your OS" />
        <Row label="This app" value="Config & telemetry" />
        {!s.webBluetoothAvailable && (
          <Row label="Mode" value="Mock (no hardware)" />
        )}
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
