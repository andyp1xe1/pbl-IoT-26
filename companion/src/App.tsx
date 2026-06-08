import { useState } from "react";
import { AboutScreen } from "./screens/AboutScreen";
import { CalibrateScreen } from "./screens/CalibrateScreen";
import { ConnectScreen } from "./screens/ConnectScreen";
import { TuneScreen } from "./screens/TuneScreen";
import { ConnectionChip } from "./ui/ConnectionChip";
import { Nav, TabId } from "./ui/TabBar";

export default function App() {
  const [tab, setTab] = useState<TabId>("connect");

  return (
    <div className="app">
      <aside className="sidebar">
        <div className="brand">
          <span className="brand-mark">AIR</span>
          <span className="brand-mark">GLOVE</span>
        </div>
        <Nav active={tab} onChange={setTab} variant="side" />
        <div className="sidebar-foot">
          <ConnectionChip />
        </div>
      </aside>

      <header className="topbar">
        <span className="brand-inline">AIR GLOVE</span>
        <ConnectionChip />
      </header>

      <main className="content">
        <div className="content-inner">
          {tab === "connect" && <ConnectScreen />}
          {tab === "tune" && <TuneScreen />}
          {tab === "calibrate" && <CalibrateScreen />}
          {tab === "about" && <AboutScreen />}
        </div>
      </main>

      <Nav active={tab} onChange={setTab} variant="bottom" />
    </div>
  );
}
