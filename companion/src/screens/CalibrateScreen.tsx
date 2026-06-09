import { Command, StatusState } from "../ble/types";
import { Row, Section } from "../ui/Section";
import { Screen } from "../ui/Screen";
import { StatBox, StatGrid } from "../ui/StatBox";
import { store, useAppState } from "../state/store";

export function CalibrateScreen() {
  const s = useAppState();
  const connected = s.status === "connected";
  const t = s.telemetry;
  const st = s.lastStatus;
  const running = st?.state === StatusState.Running;

  if (!connected) {
    return (
      <Screen title="Calibrate">
        <Section>
          <Row label="Not connected" />
        </Section>
      </Screen>
    );
  }

  return (
    <Screen title="Calibrate">
      <Section title="IMU — live">
        <StatGrid>
          <StatBox label="ACCEL X" value={g(t?.accel[0])} />
          <StatBox label="ACCEL Y" value={g(t?.accel[1])} />
          <StatBox label="ACCEL Z" value={g(t?.accel[2])} />
          <StatBox label="GYRO X" value={dps(t?.gyro[0])} />
          <StatBox label="GYRO Y" value={dps(t?.gyro[1])} />
          <StatBox label="GYRO Z" value={dps(t?.gyro[2])} />
        </StatGrid>
      </Section>

      <Section title="Procedure">
        <ol className="steps steps-numbered">
          <li>Place the glove flat on a table.</li>
          <li>Hold still for 3 seconds.</li>
          <li>Tap Calibrate IMU below.</li>
        </ol>
        {st && (
          <>
            <Row label={opcodeLabel(st.lastOpcode)} value={statusLabel(st.state)} />
            {running && (
              <div className="progress">
                <div
                  className="progress-fill"
                  style={{ width: `${st.progress}%` }}
                />
              </div>
            )}
          </>
        )}
        <div className="card-actions">
          <button
            className="btn btn-primary"
            disabled={running}
            onClick={() => void store.sendCommand(Command.CalibrateImu)}
          >
            Calibrate IMU
          </button>
          <button
            className="btn btn-secondary"
            disabled={running}
            onClick={() => void store.sendCommand(Command.RecalibrateTouch)}
          >
            Recalibrate touch baseline
          </button>
        </div>
      </Section>
    </Screen>
  );
}

function g(milliG?: number): string {
  if (milliG == null) return "—";
  return (milliG / 1000).toFixed(2);
}
function dps(milliDps?: number): string {
  if (milliDps == null) return "—";
  return (milliDps / 1000).toFixed(3);
}
function opcodeLabel(op: number): string {
  if (op === Command.CalibrateImu) return "IMU calibration";
  if (op === Command.RecalibrateTouch) return "Touch baseline";
  if (op === Command.SaveConfig) return "Save config";
  if (op === Command.FactoryReset) return "Factory reset";
  return `Command 0x${op.toString(16)}`;
}
function statusLabel(state: StatusState): string {
  switch (state) {
    case StatusState.Running:
      return "Running…";
    case StatusState.Success:
      return "Success";
    case StatusState.Fail:
      return "Failed";
    default:
      return "Idle";
  }
}
