export type TabId = "device" | "tune" | "calibrate" | "about";

const TABS: { id: TabId; label: string; icon: string }[] = [
  { id: "device", label: "Home", icon: "home" },
  { id: "tune", label: "Tune", icon: "tune" },
  { id: "calibrate", label: "Calibrate", icon: "target" },
  { id: "about", label: "About", icon: "info" },
];

/**
 * App-wide navigation. The "device" tab is always interactive — it owns
 * the connection + power switches and is the home base when the glove
 * isn't available. All other tabs are *locked* (visually dimmed + clicks
 * suppressed) whenever `lockedTabs` includes them; the selectCanNavigate
 * selector in state/selectors.ts is the single source of truth for which
 * tabs are locked.
 */
export function Nav({
  active,
  onChange,
  variant,
  lockedTabs,
}: {
  active: TabId;
  onChange: (t: TabId) => void;
  variant: "side" | "bottom";
  lockedTabs: ReadonlySet<TabId>;
}) {
  return (
    <nav className={`nav nav-${variant}`}>
      {TABS.map((t) => {
        const locked = lockedTabs.has(t.id);
        const isActive = active === t.id;
        return (
          <button
            key={t.id}
            className={
              "nav-item" +
              (isActive ? " nav-item-active" : "") +
              (locked ? " nav-item-locked" : "")
            }
            aria-disabled={locked || undefined}
            aria-current={isActive ? "page" : undefined}
            onClick={() => !locked && onChange(t.id)}
          >
            <Icon name={t.icon} />
            <span>{t.label}</span>
          </button>
        );
      })}
    </nav>
  );
}

function Icon({ name }: { name: string }) {
  const paths: Record<string, JSX.Element> = {
    bluetooth: <path d="M7 7l10 10-5 5V2l5 5L7 17" fill="none" strokeWidth={2} />,
    home: (
      <g fill="none" strokeWidth={2}>
        <path d="M3 11l9-7 9 7v9a1 1 0 0 1-1 1h-5v-6h-6v6H4a1 1 0 0 1-1-1z" />
      </g>
    ),
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
