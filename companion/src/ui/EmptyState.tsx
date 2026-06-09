import { ReactNode } from "react";
import { Artwork } from "./Artwork";

interface Props {
  title: string;
  caption?: ReactNode;
  action?: ReactNode;
  error?: string | null;
  tone?: "neutral" | "error";
}

/**
 * Polished empty / fallback view. Centers a blueprint-style line drawing
 * of the AirGlove MCU board above a short title, caption, and stacked
 * actions. Used wherever the app has nothing real to display: no
 * connection yet, Web Bluetooth missing, device sleeping, no device info
 * available. Identical visual rhythm across every screen so transitions
 * between states never feel jarring.
 */
export function EmptyState({ title, caption, action, error, tone }: Props) {
  const actualTone = tone ?? (error ? "error" : "neutral");
  return (
    <div className={`empty-state empty-state-${actualTone}`}>
      <Artwork variant="empty" stateClass="" />
      <div className="empty-copy">
        <h3 className="empty-title">{title}</h3>
        {caption && <p className="empty-caption">{caption}</p>}
        {error && <p className="empty-error" role="alert">{error}</p>}
      </div>
      {action && <div className="empty-action">{action}</div>}
    </div>
  );
}
