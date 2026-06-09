/**
 * Live touch readout. The ESP32 touch driver reports `raw < threshold` as
 * touched (raw drops as capacitance rises; the button pads normalise
 * "open" to 4095). To make the bar match intuition — wider = more touched —
 * `invert` flips the fill direction and the active rule.
 */
export function TouchBar({
  label,
  value,
  max = 4095,
  threshold,
  invert = false,
}: {
  label: string;
  value: number;
  max?: number;
  threshold?: number;
  invert?: boolean;
}) {
  const norm = Math.min(1, Math.max(0, value / max));
  const fillPct = (invert ? 1 - norm : norm) * 100;
  const threshPct =
    threshold === undefined
      ? 0
      : (invert ? 1 - threshold / max : threshold / max) * 100;
  const active =
    threshold !== undefined && (invert ? value < threshold : value >= threshold);
  return (
    <div className="touch-row">
      <span className="touch-label">{label}</span>
      <div className="touch-track">
        <div
          className={`touch-fill${active ? " touch-fill-active" : ""}`}
          style={{ width: `${fillPct}%` }}
        />
        {threshold !== undefined && (
          <div
            className="touch-threshold"
            style={{ left: `${Math.min(100, Math.max(0, threshPct))}%` }}
          />
        )}
      </div>
      <span className="touch-value">{value}</span>
    </div>
  );
}
