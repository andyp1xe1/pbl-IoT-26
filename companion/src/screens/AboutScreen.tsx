import { ExternalLink, GitFork } from "lucide-react";
import { WorkScreen } from "../ui/WorkScreen";
import ionAvatar from "../assets/pfp/ion.jpg";
import alexandraAvatar from "../assets/pfp/alexandra.jpg";
import andreiAvatar from "../assets/pfp/andrei.jpg";
import mihaiAvatar from "../assets/pfp/mihai.jpg";
import patriciaAvatar from "../assets/pfp/patricia.png";

const TEAM = [
  {
    name: "Chicu Andrei",
    role: "Companion app & BLE",
    avatar: andreiAvatar,
    github: "https://github.com/andyp1xe1",
  },
  {
    name: "Moraru Patricia",
    role: "Design & documentation",
    avatar: patriciaAvatar,
    github: "https://github.com/PatriciaMoraru",
  },
  {
    name: "Gurduza Mihai",
    role: "Hardware & enclosure",
    avatar: mihaiAvatar,
    github: "https://github.com/m33ga",
  },
  {
    name: "Crudu Alexandra",
    role: "UX, research & validation",
    avatar: alexandraAvatar,
    github: "https://github.com/crudualexandra",
  },
  {
    name: "Cobzari Ion",
    role: "Firmware & integration",
    avatar: ionAvatar,
    github: "https://github.com/Johnny-C-05",
  },
];

/* Caption is rendered only by WorkScreen's disconnected/sleeping placeholders,
 * not on the connected About view. */
const CAPTION = "The team and references behind AirGlove.";

export function AboutScreen() {
  return (
    <WorkScreen title="About" caption={CAPTION}>
      <AboutBody />
    </WorkScreen>
  );
}

function AboutBody() {
  return (
    <div className="about-layout">
      <section className="about-hero" aria-label="AirGlove overview">
        <p className="about-hero-desc">
          <strong>AirGlove</strong> is a wearable wireless mouse: move the cursor
          by tilting your hand and trigger clicks with finger pads. Built as a
          small, practical bridge between embedded sensing and everyday
          interaction.
        </p>
      </section>

      <div className="about-team-cloud">
        {TEAM.map((member, i) => (
          <a
            key={member.name}
            className={`about-team-orb about-team-orb-${i + 1}`}
            href={member.github}
            target="_blank"
            rel="noopener noreferrer"
            title={`${member.name} — ${member.role}`}
          >
            <span className="about-team-orb-avatar-wrap">
              <img className="about-team-orb-avatar" src={member.avatar} alt={member.name} />
            </span>
            <span className="about-team-orb-name">{member.name}</span>
          </a>
        ))}
      </div>

      <div className="about-link-cloud">
        <a
          className="about-link-orb"
          href="https://github.com/andyp1xe1/pbl-IoT-26"
          target="_blank"
          rel="noopener noreferrer"
          title="github.com/andyp1xe1/pbl-IoT-26"
        >
          <span className="about-link-orb-icon">
            <GitFork size={28} strokeWidth={2} />
          </span>
          <span className="about-link-orb-name">Source code</span>
        </a>
        <a
          className="about-link-orb"
          href="https://airglove.chillguys.studio/"
          target="_blank"
          rel="noopener noreferrer"
          title="airglove.chillguys.studio"
        >
          <span className="about-link-orb-icon">
            <ExternalLink size={28} strokeWidth={2} />
          </span>
          <span className="about-link-orb-name">Project site</span>
        </a>
      </div>
    </div>
  );
}
