import { useAppState } from "../state/store";

export function ConnectionChip() {
  const s = useAppState();
  const tone =
    s.status === "connected"
      ? "good"
      : s.status === "connecting"
        ? "warn"
        : "off";
  const label =
    s.status === "connected"
      ? "Connected"
      : s.status === "connecting"
        ? "Connecting…"
        : "Offline";
  return (
    <div className={`conn-chip conn-${tone}`}>
      <span className="conn-dot" />
      <span className="conn-text">{label}</span>
    </div>
  );
}
