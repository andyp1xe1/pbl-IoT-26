import React, { useCallback, useRef } from "react";

/* Rotary "microwave dial" — pointer-drag vertically (up = increase) over a
 * 160 px range traverses the full value span. Visually a circle with a tick
 * pointing to the current position; rotation sweeps -135° → +135° (270°). */
export function RotaryKnob({
  value,
  min,
  max,
  step = 1,
  ariaLabel,
  onChange,
}: {
  value: number;
  min: number;
  max: number;
  step?: number;
  ariaLabel: string;
  onChange: (v: number) => void;
}) {
  const dragRef = useRef<{ startY: number; startValue: number } | null>(null);

  const SWEEP = 270;
  const START = -135;
  const norm = max === min ? 0 : (value - min) / (max - min);
  const angle = START + norm * SWEEP;

  const PX_PER_RANGE = 160;

  const clamp = (v: number) => Math.max(min, Math.min(max, v));
  const snap = (v: number) => Math.round(v / step) * step;

  const handleMove = useCallback(
    (e: PointerEvent) => {
      const s = dragRef.current;
      if (!s) return;
      const dy = s.startY - e.clientY;
      const delta = (dy / PX_PER_RANGE) * (max - min);
      onChange(clamp(snap(s.startValue + delta)));
    },
    [min, max, step, onChange],
  );

  const handleUp = useCallback(() => {
    dragRef.current = null;
    window.removeEventListener("pointermove", handleMove);
    window.removeEventListener("pointerup", handleUp);
  }, [handleMove]);

  const handleDown = (e: React.PointerEvent) => {
    e.preventDefault();
    dragRef.current = { startY: e.clientY, startValue: value };
    window.addEventListener("pointermove", handleMove);
    window.addEventListener("pointerup", handleUp);
  };

  const handleKey = (e: React.KeyboardEvent) => {
    const big = step * 10;
    let next = value;
    if (e.key === "ArrowUp" || e.key === "ArrowRight") next = value + step;
    else if (e.key === "ArrowDown" || e.key === "ArrowLeft") next = value - step;
    else if (e.key === "PageUp") next = value + big;
    else if (e.key === "PageDown") next = value - big;
    else if (e.key === "Home") next = min;
    else if (e.key === "End") next = max;
    else return;
    e.preventDefault();
    onChange(clamp(snap(next)));
  };

  return (
    <div
      role="slider"
      aria-label={ariaLabel}
      aria-valuemin={min}
      aria-valuemax={max}
      aria-valuenow={value}
      tabIndex={0}
      className="rotary-knob"
      onPointerDown={handleDown}
      onKeyDown={handleKey}
      style={{ "--fill": norm } as React.CSSProperties}
    >
      <div
        className="rotary-knob-dial"
        style={{ "--angle": `${angle}deg` } as React.CSSProperties}
      >
        <span className="rotary-knob-tick" />
      </div>
    </div>
  );
}
