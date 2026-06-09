import { AboutScreen } from "./screens/AboutScreen";
import { CalibrateScreen } from "./screens/CalibrateScreen";
import { ConnectScreen } from "./screens/ConnectScreen";
import { TuneScreen } from "./screens/TuneScreen";
import { ConnectionChip } from "./ui/ConnectionChip";
import { Nav } from "./ui/TabBar";
import { store, useAppState } from "./state/store";

function AppVersion() {
  return (
    <span className="app-version" aria-label="App version">
      v{import.meta.env.VITE_APP_VERSION ?? "0.1.0"}
    </span>
  );
}

export default function App() {
  const tab = useAppState().tab;

  return (
    <div className="app">
      <aside className="sidebar">
        <div className="brand">
          <span className="brand-mark">AIR</span>
          <span className="brand-mark">GLOVE</span>
        </div>
        <Nav active={tab} onChange={store.setTab} variant="side" />
        <div className="sidebar-foot">
          <ConnectionChip />
          <AppVersion />
        </div>
      </aside>

      <header className="topbar">
        <span className="brand-inline">AIR GLOVE</span>
        <div className="topbar-right">
          <AppVersion />
          <ConnectionChip />
        </div>
      </header>

      <main className="content">
        <div className="content-inner">
          {tab === "connect" && <ConnectScreen />}
          {tab === "tune" && <TuneScreen />}
          {tab === "calibrate" && <CalibrateScreen />}
          {tab === "about" && <AboutScreen />}
        </div>
      </main>

      <Nav active={tab} onChange={store.setTab} variant="bottom" />
    </div>
  );
}
