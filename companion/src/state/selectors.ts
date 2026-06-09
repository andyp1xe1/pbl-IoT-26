/**
 * Derived view-model selectors. UI components should consume LinkState rather
 * than reading `status`, `error`, `telemetry.flags`, and `knownDevices`
 * independently — the combination has invalid permutations (e.g. `failed`
 * while still `connecting`) that the union makes unrepresentable.
 *
 * Keep this file pure: no React, no store mutations. One AppState in →
 * one discriminated union out.
 */

import { TELEMETRY_FLAG_HID_CONNECTED, TELEMETRY_FLAG_SLEEPING } from "../ble/types";
import type { KnownDevice } from "../ble/types";
import type { AppState } from "./store";
import type { TabId } from "../ui/TabBar";

/** What the HID profile is doing on the connected host, as exposed via the
 *  firmware's telemetry flag byte. */
export type HidState = "sleeping" | "active" | "idle";

/** The Connect screen's user-facing modes. Every screen state is one of
 *  exactly these — no overlapping booleans. */
export type LinkState =
  | { kind: "unsupported" }
  | { kind: "idle"; known: KnownDevice | null }
  | { kind: "connecting" }
  | { kind: "failed"; reason: string; known: KnownDevice | null }
  | { kind: "linked"; hid: HidState };

export function selectLinkState(s: AppState): LinkState {
  if (!s.webBluetoothAvailable) return { kind: "unsupported" };

  const known = s.knownDevices[0] ?? null;

  if (s.status === "connecting") return { kind: "connecting" };

  if (s.status === "disconnected") {
    // An error left over from a previous *connected* session isn't a connect
    // failure — only treat `error` as a connect failure while disconnected.
    if (s.error != null) return { kind: "failed", reason: s.error, known };
    return { kind: "idle", known };
  }

  // status === "connected"
  return { kind: "linked", hid: selectHidState(s) };
}

function selectHidState(s: AppState): HidState {
  const flags = s.telemetry?.flags ?? 0;
  if (flags & TELEMETRY_FLAG_SLEEPING) return "sleeping";
  if (flags & TELEMETRY_FLAG_HID_CONNECTED) return "active";
  return "idle";
}

/** True when the glove is linked AND awake — i.e. the work-tabs (Tune,
 *  Calibrate, About) make sense to navigate to. Falsy in every other
 *  state including the brief "HID idle" gap right after connect; callers
 *  who want to be more permissive can use `selectLinkState` directly. */
export function selectCanNavigate(s: AppState): boolean {
  const link = selectLinkState(s);
  return link.kind === "linked" && link.hid !== "sleeping";
}

/** The tab that should actually render. Falls back to "device" whenever
 *  the user's intended tab isn't navigable, so a state degradation while
 *  the user is on Tune snaps them back home automatically. When the link
 *  recovers, `state.tab` is preserved so they pick up where they left. */
export function selectActiveTab(s: AppState): TabId {
  return selectCanNavigate(s) ? s.tab : "device";
}
