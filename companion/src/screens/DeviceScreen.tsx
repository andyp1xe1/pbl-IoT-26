import type { ReactNode } from "react";
import { Artwork } from "../ui/Artwork";
import { Row, Section } from "../ui/Section";
import { Screen } from "../ui/Screen";
import { Switch } from "../ui/Switch";
import { store, useAppState } from "../state/store";
import { LinkState, selectLinkState } from "../state/selectors";

/**
 * Device screen — home base. Two switches drive everything:
 *   • Connection  toggles GATT link (off → reconnect/chooser, on → disconnect)
 *   • Power       toggles soft-sleep (visible only when linked)
 *
 * No "Connected"/"Disconnected" literal text — the switch state itself is
 * the indicator. A short status caption below the switches names what's
 * currently true (e.g. "Linked — mouse active", "Sleeping to save power").
 *
 * Device info rows show when the link is up so the user can confirm what
 * they're talking to. Disconnected/error state collapses to the same
 * centered layout with just the Connection switch + a hint.
 */
export function DeviceScreen() {
  const s = useAppState();
  const link = selectLinkState(s);

  if (link.kind === "unsupported") {
    return (
      <Screen title="Air Glove">
        <UnsupportedView />
      </Screen>
    );
  }

  // Paired iff the browser remembers a device OR we're currently linked
  // to one. Drives the screen's overall shape: paired state shows the
  // switches + forget + info card; not-paired state collapses everything
  // to a single "Pair device" call-to-action.
  const known = s.knownDevices[0] ?? null;
  const paired = known != null || link.kind === "linked";

  // Title is one of three states, in priority order:
  //   • model name from GATT Device Info → linked and fully introduced
  //   • "Connecting"                     → link in flight OR linked but
  //                                        DIS not yet read back. Avoids
  //                                        ever showing the advertised
  //                                        "AirGlove" name as a flash
  //                                        before the real model arrives.
  //   • "Welcome"                        → no link, no in-flight attempt
  let title: string;
  if (link.kind === "linked" && s.deviceInfo?.model) {
    title = s.deviceInfo.model;
  } else if (link.kind === "connecting" || link.kind === "linked") {
    title = "Connecting";
  } else {
    title = "Welcome";
  }

  return (
    <Screen title={title} gridClass="device-grid">
      <div className="device-stage">
        <Artwork variant="device" stateClass={artworkClass(link)} />
        {link.kind === "failed" && link.reason && (
          <p className="device-error" role="alert">
            {link.reason}
          </p>
        )}
        {paired ? (
          <PairedView link={link} known={known} deviceInfo={s.deviceInfo} />
        ) : (
          <UnpairedView connecting={link.kind === "connecting"} />
        )}
      </div>
    </Screen>
  );
}

/* ── State variants ───────────────────────────────────────────────────── *
 * Pairing is the orthogonal axis to connection: a paired device may be
 * disconnected (cards stay visible, switches off, info dashes), and a
 * connecting device is implicitly being paired (UI matches the paired
 * shape so the layout doesn't jump on success). The unpaired view is a
 * single CTA — nothing else to do until the chooser runs.                */

function PairedView({
  link,
  known,
  deviceInfo,
}: {
  link: LinkState;
  known: { id: string; name: string } | null;
  deviceInfo: { model: string; firmware: string; manufacturer: string } | null;
}) {
  return (
    <>
      <Section>
        <ConnectionRow link={link} known={known} />
        <PowerRow link={link} />
      </Section>
      <button
        className="btn btn-ghost-danger device-forget"
        onClick={() => void store.forget(known?.id)}
      >
        Forget device
      </button>
      <Section title="Info">
        <InfoRow label="Model" value={deviceInfo?.model} link={link} />
        <InfoRow label="Firmware" value={deviceInfo?.firmware} link={link} />
        <InfoRow
          label="Manufacturer"
          value={deviceInfo?.manufacturer}
          link={link}
        />
      </Section>
    </>
  );
}

function UnpairedView({ connecting }: { connecting: boolean }) {
  return (
    <button
      className="btn btn-primary device-pair"
      disabled={connecting}
      onClick={() => void store.connect()}
    >
      {connecting ? "Pairing…" : "Pair device"}
    </button>
  );
}

/* ── Rows ─────────────────────────────────────────────────────────────── */

function ConnectionRow({
  link,
  known,
}: {
  link: LinkState;
  known: { id: string; name: string } | null;
}) {
  // The switch reflects "is GATT linked?" — true for any "linked" state
  // (active, sleeping, idle); false otherwise. Connecting is busy.
  const checked = link.kind === "linked";
  const busy = link.kind === "connecting";
  return (
    <Row
      label="Connection"
      value={
        <Switch
          label="Connection"
          checked={checked}
          busy={busy}
          onChange={(next) => {
            if (next) {
              // Prefer silent reconnect if the browser remembers a device.
              if (known) void store.reconnect(known.id);
              else void store.connect();
            } else {
              void store.disconnect();
            }
          }}
        />
      }
    />
  );
}

function PowerRow({ link }: { link: LinkState }) {
  // Power only does anything when the link is up — otherwise there's
  // nothing to put to sleep. We still render the row so the layout
  // doesn't shift when (re)connecting; the switch just disables.
  const linked = link.kind === "linked";
  const sleeping = linked && link.hid === "sleeping";
  return (
    <Row
      label="Power"
      value={
        <Switch
          label="Power"
          checked={linked && !sleeping}
          disabled={!linked}
          onChange={(next) => {
            if (next) void store.wake();
            else void store.sleep();
          }}
        />
      }
    />
  );
}

/** Device-info row. Three rendering modes, picked from link state so the
 *  row honestly communicates whether a value is *expected* or not:
 *    • value present              → show it
 *    • linking, value not yet in  → shimmer skeleton (we're fetching)
 *    • disconnected / failed / …  → "—" (nothing to fetch, no false hope) */
function InfoRow({
  label,
  value,
  link,
}: {
  label: string;
  value: string | undefined;
  link: LinkState;
}) {
  let content: ReactNode;
  if (value) {
    content = value;
  } else if (link.kind === "connecting" || link.kind === "linked") {
    content = <span className="skeleton skeleton-text" aria-hidden="true" />;
  } else {
    content = <span className="row-value-empty">—</span>;
  }
  return <Row label={label} value={content} />;
}

function UnsupportedView() {
  return (
    <div className="device-stage">
      <Artwork variant="device" stateClass="artwork-offline" />
      <p className="device-caption device-caption-error">
        Web Bluetooth isn't available here. Use Chrome, Edge, Brave, or
        another Chromium-based browser.
      </p>
    </div>
  );
}

/** Map the link state to an artwork modifier class. The class drives all
 *  visual state — shake, grayscale, error tint — from a single source. */
function artworkClass(link: LinkState): string {
  switch (link.kind) {
    case "connecting":
      return "artwork-busy";
    case "failed":
      return "artwork-error artwork-offline";
    case "idle":
      return "artwork-offline";
    case "linked":
      return link.hid === "sleeping" ? "artwork-sleeping" : "";
    case "unsupported":
      return "artwork-offline";
  }
}
