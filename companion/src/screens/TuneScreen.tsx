import { useState } from "react";
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
  const scored = live > thresh;

  return (
    <>
      {/* ── Controls (dead zone + debounce) ── teammate addition */}
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
        </div>
      </section>

      {/* ── Motion mix (sensitivity / fusion) ── teammate addition */}
      <MotionMix
        cfg={cfg}
        onChange={(patch: Partial<AgConfig>) => store.updateConfigLocal(patch)}
        className="tune-mix"
      />

      {/* ── Buttons: live bars + per-pad threshold ── teammate addition.
           Note: use per-iteration lv/thr, not the selected-pad live/thresh. */}
      <Section title="Buttons" className="tune-buttons">
        <div className="buttons-head">
          <span />
          <span className="buttons-col-label">Live</span>
          <span className="buttons-col-label">Threshold</span>
          <span className="buttons-col-label">Action</span>
        </div>
        {PAD_NAMES.map((name, i) => {
          const lv = s.telemetry?.touch[i] ?? 0;
          const thr = cfg.touchThreshold[i];
          return (
            <div key={name} className="button-row">
              <span className="button-row-label">{name}</span>
              <TouchBar
                label=""
                value={lv}
                threshold={thr}
                max={TOUCH_BAR_MAX}
                invert
              />
              <div className="button-row-thresh">
                <input
                  type="range"
                  min={1}
                  max={TOUCH_BAR_MAX}
                  step={1}
                  value={thr}
                  aria-label={`${name} threshold`}
                  onChange={(e) => {
                    const arr = [
                      ...cfg.touchThreshold,
                    ] as typeof cfg.touchThreshold;
                    arr[i] = Number(e.target.value);
                    store.updateConfigLocal({ touchThreshold: arr });
                  }}
                />
                <span className="button-row-thresh-value">{thr}</span>
              </div>
              <ActionSelect
                value={cfg.clickAction[i]}
                options={PRIMARY_OPTIONS}
                onChange={(a) => {
                  const arr = [...cfg.clickAction] as typeof cfg.clickAction;
                  arr[i] = a;
                  store.updateConfigLocal({ clickAction: arr });
                }}
              />
            </div>
          );
        })}
      </Section>

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

              {/* Action grid — hidden with a notice when this finger IS the modifier */}
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
                    {altSlot >= 0 && (
                      <>
                        <div className="click-map-action-tabs">
                          <button
                            className={`click-map-action-tab${actionTab === "normal" ? " click-map-action-tab--active" : ""}`}
                            onClick={() => setActionTab("normal")}
                          >
                            Normal
                          </button>
                          <button
                            className={`click-map-action-tab${actionTab === "alt" ? " click-map-action-tab--active" : ""}`}
                            onClick={() => setActionTab("alt")}
                          >
                            While {PAD_NAMES[cfg.modifierPad]} held
                          </button>
                        </div>
                        <p className="click-map-modifier-hint">
                          {actionTab === "normal"
                            ? <>Tap <strong>{PAD_NAMES[selectedPad]}</strong> alone — {PAD_NAMES[cfg.modifierPad]} is not held.</>
                            : <>Hold <strong>{PAD_NAMES[cfg.modifierPad]}</strong>, then tap <strong>{PAD_NAMES[selectedPad]}</strong> to fire this action.</>
                          }
                        </p>
                      </>
                    )}

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
                <div className="click-map-thresh-header">
                  <span className="click-map-field-label">
                    Touch threshold
                  </span>
                  <span className="click-map-thresh-value">{thresh}</span>
                </div>
                <input
                  type="range"
                  className="click-map-thresh-slider"
                  min={1}
                  max={TOUCH_BAR_MAX}
                  step={1}
                  value={thresh}
                  onChange={(e) => {
                    const arr = [
                      ...cfg.touchThreshold,
                    ] as typeof cfg.touchThreshold;
                    arr[selectedPad] = Number(e.target.value);
                    store.updateConfigLocal({ touchThreshold: arr });
                  }}
                />
                <span
                  className={`click-map-thresh-live${scored ? " click-map-thresh-live--scored" : ""}`}
                >
                  Live reading {live} —{" "}
                  {scored ? "above threshold, registering as press" : "below threshold"}
                </span>
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
    </>
  );
}

function ActionSelect({
  value,
  options,
  onChange,
}: {
  value: ClickAction;
  options: ClickAction[];
  onChange: (a: ClickAction) => void;
}) {
  return (
    <select
      className="action-select"
      value={value}
      onChange={(e) => onChange(Number(e.target.value) as ClickAction)}
    >
      {options.map((a) => (
        <option key={a} value={a}>
          {CLICK_ACTION_LABELS[a]}
        </option>
      ))}
    </select>
  );
}

// Kept for any future use; suppresses unused-export lint without removing
export { ActionSelect };
