import { CONFIG_FLAG_DIRTY } from "../ble/types";
import { Row, Section } from "../ui/Section";
import { Screen } from "../ui/Screen";
import { Slider } from "../ui/Slider";
import { TouchBar } from "../ui/TouchBar";
import { store, useAppState } from "../state/store";

const FINGERS = ["Thumb", "Index", "Middle", "Ring"];

export function TuneScreen() {
  const s = useAppState();
  const cfg = s.config;
  const connected = s.status === "connected";
  const dirty = s.configDirtyLocal || (cfg.flags & CONFIG_FLAG_DIRTY) !== 0;

  if (!connected) {
    return (
      <Screen title="Tune" subtitle="Adjust how the glove moves the cursor.">
        <Section footer="Connect to the glove from the Connect tab to adjust these settings.">
          <Row label="Not connected" />
        </Section>
      </Screen>
    );
  }

  return (
    <Screen title="Tune" subtitle="Adjust how the glove moves the cursor.">
      <Section title="Pointer">
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
          step={5}
          value={cfg.deadzoneMrad}
          display={`${(cfg.deadzoneMrad / 1000).toFixed(2)} rad`}
          onChange={(v) => store.updateConfigLocal({ deadzoneMrad: v })}
        />
      </Section>

      <Section title="Touch — live" footer="Red marker shows the click threshold.">
        {s.telemetry
          ? s.telemetry.touch.map((v, i) => (
              <TouchBar
                key={i}
                label={FINGERS[i]}
                value={v}
                threshold={i === 1 ? 500 : undefined}
              />
            ))
          : FINGERS.map((f) => <TouchBar key={f} label={f} value={0} />)}
      </Section>

      <Section title="Click mapping">
        <Row
          label="Index → Left, Middle → Right"
          value={<Radio checked={cfg.clickMap === 0} />}
          onClick={() => store.updateConfigLocal({ clickMap: 0 })}
        />
        <Row
          label="Index → Right, Middle → Left"
          value={<Radio checked={cfg.clickMap === 1} />}
          onClick={() => store.updateConfigLocal({ clickMap: 1 })}
        />
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
    </Screen>
  );
}

function Radio({ checked }: { checked: boolean }) {
  return <span className={`radio${checked ? " radio-on" : ""}`} />;
}
