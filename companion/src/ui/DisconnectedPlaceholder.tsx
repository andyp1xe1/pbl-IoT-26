import { store } from "../state/store";
import { EmptyState } from "./EmptyState";
import type { LinkState } from "../state/selectors";
import type { KnownDevice } from "../ble/types";

/**
 * Polished placeholder used by every non-Connect screen when the device
 * isn't linked. Reads `LinkState` so it surfaces the same Reconnect /
 * Choose actions the Connect screen uses, plus a per-screen caption that
 * explains what the user could do here once connected.
 *
 * Caller passes only the non-linked states; passing `kind: "linked"` is a
 * type error at the call site by construction (`Exclude<…, "linked">`).
 */
export function DisconnectedPlaceholder({
  link,
  caption,
}: {
  link: Exclude<LinkState, { kind: "linked" }>;
  /** One short sentence on what this screen offers when connected. */
  caption: string;
}) {
  switch (link.kind) {
    case "unsupported":
      return (
        <EmptyState
          title="Unsupported browser"
          caption="Use Chrome, Edge, Brave, or another Chromium-based browser."
        />
      );

    case "connecting":
      return (
        <EmptyState
          title="Connecting…"
          caption={caption}
          action={
            <button className="btn btn-primary" disabled>
              Connecting…
            </button>
          }
        />
      );

    case "failed":
      return (
        <EmptyState
          title="Not connected"
          caption={caption}
          error={link.reason}
          action={<ConnectActions known={link.known} primary="retry" />}
        />
      );

    case "idle":
      return (
        <EmptyState
          title={link.known ? "Ready to reconnect" : "Not connected"}
          caption={
            link.known
              ? `${caption} ${link.known.name} is remembered.`
              : caption
          }
          action={<ConnectActions known={link.known} primary="connect" />}
        />
      );
  }
}

function ConnectActions({
  known,
  primary,
}: {
  known: KnownDevice | null;
  primary: "connect" | "retry";
}) {
  if (!known) {
    return (
      <button className="btn btn-primary" onClick={() => void store.connect()}>
        {primary === "retry" ? "Try again" : "Connect"}
      </button>
    );
  }
  return (
    <div className="connect-actions">
      <button
        className="btn btn-primary"
        onClick={() => void store.reconnect(known.id)}
      >
        {primary === "retry" ? "Retry" : `Reconnect ${known.name}`}
      </button>
      <button className="btn btn-ghost" onClick={() => void store.connect()}>
        Choose another…
      </button>
    </div>
  );
}
