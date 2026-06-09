import { store } from "../state/store";
import { EmptyState } from "./EmptyState";

/**
 * Polished placeholder shown on screens whose content depends on a live
 * sensor stream (Tune, Calibrate, About) when the device is in soft-sleep.
 * Matches the visual rhythm of <DisconnectedPlaceholder>: glove schematic +
 * one-line caption tailored to the host screen + a single primary action.
 *
 * The Connect screen does *not* use this — it surfaces sleep state inline as
 * a pill on the device card so the wake action stays adjacent to the
 * Disconnect action.
 */
export function SleepingPlaceholder({ caption }: { caption: string }) {
  return (
    <EmptyState
      title="Sleeping"
      caption={caption}
      action={
        <button className="btn btn-primary" onClick={() => void store.wake()}>
          Wake
        </button>
      }
    />
  );
}
