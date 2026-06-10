import React, { useState } from "react";
import {
  ALT_FORBIDDEN,
  CLICK_ACTION_LABELS,
  CONFIG_FLAG_DIRTY,
  ClickAction,
  NO_MODIFIER,
  PAD_NAMES,
  type AgConfig,
} from "../ble/types";
import {
  Ban,
  MousePointerClick,
  Mouse,
  CircleDot,
  ArrowUp,
  ArrowDown,
  Globe,
  ArrowUpDown,
  type LucideIcon,
} from "lucide-react";
import { Section } from "../ui/Section";
import { MotionMix } from "../ui/MotionMix";
import { Slider } from "../ui/Slider";
import { RotaryKnob } from "../ui/RotaryKnob";
import { TouchBar } from "../ui/TouchBar";
import { WorkScreen } from "../ui/WorkScreen";
import { HandMap3D } from "../ui/HandMap3D";
import { store, useAppState } from "../state/store";

const CAPTION = "Adjust sensitivity, touch thresholds, and click mappings.";

/* Cap-pad raw counts sit in the 0–~200 band (idle ~50–100, finger contact
 * pulls toward zero). 300 leaves headroom for noisy baselines without
 * throwing away resolution. */
const TOUCH_BAR_MAX = 300;

const ACTION_ICON: Record<ClickAction, LucideIcon> = {
  [ClickAction.None]: Ban,
  [ClickAction.Left]: MousePointerClick,
  [ClickAction.Right]: Mouse,
  [ClickAction.Middle]: CircleDot,
  [ClickAction.ScrollUp]: ArrowUp,
  [ClickAction.ScrollDown]: ArrowDown,
  [ClickAction.Clutch]: Globe,
  [ClickAction.ScrollMode]: ArrowUpDown,
};

const PRIMARY_OPTIONS: ClickAction[] = [
  ClickAction.None,
  ClickAction.Left,
  ClickAction.Right,
  ClickAction.Middle,
  ClickAction.ScrollUp,
  ClickAction.ScrollDown,
  ClickAction.Clutch,
  ClickAction.ScrollMode,
];

const ALT_OPTIONS: ClickAction[] = PRIMARY_OPTIONS.filter(
  (a) => !ALT_FORBIDDEN.has(a),
);

export function TuneScreen() {
  return (
    <WorkScreen title="Tune" caption={CAPTION} gridClass="tune-layout">
      <TuneBody />
    </WorkScreen>
  );
}

function TuneBody() {
  const s = useAppState();
  const cfg = s.config;
  const dirty = s.configDirtyLocal || (cfg.flags & CONFIG_FLAG_DIRTY) !== 0;
  const [selectedPad, setSelectedPad] = useState(1);
  const [actionTab, setActionTab] = useState<"normal" | "alt">("normal");

  const altPads =
    cfg.modifierPad === NO_MODIFIER
      ? []
      : [0, 1, 2, 3].filter((p) => p !== cfg.modifierPad);

  // Alt-slot index for the selected pad (–1 if not in alt table)
  const altSlot = altPads.indexOf(selectedPad);

  const liveTouch = s.telemetry?.touch ?? [0, 0, 0, 0];
  const liveTouchTuple = liveTouch as [number, number, number, number];
  const threshTuple = cfg.touchThreshold as [number, number, number, number];
  const live = liveTouch[selectedPad] ?? 0;
  const thresh = cfg.touchThreshold[selectedPad];
  // Raw value drops toward 0 on finger contact — a press registers when live < threshold
  const scored = live < thresh;

  return (
    <>
      {/* ── Click mapping: 3D hand picker + per-finger detail ── our addition */}
      <Section title="Click mapping" className="tune-click">
        <div className="click-map-layout">
          <HandMap3D
            selected={selectedPad}
            onSelect={setSelectedPad}
            touch={liveTouchTuple}
            thresholds={threshTuple}
            actions={cfg.clickAction}
          />

          <div className="click-map-detail">
            <div className="click-map-detail-header">
              <span className="click-map-detail-name">
                {PAD_NAMES[selectedPad]} finger
              </span>
              {scored && (
                <span className="click-map-badge">active</span>
              )}
            </div>

            <div className="click-map-detail-body">
              {/* Modifier finger — above action so the tab strip it controls is right below */}
              <div className="click-map-field click-map-field--inline">
                <span className="click-map-field-label" style={{ marginBottom: 0 }}>
                  Modifier finger
                </span>
                <select
                  className="action-select"
                  value={
                    cfg.modifierPad === NO_MODIFIER
                      ? "none"
                      : String(cfg.modifierPad)
                  }
                  onChange={(e) => {
                    const v =
                      e.target.value === "none"
                        ? NO_MODIFIER
                        : Number(e.target.value);
                    setActionTab("normal");
                    store.updateConfigLocal({ modifierPad: v });
                  }}
                >
                  <option value="none">None</option>
                  {PAD_NAMES.map((name, i) => (
                    <option key={name} value={i}>
                      {name}
                    </option>
                  ))}
                </select>
              </div>

              {/* Action grid — hidden with a notice when this finger IS the modifier.
                  The tab strip + hint render in every non-modifier state so the
                  panel keeps the same height whether a modifier finger is set
                  or not; visibility (not display) is what's toggled. */}
              <div className="click-map-field">
                {selectedPad === cfg.modifierPad ? (
                  <div className="click-map-modifier-notice">
                    <span className="click-map-modifier-notice-icon">⇧</span>
                    <span>
                      <strong>{PAD_NAMES[selectedPad]}</strong> is the modifier finger.
                      It doesn't fire an action on its own — holding it changes
                      what the other fingers do.
                    </span>
                  </div>
                ) : (
                  <>
                    <div
                      className={`click-map-tabs-slot${altSlot < 0 ? " click-map-tabs-slot--disabled" : ""}`}
                    >
                      <div className="click-map-action-tabs">
                        <button
                          className={`click-map-action-tab${actionTab === "normal" ? " click-map-action-tab--active" : ""}`}
                          onClick={() => setActionTab("normal")}
                          disabled={altSlot < 0}
                        >
                          Normal
                        </button>
                        <button
                          className={`click-map-action-tab${actionTab === "alt" ? " click-map-action-tab--active" : ""}`}
                          onClick={() => setActionTab("alt")}
                          disabled={altSlot < 0}
                        >
                          {altSlot < 0
                            ? "While modifier held"
                            : `While ${PAD_NAMES[cfg.modifierPad]} held`}
                        </button>
                      </div>
                      <p className="click-map-modifier-hint">
                        {altSlot < 0 ? (
                          <>Set a modifier finger above to enable a second action.</>
                        ) : actionTab === "normal" ? (
                          <>Tap <strong>{PAD_NAMES[selectedPad]}</strong> alone — {PAD_NAMES[cfg.modifierPad]} is not held.</>
                        ) : (
                          <>Hold <strong>{PAD_NAMES[cfg.modifierPad]}</strong>, then tap <strong>{PAD_NAMES[selectedPad]}</strong> to fire this action.</>
                        )}
                      </p>
                    </div>

                    {(altSlot < 0 || actionTab === "normal") && (
                      <div className="click-map-action-grid">
                        {PRIMARY_OPTIONS.map((a) => {
                          const Icon = ACTION_ICON[a];
                          return (
                            <button
                              key={a}
                              className={`click-map-action-btn${cfg.clickAction[selectedPad] === a ? " click-map-action-btn--active" : ""}`}
                              onClick={() => {
                                const arr = [...cfg.clickAction] as typeof cfg.clickAction;
                                arr[selectedPad] = a;
                                store.updateConfigLocal({ clickAction: arr });
                              }}
                            >
                              <Icon className="click-map-action-icon" size={15} strokeWidth={2} />
                              {CLICK_ACTION_LABELS[a]}
                            </button>
                          );
                        })}
                      </div>
                    )}

                    {altSlot >= 0 && actionTab === "alt" && (
                      <div className="click-map-action-grid">
                        {ALT_OPTIONS.map((a) => {
                          const Icon = ACTION_ICON[a];
                          return (
                            <button
                              key={a}
                              className={`click-map-action-btn${cfg.clickActionAlt[altSlot] === a ? " click-map-action-btn--active" : ""}`}
                              onClick={() => {
                                const arr = [...cfg.clickActionAlt] as typeof cfg.clickActionAlt;
                                arr[altSlot] = a;
                                store.updateConfigLocal({ clickActionAlt: arr });
                              }}
                            >
                              <Icon className="click-map-action-icon" size={15} strokeWidth={2} />
                              {CLICK_ACTION_LABELS[a]}
                            </button>
                          );
                        })}
                      </div>
                    )}
                  </>
                )}
              </div>

              {/* Touch threshold — per-finger calibration */}
              <div className="click-map-field">
                <span className="click-map-field-label">Touch threshold</span>
                <div className="click-map-thresh-row">
                  <input
                    type="range"
                    className="click-map-thresh-slider"
                    min={1}
                    max={TOUCH_BAR_MAX}
                    step={1}
                    value={thresh}
                    style={{ "--pct": `${(thresh / TOUCH_BAR_MAX) * 100}%` } as React.CSSProperties}
                    onChange={(e) => {
                      const arr = [
                        ...cfg.touchThreshold,
                      ] as typeof cfg.touchThreshold;
                      arr[selectedPad] = Number(e.target.value);
                      store.updateConfigLocal({ touchThreshold: arr });
                    }}
                  />
                  <span className="click-map-thresh-value">{thresh}</span>
                </div>
                <TouchBar
                  label=""
                  value={live}
                  threshold={thresh}
                  max={TOUCH_BAR_MAX}
                  invert
                />
              </div>
            </div>

            <div className="click-map-actions">
              <button
                className="btn btn-primary"
                disabled={!dirty}
                onClick={() => void store.save()}
              >
                {dirty ? "Save to device" : "Saved"}
              </button>
            </div>
          </div>
        </div>
      </Section>

      {/* ── Controls (dead zone + debounce) ── */}
      <section className="section tune-controls">
        <div className="section-head">
          <h2 className="section-title">Controls</h2>
        </div>
        <div className="card tune-controls-card">
          <Slider
            label="Dead zone"
            min={0}
            max={300}
            step={1}
            value={cfg.deadzoneMrad}
            display={`${(cfg.deadzoneMrad / 1000).toFixed(3)} rad`}
            onChange={(v) => store.updateConfigLocal({ deadzoneMrad: v })}
          />
          <Slider
            label="Debounce"
            min={5}
            max={200}
            step={1}
            value={cfg.debounceMs}
            display={`${cfg.debounceMs} ms`}
            onChange={(v) => store.updateConfigLocal({ debounceMs: v })}
          />
          <div className="slider-row knob-row">
            <span className="row-label">Wrist-roll comp</span>
            <RotaryKnob
              ariaLabel="Wrist-roll compensation"
              value={cfg.wristRollCompMilli}
              min={0}
              max={1000}
              step={10}
              onChange={(v) => store.updateConfigLocal({ wristRollCompMilli: v })}
            />
            <span className="row-value">
              {(cfg.wristRollCompMilli / 1000).toFixed(2)}
            </span>
          </div>
        </div>
      </section>

      {/* ── Motion mix (sensitivity / fusion) ── */}
      <MotionMix
        cfg={cfg}
        onChange={(patch: Partial<AgConfig>) => store.updateConfigLocal(patch)}
        className="tune-mix"
      />
    </>
  );
}

