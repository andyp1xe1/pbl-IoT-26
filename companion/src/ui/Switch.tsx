/**
 * GNOME-style pill switch. Off = muted track + circle on the left; on =
 * accent track + circle on the right. The track and circle slide smoothly
 * via CSS transition.
 *
 * Use `busy` for an in-flight state (e.g. connecting): the switch becomes
 * non-interactive and the track shows a subtle warning tint, so the user
 * knows their click was received and is being applied. Calling code is
 * free to commit the new state optimistically — when the underlying
 * truth updates the switch will animate to match without flicker.
 */
export function Switch({
  checked,
  onChange,
  disabled,
  busy,
  label,
}: {
  checked: boolean;
  onChange: (next: boolean) => void;
  disabled?: boolean;
  busy?: boolean;
  /** Accessible label — required because the visual switch has no caption. */
  label: string;
}) {
  const interactive = !disabled && !busy;
  return (
    <button
      type="button"
      role="switch"
      aria-checked={checked}
      aria-busy={busy || undefined}
      aria-label={label}
      disabled={!interactive}
      onClick={() => interactive && onChange(!checked)}
      className={
        "switch" +
        (checked ? " switch-on" : " switch-off") +
        (busy ? " switch-busy" : "") +
        (disabled ? " switch-disabled" : "")
      }
    >
      <span className="switch-thumb" aria-hidden="true" />
    </button>
  );
}
