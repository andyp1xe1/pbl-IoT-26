export function StatGrid({ children }: { children: React.ReactNode }) {
  return <div className="stat-grid">{children}</div>;
}

export function StatBox({ label, value }: { label: string; value: string }) {
  return (
    <div className="stat-box">
      <span className="stat-label">{label}</span>
      <span className="stat-value">{value}</span>
    </div>
  );
}
