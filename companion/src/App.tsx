import { useEffect } from "react";
import { AboutScreen } from "./screens/AboutScreen";
import { CalibrateScreen } from "./screens/CalibrateScreen";
import { DeviceScreen } from "./screens/DeviceScreen";
import { TuneScreen } from "./screens/TuneScreen";
import { Nav, TabId } from "./ui/TabBar";
import {
  selectActiveTab,
  selectCanNavigate,
} from "./state/selectors";
import { store, useAppState } from "./state/store";

/** Telemetry rate per tab. Both work-tab consumers (Tune touch bars,
 *  Calibrate live IMU read-outs) saturate the eye well below 20 Hz, so
 *  they ride NORMAL (~15 Hz). Home / About only consume the SLEEPING
 *  flag and battery, so idle. FAST (~20 Hz) stays defined for future
 *  consumers that genuinely need denser sampling (e.g. a live gesture
 *  recorder), but no current screen uses it. */
const TAB_TELEMETRY_RATE: Record<TabId, "idle" | "normal" | "fast"> = {
  device: "idle",
  about: "idle",
  tune: "normal",
  calibrate: "normal",
};

/** Tabs locked when the glove isn't linked + awake. Device is never locked. */
const WORK_TABS: ReadonlySet<TabId> = new Set(["tune", "calibrate", "about"]);
const EMPTY_LOCKS: ReadonlySet<TabId> = new Set();

export default function App() {
  const s = useAppState();
  const tab = selectActiveTab(s);
  const lockedTabs = selectCanNavigate(s) ? EMPTY_LOCKS : WORK_TABS;

  // Push the rate hint each time the active tab changes (and on first
  // connect, since `status` flipping to "connected" re-fires this effect
  // while `tab` stays the same → the firmware learns the desired rate
  // immediately rather than sitting at its 10 Hz default until a switch).
  useEffect(() => {
    if (s.status === "connected") {
      void store.setTelemetryRate(TAB_TELEMETRY_RATE[tab]);
    }
  }, [tab, s.status]);

  return (
    <div className="app">
      <aside className="sidebar">
        <div className="brand">
          <span className="brand-mark">AIR</span>
          <span className="brand-mark">GLOVE</span>
        </div>
        <Nav
          active={tab}
          onChange={store.setTab}
          variant="side"
          lockedTabs={lockedTabs}
        />
      </aside>

      <header className="topbar">
        <span className="brand-inline">AIR GLOVE</span>
      </header>

      <main className="content">
        <div className="content-inner">
          {tab === "device" && <DeviceScreen />}
          {tab === "tune" && <TuneScreen />}
          {tab === "calibrate" && <CalibrateScreen />}
          {tab === "about" && <AboutScreen />}
        </div>
      </main>

      <Nav
        active={tab}
        onChange={store.setTab}
        variant="bottom"
        lockedTabs={lockedTabs}
      />
    </div>
  );
}
