import { ReactNode } from "react";

interface Props {
  title: string;
  caption?: ReactNode;
  action?: ReactNode;
  error?: string | null;
  tone?: "neutral" | "error";
}

/**
 * Polished empty / fallback view. Centers a hand-drawn schematic of the
 * glove (4 touch pads + IMU, wired together) above a short message.
 * Used wherever the app has nothing real to display: no connection yet,
 * Web Bluetooth missing, no device info available.
 */
export function EmptyState({ title, caption, action, error, tone }: Props) {
  const actualTone = tone ?? (error ? "error" : "neutral");
  return (
    <div className={`empty-state empty-state-${actualTone}`}>
      <GloveSchematic />
      <h3 className="empty-title">{title}</h3>
      {caption && <p className="empty-caption">{caption}</p>}
      {error && <p className="empty-error" role="alert">{error}</p>}
      {action && <div className="empty-action">{action}</div>}
    </div>
  );
}

/** A schematic of the glove: thumb (T) lower-left, index/middle/ring (I, M, R)
 *  across the top, IMU board at the centre, dashed lines for the wires. */
function GloveSchematic() {
  return (
    <svg
      viewBox="0 0 280 240"
      className="empty-art"
      role="img"
      aria-label="Air Glove schematic"
    >
      {/* Wires from IMU → each pad. Drawn first so the pads sit on top. */}
      <g className="empty-art-wire">
        <path d="M 140 150 Q 100 140 75 115" />
        <path d="M 140 150 Q 130 115 110 75" />
        <path d="M 140 150 Q 150 110 155 70" />
        <path d="M 140 150 Q 175 110 205 80" />
      </g>

      {/* IMU board */}
      <g className="empty-art-imu">
        <rect x="108" y="135" width="64" height="44" rx="6" />
        {/* mounting holes */}
        <circle cx="116" cy="143" r="1.8" />
        <circle cx="164" cy="143" r="1.8" />
        <circle cx="116" cy="171" r="1.8" />
        <circle cx="164" cy="171" r="1.8" />
        {/* chip outline */}
        <rect x="126" y="151" width="28" height="12" rx="1" />
        <text x="140" y="200" textAnchor="middle" className="empty-art-label">
          IMU
        </text>
      </g>

      {/* Four pads — thumb separate, others in a fingertip-like arc */}
      <Pad cx={75} cy={115} label="T" />
      <Pad cx={110} cy={60} label="I" />
      <Pad cx={155} cy={50} label="M" />
      <Pad cx={205} cy={65} label="R" />

      {/* Wrist indicator below the IMU */}
      <g className="empty-art-wrist">
        <line x1="108" y1="216" x2="172" y2="216" />
        <line x1="118" y1="222" x2="162" y2="222" />
      </g>
    </svg>
  );
}

function Pad({ cx, cy, label }: { cx: number; cy: number; label: string }) {
  return (
    <g className="empty-art-pad">
      <circle cx={cx} cy={cy} r="16" />
      <text
        x={cx}
        y={cy + 3.5}
        textAnchor="middle"
        className="empty-art-pad-label"
      >
        {label}
      </text>
    </g>
  );
}
