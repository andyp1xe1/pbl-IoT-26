/**
 * Bipolar range slider centred on zero. Drag right = positive, left =
 * negative; zero is the "disabled" detent. Value is a signed integer in
 * milli units (matches the firmware wire format: ±2000 → ±2.0×).
 *
 * Visuals: a centre tick mark, an accent-tinted fill that grows from the
 * centre toward the thumb, and a compact value readout that renders "0"
 * in a muted tone so the user can scan a column of weights and see which
 * axes are actually contributing.
 */
export function BipolarSlider({
  value,
  onChange,
  min = -2000,
  max = +2000,
  step = 50,
  ariaLabel,
}: {
  value: number;
  onChange: (next: number) => void;
  min?: number;
  max?: number;
  step?: number;
  ariaLabel: string;
}) {
  const fillPct = (value / max) * 50;          // signed distance from centre
  const fillStart = Math.min(50, 50 + fillPct);
  const fillEnd = Math.max(50, 50 + fillPct);
  /* Sub-50 step means the lane is tuned in the 0.00x range (e.g. raw gyro
   * weights), so two decimals would round 0.005 to "0.01" and hide the
   * resolution the slider just gained. */
  const decimals = step < 50 ? 3 : 2;
  return (
    <div className={"bipolar" + (value === 0 ? " bipolar-zero" : "")}>
      <div
        className="bipolar-control"
        style={
          {
            "--bp-fill-start": `${fillStart}%`,
            "--bp-fill-end": `${fillEnd}%`,
          } as React.CSSProperties
        }
      >
        <div className="bipolar-track" aria-hidden="true">
          <span className="bipolar-fill" />
          <span className="bipolar-center" />
        </div>
        <input
          type="range"
          aria-label={ariaLabel}
          min={min}
          max={max}
          step={step}
          value={value}
          onChange={(e) => onChange(Number(e.target.value))}
          onDoubleClick={() => onChange(0)}
        />
      </div>
      <span className="bipolar-value">{formatMilli(value, decimals)}</span>
    </div>
  );
}

function formatMilli(v: number, decimals: number): string {
  if (v === 0) return "0";
  const sign = v > 0 ? "+" : "−";
  return `${sign}${(Math.abs(v) / 1000).toFixed(decimals)}`;
}
