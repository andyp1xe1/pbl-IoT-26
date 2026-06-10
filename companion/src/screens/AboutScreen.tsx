import { GitFork, ExternalLink } from "lucide-react";
import { Section } from "../ui/Section";
import { WorkScreen } from "../ui/WorkScreen";
import { store, useAppState } from "../state/store";

const APP_VERSION = "0.1.0";

const TEAM = [
  "Cobzari Ion",
  "Crudu Alexandra",
  "Chicu Andrei",
  "Gurduza Mihai",
  "Moraru Patricia",
];

function initials(name: string) {
  return name
    .split(" ")
    .map((p) => p[0])
    .join("")
    .toUpperCase()
    .slice(0, 2);
}

export function AboutScreen() {
  return (
    <WorkScreen title="About">
      <AboutBody />
    </WorkScreen>
  );
}

function AboutBody() {
  const info = useAppState().deviceInfo;
  return (
    <div className="about-layout">
      {/* ── Hero ── */}
      <div className="about-hero">
        <div className="about-hero-name">AirGlove</div>
        <span className="about-hero-version">v{APP_VERSION}</span>
        <p className="about-hero-desc">
          A gesture-driven wireless mouse built from an ESP32 glove with a
          6-axis IMU and capacitive touch. Tilt your hand to move the cursor,
          tap your fingers to click.
        </p>
      </div>

      {/* ── Team ── */}
      <Section title="Built by" className="about-section-team">
        <div className="about-team-grid">
          {TEAM.map((name) => (
            <div key={name} className="about-member-card">
              <div className="about-member-avatar">{initials(name)}</div>
              <span className="about-member-name">{name}</span>
            </div>
          ))}
        </div>
      </Section>

      {/* ── Device info + factory reset ── */}
      <Section title="Device" className="about-section-device">
        <div className="about-device-grid">
          <div className="about-device-row">
            <span className="about-device-label">Manufacturer</span>
            <span className="about-device-value">{info?.manufacturer ?? "—"}</span>
          </div>
          <div className="about-device-row">
            <span className="about-device-label">Model</span>
            <span className="about-device-value">{info?.model ?? "—"}</span>
          </div>
          <div className="about-device-row">
            <span className="about-device-label">Firmware</span>
            <span className="about-device-value">{info?.firmware ?? "—"}</span>
          </div>
          <button
            className="about-device-reset"
            onClick={() => void store.factoryReset()}
          >
            Factory reset
          </button>
        </div>
      </Section>

      {/* ── Links ── */}
      <Section title="Links" className="about-section-links">
        <div className="about-links-grid">
          <a
            className="about-link-card"
            href="https://github.com/andyp1xe1/pbl-IoT-26"
            target="_blank"
            rel="noopener noreferrer"
          >
            <span className="about-link-card-icon">
              <GitFork size={20} strokeWidth={2} />
            </span>
            <span className="about-link-card-text">
              <span className="about-link-card-label">Source code</span>
              <span className="about-link-card-url">github.com</span>
            </span>
          </a>
          <a
            className="about-link-card"
            href="https://airglove.chillguys.studio/"
            target="_blank"
            rel="noopener noreferrer"
          >
            <span className="about-link-card-icon">
              <ExternalLink size={20} strokeWidth={2} />
            </span>
            <span className="about-link-card-text">
              <span className="about-link-card-label">Project site</span>
              <span className="about-link-card-url">airglove.chillguys.studio</span>
            </span>
          </a>
        </div>
      </Section>
    </div>
  );
}
