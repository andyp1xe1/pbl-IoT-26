export type TabId = "connect" | "tune" | "calibrate" | "about";

const TABS: { id: TabId; label: string; icon: string }[] = [
  { id: "connect", label: "Connect", icon: "bluetooth" },
  { id: "tune", label: "Tune", icon: "tune" },
  { id: "calibrate", label: "Calibrate", icon: "target" },
  { id: "about", label: "About", icon: "info" },
];

export function Nav({
  active,
  onChange,
  variant,
}: {
  active: TabId;
  onChange: (t: TabId) => void;
  variant: "side" | "bottom";
}) {
  return (
    <nav className={`nav nav-${variant}`}>
      {TABS.map((t) => (
        <button
          key={t.id}
          className={`nav-item${active === t.id ? " nav-item-active" : ""}`}
          onClick={() => onChange(t.id)}
        >
          <Icon name={t.icon} />
          <span>{t.label}</span>
        </button>
      ))}
    </nav>
  );
}

function Icon({ name }: { name: string }) {
  const paths: Record<string, JSX.Element> = {
    bluetooth: <path d="M7 7l10 10-5 5V2l5 5L7 17" fill="none" strokeWidth={2} />,
    tune: (
      <g fill="none" strokeWidth={2}>
        <path d="M4 6h10M18 6h2M4 12h2M10 12h10M4 18h12M20 18h0" />
        <circle cx={16} cy={6} r={2} />
        <circle cx={8} cy={12} r={2} />
        <circle cx={18} cy={18} r={2} />
      </g>
    ),
    target: (
      <g fill="none" strokeWidth={2}>
        <circle cx={12} cy={12} r={8} />
        <circle cx={12} cy={12} r={3} />
      </g>
    ),
    info: (
      <g fill="none" strokeWidth={2}>
        <circle cx={12} cy={12} r={9} />
        <path d="M12 11v5M12 8h0" />
      </g>
    ),
  };
  return (
    <svg
      viewBox="0 0 24 24"
      stroke="currentColor"
      strokeLinecap="round"
      strokeLinejoin="round"
    >
      {paths[name]}
    </svg>
  );
}
