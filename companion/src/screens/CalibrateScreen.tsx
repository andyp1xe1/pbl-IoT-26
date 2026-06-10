import { Command, StatusState } from "../ble/types";
import { Section } from "../ui/Section";
import { StatBox, StatGrid } from "../ui/StatBox";
import { WorkScreen } from "../ui/WorkScreen";
import { store, useAppState } from "../state/store";

const CAPTION =
  "Inspect live IMU readings and calibrate gyro bias or touch baselines.";

export function CalibrateScreen() {
  return (
    <WorkScreen title="Calibrate" caption={CAPTION}>
      <CalibrateBody />
    </WorkScreen>
  );
}

function CalibrateBody() {
  const s = useAppState();
  const t = s.telemetry;
  const st = s.lastStatus;
  const running = st?.state === StatusState.Running;

  return (
    <>
      <Section title="Procedure">
        <ol className="steps cal-steps">
          <li>Place the glove flat on a table.</li>
          <li>Hold still for 3 seconds.</li>
          <li>Tap Calibrate IMU below.</li>
        </ol>
        {st && (
          <div className="cal-status-row">
            <span className="cal-status-label">{opcodeLabel(st.lastOpcode)}</span>
            <span className={`cal-status-badge cal-status-badge--${badgeVariant(st.state)}`}>
              {statusLabel(st.state)}
            </span>
          </div>
        )}
        {running && (
          <div className="progress">
            <div
              className="progress-fill"
              style={{ width: `${st!.progress}%` }}
            />
          </div>
        )}
        <div className="card-actions">
          <div className="cal-action-group">
            <button
              className="btn btn-primary"
              disabled={running}
              onClick={() => void store.sendCommand(Command.CalibrateImu)}
            >
              Calibrate IMU
            </button>
            <p className="cal-hint">
              Use when the cursor drifts slowly while the glove is flat on the
              table — that's the gyro zero-rate offset accumulating.
            </p>
          </div>
          <div className="cal-action-group">
            <button
              className="btn btn-secondary"
              disabled={running}
              onClick={() => void store.sendCommand(Command.RecalibrateTouch)}
            >
              Recalibrate touch baseline
            </button>
            <p className="cal-hint">
              Use when touch detection becomes unreliable after putting the
              glove on, or after temperature changes affect the thumb pad.
            </p>
          </div>
        </div>
      </Section>

      <Section
        title="IMU — live"
        action={<span className="hz-badge">{s.telemetryHz} Hz</span>}
      >
        <StatGrid>
          <StatBox label="ACCEL X" value={g(t?.accel[0])} />
          <StatBox label="ACCEL Y" value={g(t?.accel[1])} />
          <StatBox label="ACCEL Z" value={g(t?.accel[2])} />
          <StatBox label="GYRO X" value={dps(t?.gyro[0])} />
          <StatBox label="GYRO Y" value={dps(t?.gyro[1])} />
          <StatBox label="GYRO Z" value={dps(t?.gyro[2])} />
        </StatGrid>
      </Section>
    </>
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
    case StatusState.Running: return "Running…";
    case StatusState.Success: return "Success";
    case StatusState.Fail:    return "Failed";
    default:                  return "Idle";
  }
}

function badgeVariant(state: StatusState): string {
  switch (state) {
    case StatusState.Running: return "running";
    case StatusState.Success: return "success";
    case StatusState.Fail:    return "fail";
    default:                  return "idle";
  }
}
