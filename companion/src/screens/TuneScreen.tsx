import { useState } from "react";
import {
  ALT_FORBIDDEN,
  CLICK_ACTION_LABELS,
  CONFIG_FLAG_DIRTY,
  ClickAction,
  NO_MODIFIER,
  PAD_NAMES,
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
import { Row, Section } from "../ui/Section";
import { Slider } from "../ui/Slider";
import { TouchBar } from "../ui/TouchBar";
import { WorkScreen } from "../ui/WorkScreen";
import { HandMap3D } from "../ui/HandMap3D";
import { store, useAppState } from "../state/store";

const CAPTION = "Adjust sensitivity, touch thresholds, and click mappings.";

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
      <Section title="Pointer" className="tune-pointer">
        <Slider
          label="Sensitivity X"
          min={200}
          max={3000}
          step={10}
          value={cfg.sensXMilli}
          display={`${(cfg.sensXMilli / 1000).toFixed(2)}×`}
          onChange={(v) => store.updateConfigLocal({ sensXMilli: v })}
        />
        <Slider
          label="Sensitivity Y"
          min={200}
          max={3000}
          step={10}
          value={cfg.sensYMilli}
          display={`${(cfg.sensYMilli / 1000).toFixed(2)}×`}
          onChange={(v) => store.updateConfigLocal({ sensYMilli: v })}
        />
        <Slider
          label="Dead zone"
          min={0}
          max={300}
          step={1}
          value={cfg.deadzoneMrad}
          display={`${(cfg.deadzoneMrad / 1000).toFixed(3)} rad`}
          onChange={(v) => store.updateConfigLocal({ deadzoneMrad: v })}
        />
      </Section>

      <Section title="Fusion & input" className="tune-fusion">
        <Slider
          label="Madgwick β"
          min={0}
          max={300}
          step={1}
          value={cfg.madgwickBetaMilli}
          display={`${(cfg.madgwickBetaMilli / 1000).toFixed(3)}`}
          onChange={(v) => store.updateConfigLocal({ madgwickBetaMilli: v })}
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
      </Section>

      <Section title="Touch — live & thresholds" className="tune-touch">
        {PAD_NAMES.map((name, i) => {
          const lv = s.telemetry?.touch[i] ?? 0;
          const thr = cfg.touchThreshold[i];
          return (
            <div key={name} className="touch-tune">
              <TouchBar
                label={name}
                value={lv}
                threshold={thr}
                max={4095}
                invert
              />
              <div className="touch-tune-thresh">
                <span className="touch-tune-thresh-label">Threshold</span>
                <input
                  type="range"
                  min={1}
                  max={4095}
                  step={10}
                  value={thr}
                  onChange={(e) => {
                    const arr = [
                      ...cfg.touchThreshold,
                    ] as typeof cfg.touchThreshold;
                    arr[i] = Number(e.target.value);
                    store.updateConfigLocal({ touchThreshold: arr });
                  }}
                />
                <span className="touch-tune-thresh-value">{thr}</span>
              </div>
            </div>
          );
        })}
      </Section>

      <Section title="Click mapping" className="tune-click">
        <div className="click-map-layout">
          {/* ── Left: 3D hand picker ── */}
          <HandMap3D
            selected={selectedPad}
            onSelect={setSelectedPad}
            touch={liveTouchTuple}
            thresholds={threshTuple}
            actions={cfg.clickAction}
          />

          {/* ── Right: per-finger detail panel ── */}
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
              <div className="click-map-field">
                {/* Tab strip — only shown when a modifier finger is set */}
                {altSlot >= 0 && (
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
                )}

                {/* Single action grid — content switches with tab */}
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
              </div>

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
                  max={4095}
                  step={10}
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

            <Row
              label="Modifier finger"
              value={
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
              }
            />

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
