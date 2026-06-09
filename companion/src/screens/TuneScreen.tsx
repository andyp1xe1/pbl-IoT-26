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

      <Section title="Motion mix" className="tune-mix">
        <MotionMix cfg={cfg} onChange={(patch) => store.updateConfigLocal(patch)} />
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
          const live = s.telemetry?.touch[i] ?? 0;
          const thresh = cfg.touchThreshold[i];
          return (
            <div key={name} className="touch-tune">
              <TouchBar
                label={name}
                value={live}
                threshold={thresh}
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
                  value={thresh}
                  onChange={(e) => {
                    const arr = [
                      ...cfg.touchThreshold,
                    ] as typeof cfg.touchThreshold;
                    arr[i] = Number(e.target.value);
                    store.updateConfigLocal({ touchThreshold: arr });
                  }}
                />
                <span className="touch-tune-thresh-value">{thresh}</span>
              </div>
            </div>
          );
        })}
      </Section>

      <Section title="Click mapping" className="tune-click">
        {PAD_NAMES.map((name, i) => (
          <Row
            key={name}
            label={name}
            value={
              <ActionSelect
                value={cfg.clickAction[i]}
                options={PRIMARY_OPTIONS}
                onChange={(a) => {
                  const arr = [...cfg.clickAction] as typeof cfg.clickAction;
                  arr[i] = a;
                  store.updateConfigLocal({ clickAction: arr });
                }}
              />
            }
          />
        ))}

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
