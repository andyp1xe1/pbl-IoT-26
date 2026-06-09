import type { ReactNode } from "react";
import { Screen } from "./Screen";
import { DisconnectedPlaceholder } from "./DisconnectedPlaceholder";
import { SleepingPlaceholder } from "./SleepingPlaceholder";
import { selectLinkState } from "../state/selectors";
import { useAppState } from "../state/store";

/**
 * Wrapper for the "work" screens (Tune, Calibrate, About) that all share
 * the same precondition: device must be linked AND awake to render their
 * real content. Owns the disconnected / sleeping placeholder branches so
 * each screen body can assume the happy path.
 *
 * React only mounts `children` when the gate passes — the body component's
 * hooks (useAppState etc.) don't run while the placeholder is showing.
 */
export function WorkScreen({
  title,
  caption,
  gridClass,
  children,
}: {
  title: string;
  /** One short sentence telling the user what this screen offers when
   *  connected. Surfaced in both placeholder variants. */
  caption: string;
  gridClass?: string;
  children: ReactNode;
}) {
  const link = selectLinkState(useAppState());

  if (link.kind !== "linked") {
    return (
      <Screen title={title}>
        <DisconnectedPlaceholder link={link} caption={caption} />
      </Screen>
    );
  }
  if (link.hid === "sleeping") {
    return (
      <Screen title={title}>
        <SleepingPlaceholder caption={caption} />
      </Screen>
    );
  }
  return (
    <Screen title={title} gridClass={gridClass}>
      {children}
    </Screen>
  );
}
