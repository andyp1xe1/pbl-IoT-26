import {
  ALT_FORBIDDEN,
  CLICK_ACTION_LABELS,
  CONFIG_FLAG_DIRTY,
  ClickAction,
  NO_MODIFIER,
  PAD_NAMES,
} from "../ble/types";
import { MotionMix } from "../ui/MotionMix";
import { Row, Section } from "../ui/Section";
import { Slider } from "../ui/Slider";
import { TouchBar } from "../ui/TouchBar";
import { WorkScreen } from "../ui/WorkScreen";
import { store, useAppState } from "../state/store";

const CAPTION = "Adjust sensitivity, touch thresholds, and click mappings.";

/* Cap-pad raw counts sit in the 0–~200 band (idle ~50–100, finger contact
 * pulls toward zero). The legacy 4095 ceiling came from the swing-button
 * thumb assumption; with cap pads everywhere the bar saturated and the
 * threshold marker pinned to the right edge. 300 leaves headroom for noisy
 * baselines without throwing away resolution. */
const TOUCH_BAR_MAX = 300;

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

  // Non-modifier pad indices, in the order their alt slot is stored.
  const altPads =
    cfg.modifierPad === NO_MODIFIER
      ? []
      : [0, 1, 2, 3].filter((p) => p !== cfg.modifierPad);

  return (
    <>
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

      <MotionMix
        cfg={cfg}
        onChange={(patch) => store.updateConfigLocal(patch)}
        className="tune-mix"
      />

      <Section title="Buttons" className="tune-buttons">
        <div className="buttons-head">
          <span />
          <span className="buttons-col-label">Live</span>
          <span className="buttons-col-label">Threshold</span>
          <span className="buttons-col-label">Action</span>
        </div>
        {PAD_NAMES.map((name, i) => {
          const live = s.telemetry?.touch[i] ?? 0;
          const thresh = cfg.touchThreshold[i];
          return (
            <div key={name} className="button-row">
              <span className="button-row-label">{name}</span>
              <TouchBar
                label=""
                value={live}
                threshold={thresh}
                max={TOUCH_BAR_MAX}
                invert
              />
              <div className="button-row-thresh">
                <input
                  type="range"
                  min={1}
                  max={TOUCH_BAR_MAX}
                  step={1}
                  value={thresh}
                  aria-label={`${name} threshold`}
                  onChange={(e) => {
                    const arr = [
                      ...cfg.touchThreshold,
                    ] as typeof cfg.touchThreshold;
                    arr[i] = Number(e.target.value);
                    store.updateConfigLocal({ touchThreshold: arr });
                  }}
                />
                <span className="button-row-thresh-value">{thresh}</span>
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

        <Row
          label="Modifier finger"
          value={
            <select
              className="action-select"
              value={cfg.modifierPad === NO_MODIFIER ? "none" : String(cfg.modifierPad)}
              onChange={(e) => {
                const v = e.target.value === "none" ? NO_MODIFIER : Number(e.target.value);
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

        {altPads.map((pad, slot) => (
          <Row
            key={`alt-${pad}`}
            label={`${PAD_NAMES[pad]} (while modifier held)`}
            value={
              <ActionSelect
                value={cfg.clickActionAlt[slot]}
                options={ALT_OPTIONS}
                onChange={(a) => {
                  const arr = [...cfg.clickActionAlt] as typeof cfg.clickActionAlt;
                  arr[slot] = a;
                  store.updateConfigLocal({ clickActionAlt: arr });
                }}
              />
            }
          />
        ))}

        <div className="card-actions">
          <button
            className="btn btn-primary"
            disabled={!dirty}
            onClick={() => void store.save()}
          >
            {dirty ? "Save to device" : "Saved"}
          </button>
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
