import { AgConfig, MIX_AXIS_COUNT, MIX_AXIS_LABELS, MixAxis, MixVector } from "../ble/types";
import { BipolarSlider } from "./BipolarSlider";
import { Switch } from "./Switch";

/**
 * Motion-mix configurator. One row per AG_MIX_* axis with two bipolar
 * sliders ("→ X" and "→ Y") that each encode contribution direction and
 * magnitude into a signed milli-weight. The math is:
 *
 *   dx_raw = Σ mix_x_milli[i] · signal[i] / 1000
 *   dy_raw = Σ mix_y_milli[i] · signal[i] / 1000
 *
 * The first six rows (raw gyro/accel) are always active. The last three
 * (fused roll/pitch/yaw rates) are only meaningful while the Madgwick
 * toggle is on — when off, their slider state is preserved but the rows
 * are dimmed to communicate that they currently multiply against zero.
 */
export function MotionMix({
  cfg,
  onChange,
}: {
  cfg: AgConfig;
  onChange: (patch: Partial<AgConfig>) => void;
}) {
  const setWeight = (axis: "x" | "y", i: number, v: number) => {
    const key = axis === "x" ? "mixX" : "mixY";
    const next = [...cfg[key]] as MixVector;
    next[i] = v;
    onChange({ [key]: next } as Partial<AgConfig>);
  };

  const rows: MixAxis[] = Array.from({ length: MIX_AXIS_COUNT }, (_, i) => i as MixAxis);
  const isFused = (a: MixAxis) =>
    a === MixAxis.Roll || a === MixAxis.Pitch || a === MixAxis.Yaw;

  return (
    <div className="motion-mix">
      <div className="motion-mix-head">
        <span className="row-label">Madgwick fusion</span>
        <Switch
          label="Madgwick fusion"
          checked={cfg.madgwickEnabled}
          onChange={(next) => onChange({ madgwickEnabled: next })}
        />
      </div>
      <p className="motion-mix-caption">
        Each axis contributes to cursor X and Y by its slider weight.
        Center = disabled. Drag right for positive, left for negative.
        Double-click a slider to reset to zero.
      </p>

      <div className="mix-grid">
        <div className="mix-grid-head">
          <span />
          <span className="mix-col-label">→ X</span>
          <span className="mix-col-label">→ Y</span>
        </div>
        {rows.map((axis) => {
          const dim = isFused(axis) && !cfg.madgwickEnabled;
          return (
            <div key={axis} className={"mix-row" + (dim ? " mix-row-dim" : "")}>
              <span className="mix-axis-label">{MIX_AXIS_LABELS[axis]}</span>
              <BipolarSlider
                ariaLabel={`${MIX_AXIS_LABELS[axis]} contribution to cursor X`}
                value={cfg.mixX[axis]}
                onChange={(v) => setWeight("x", axis, v)}
              />
              <BipolarSlider
                ariaLabel={`${MIX_AXIS_LABELS[axis]} contribution to cursor Y`}
                value={cfg.mixY[axis]}
                onChange={(v) => setWeight("y", axis, v)}
              />
            </div>
          );
        })}
      </div>
    </div>
  );
}
