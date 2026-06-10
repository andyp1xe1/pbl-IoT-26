import { Command, PAD_NAMES, StatusState } from "../ble/types";
import { Section } from "../ui/Section";
import { StatBox, StatGrid } from "../ui/StatBox";
import { TouchBar } from "../ui/TouchBar";
import { WorkScreen } from "../ui/WorkScreen";
import { store, useAppState } from "../state/store";

/* Mirrors TUNE_BAR_MAX in TuneScreen — cap-pad raw counts sit in the
 * 0–~200 band, 300 leaves headroom for noisy baselines. */
const TOUCH_BAR_MAX = 300;

const CAPTION =
  "Inspect live IMU readings and calibrate gyro bias or touch baselines.";

export function CalibrateScreen() {
  return (
    <WorkScreen title="Calibrate" caption={CAPTION} gridClass="cal-layout">
      <CalibrateBody />
    </WorkScreen>
  );
}

function CalibrateBody() {
  const s = useAppState();
  const t = s.telemetry;

  return (
    <>
      <Section title="Procedure" className="cal-procedure">
        <ol className="steps cal-steps">
          <li>Place the glove flat on a table.</li>
          <li>Hold still for 3 seconds.</li>
          <li>Tap Calibrate IMU below.</li>
        </ol>
        <div className="card-actions">
          <CalAction
            opcode={Command.CalibrateImu}
            label="Calibrate IMU"
            runningLabel="Calibrating IMU"
            variant="primary"
            hint="Use when the cursor drifts slowly while the glove is flat on the table — that's the gyro zero-rate offset accumulating."
          />
          <CalAction
            opcode={Command.RecalibrateTouch}
            label="Recalibrate touch baseline"
            runningLabel="Sampling touch baseline"
            variant="secondary"
            hint="Use when touch detection becomes unreliable after putting the glove on, or after temperature changes affect the finger pads."
          />
        </div>
      </Section>

      <div className="cal-live-col">
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

        <Section title="Touch — live">
          <div className="cal-touch-grid">
            {PAD_NAMES.map((name, i) => (
              <FingerTouchPanel key={name} index={i} name={name} />
            ))}
          </div>
        </Section>
      </div>
    </>
  );
}

function FingerTouchPanel({ index, name }: { index: number; name: string }) {
  const s = useAppState();
  const live = s.telemetry?.touch[index] ?? 0;
  const thresh = s.config.touchThreshold[index];
  // Raw value drops toward 0 on finger contact — press registers when live < threshold.
  const scored = live < thresh;
  return (
    <div className="cal-touch-row">
      <span className="cal-touch-name">{name}</span>
      <TouchBar label="" value={live} threshold={thresh} max={TOUCH_BAR_MAX} invert />
      {scored && <span className="click-map-badge">active</span>}
    </div>
  );
}

/* Per-calibration button + progress + outcome. Driven by lastStatus from the
 * device: while RUNNING for *this* opcode the button label flips to a
 * "Sampling…" string and a thin fill grows below it; on SUCCESS/FAIL the
 * outcome line surfaces underneath the hint. Both buttons disable while any
 * calibration is in flight so a second one can't preempt the running one. */
function CalAction({
  opcode,
  label,
  runningLabel,
  variant,
  hint,
}: {
  opcode: Command;
  label: string;
  runningLabel: string;
  variant: "primary" | "secondary";
  hint: string;
}) {
  const st = useAppState().lastStatus;
  const matches = st?.lastOpcode === opcode;
  const anyRunning = st?.state === StatusState.Running;
  const isRunning = matches && anyRunning;
  const succeeded = matches && st?.state === StatusState.Success;
  const failed = matches && st?.state === StatusState.Fail;
  const pct = isRunning ? st?.progress ?? 0 : 0;

  return (
    <div className="cal-action-group">
      <button
        className={`btn btn-${variant}`}
        disabled={anyRunning}
        onClick={() => void store.sendCommand(opcode)}
      >
        {isRunning ? `${runningLabel}… ${pct}%` : label}
      </button>
      {isRunning && (
        <div className="cal-action-progress" aria-hidden="true">
          <div className="cal-action-progress-fill" style={{ width: `${pct}%` }} />
        </div>
      )}
      {succeeded && (
        <p className="cal-action-status cal-action-status--success">Calibration saved.</p>
      )}
      {failed && (
        <p className="cal-action-status cal-action-status--fail">Calibration failed — try again.</p>
      )}
      <p className="cal-hint">{hint}</p>
    </div>
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
