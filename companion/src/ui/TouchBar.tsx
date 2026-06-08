export function TouchBar({
  label,
  value,
  max = 1023,
  threshold,
}: {
  label: string;
  value: number;
  max?: number;
  threshold?: number;
}) {
  const pct = Math.min(100, (value / max) * 100);
  const active = threshold !== undefined && value >= threshold;
  return (
    <div className="touch-row">
      <span className="touch-label">{label}</span>
      <div className="touch-track">
        <div
          className={`touch-fill${active ? " touch-fill-active" : ""}`}
          style={{ width: `${pct}%` }}
        />
        {threshold !== undefined && (
          <div
            className="touch-threshold"
            style={{ left: `${Math.min(100, (threshold / max) * 100)}%` }}
          />
        )}
      </div>
      <span className="touch-value">{value}</span>
    </div>
  );
}
