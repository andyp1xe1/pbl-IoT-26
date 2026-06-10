import { AgConfig, MIX_AXIS_LABELS, MixAxis, MixVector } from "../ble/types";
import { BipolarSlider } from "./BipolarSlider";
import { Switch } from "./Switch";

/**
 * Motion-mix configurator. Hero card on the Tune screen — owns the
 * per-axis cursor weights plus the fusion controls (β + Madgwick
 * toggle) that govern the bottom band. β and toggle live in the card
 * header so the whole "how fusion works" story sits in one place.
 *
 * The 9 axes are grouped into Raw IMU / Fused bands; the fused band
 * dims when Madgwick is off, but its weights are preserved so toggling
 * back on restores the previous mapping.
 */
export function MotionMix({
  cfg,
  onChange,
  className,
}: {
  cfg: AgConfig;
  onChange: (patch: Partial<AgConfig>) => void;
  className?: string;
}) {
  const setWeight = (axis: "x" | "y", i: number, v: number) => {
    const key = axis === "x" ? "mixX" : "mixY";
    const next = [...cfg[key]] as MixVector;
    next[i] = v;
    onChange({ [key]: next } as Partial<AgConfig>);
  };

  const rawAxes: MixAxis[] = [MixAxis.Gx, MixAxis.Gy, MixAxis.Gz, MixAxis.Ax, MixAxis.Ay, MixAxis.Az];
  const fusedAxes: MixAxis[] = [MixAxis.Roll, MixAxis.Pitch, MixAxis.Yaw];
  /* Per-lane slider ranges, sized to each signal's natural operating window
   * so one slider step is a comparable change across lanes.
   *
   *   Gyro X/Y/Z  — rad/s, wrist peak ~3–5. ±0.5 weight gives ±2.5 contribution.
   *   Accel X/Y/Z — m/s² with a ~9.8 gravity DC. Range left at the legacy ±2.0
   *                 until linear-accel (gravity subtraction) lands; current
   *                 lanes are usable only for spike-style mixing.
   *   Fused rates — rad per 10 ms frame, peak ~0.05. ±2.0 weight gives ±0.1. */
  const RAW_GYRO_RANGE  = { min: -500,  max:  500,  step: 5  };
  const ACCEL_RANGE     = { min: -2000, max:  2000, step: 50 };
  const FUSED_RANGE     = { min: -2000, max:  2000, step: 50 };
  const rangeFor = (axis: MixAxis) => {
    switch (axis) {
      case MixAxis.Gx:
      case MixAxis.Gy:
      case MixAxis.Gz:
        return RAW_GYRO_RANGE;
      case MixAxis.Ax:
      case MixAxis.Ay:
      case MixAxis.Az:
        return ACCEL_RANGE;
      default:
        return FUSED_RANGE;
    }
  };
  const betaPct = (cfg.madgwickBetaMilli / 300) * 100;

  return (
    <section className={className ? `section motion-mix ${className}` : "section motion-mix"}>
      <div className="section-head">
        <h2 className="section-title">Motion mix</h2>
      </div>
      <div className="card mix-card">
        <header className="mix-header">
          <div className="mix-beta-inline">
            <span className="mix-beta-label">Madgwick β</span>
            <input
              type="range"
              min={0}
              max={300}
              step={1}
              value={cfg.madgwickBetaMilli}
              disabled={!cfg.madgwickEnabled}
              onChange={(e) => onChange({ madgwickBetaMilli: Number(e.target.value) })}
              style={{ ["--beta-pct" as string]: `${betaPct}%` }}
            />
            <span className="mix-beta-value">
              {(cfg.madgwickBetaMilli / 1000).toFixed(3)}
            </span>
          </div>
          <label className="motion-mix-toggle">
            <span>Madgwick fusion</span>
            <Switch
              label="Madgwick fusion"
              checked={cfg.madgwickEnabled}
              onChange={(next) => onChange({ madgwickEnabled: next })}
            />
          </label>
        </header>

        <div className="mix-grid">
          <div className="mix-grid-head">
            <span className="mix-group-label">Raw IMU</span>
            <span className="mix-col-label">→ Cursor X</span>
            <span className="mix-col-label">→ Cursor Y</span>
          </div>
          {rawAxes.map((axis) => (
            <MixRow
              key={axis}
              axis={axis}
              x={cfg.mixX[axis]}
              y={cfg.mixY[axis]}
              range={rangeFor(axis)}
              onChangeX={(v) => setWeight("x", axis, v)}
              onChangeY={(v) => setWeight("y", axis, v)}
            />
          ))}

          <div className="mix-band-sep" aria-hidden="true" />
          <div className="mix-grid-head mix-grid-head-sub">
            <span className="mix-group-label">
              Fused
              {!cfg.madgwickEnabled && <em className="mix-group-hint"> · disabled</em>}
            </span>
            <span />
            <span />
          </div>
          {fusedAxes.map((axis) => (
            <MixRow
              key={axis}
              axis={axis}
              x={cfg.mixX[axis]}
              y={cfg.mixY[axis]}
              range={rangeFor(axis)}
              dim={!cfg.madgwickEnabled}
              onChangeX={(v) => setWeight("x", axis, v)}
              onChangeY={(v) => setWeight("y", axis, v)}
            />
          ))}
        </div>
      </div>
    </section>
  );
}

function MixRow({
  axis,
  x,
  y,
  range,
  dim,
  onChangeX,
  onChangeY,
}: {
  axis: MixAxis;
  x: number;
  y: number;
  range: { min: number; max: number; step: number };
  dim?: boolean;
  onChangeX: (v: number) => void;
  onChangeY: (v: number) => void;
}) {
  const label = MIX_AXIS_LABELS[axis].replace(" (fused)", "");
  return (
    <div className={"mix-row" + (dim ? " mix-row-dim" : "")}>
      <span className="mix-axis-label">{label}</span>
      <BipolarSlider
        ariaLabel={`${label} contribution to cursor X`}
        value={x}
        onChange={onChangeX}
        min={range.min}
        max={range.max}
        step={range.step}
      />
      <BipolarSlider
        ariaLabel={`${label} contribution to cursor Y`}
        value={y}
        onChange={onChangeY}
        min={range.min}
        max={range.max}
        step={range.step}
      />
    </div>
  );
}
