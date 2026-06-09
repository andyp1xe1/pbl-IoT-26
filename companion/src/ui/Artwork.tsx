import boardFlat from "../assets/board.webp";
import boardIso from "../assets/board-iso.webp";

/**
 * Two interchangeable AirGlove board renderings, both processed the same
 * way (transparent background, accent-tinted line work) so both can be
 * used as CSS mask images. The wrapper's `background-color` paints the
 * colour, the mask alpha-cuts the silhouette out of it. State changes
 * (busy / sleeping / error / offline) remain a single CSS property swap
 * via the `.artwork-*` classes.
 *
 *   • "flat" — top-down line art (horizontal aspect, slim form factor).
 *   • "iso"  — isometric line drawing of the assembled board + IMU +
 *              connector. Richer silhouette but the same single-tone
 *              mask language.
 *
 * Pick the default at build time with `VITE_ART_STYLE=iso|flat`, default
 * "iso".
 */
export type ArtworkKind = "flat" | "iso";

const DEFAULT_KIND: ArtworkKind =
  ((import.meta.env.VITE_ART_STYLE as ArtworkKind | undefined) ?? "iso") === "flat"
    ? "flat"
    : "iso";

const MASK_URL: Record<ArtworkKind, string> = {
  flat: boardFlat,
  iso: boardIso,
};

export function Artwork({
  variant,
  stateClass,
  kind = DEFAULT_KIND,
}: {
  /** Sizes the rendering — empty placeholders are larger than the Home stage. */
  variant: "device" | "empty";
  /** State modifier (e.g. "artwork-busy artwork-sleeping") applied to the
   *  outer wrapper so existing animation + colour rules continue to work. */
  stateClass: string;
  kind?: ArtworkKind;
}) {
  const wrapperCls =
    `artwork artwork-${variant} artwork-${kind} ${stateClass}`.trim();
  const innerCls = variant === "device" ? "device-art" : "empty-art";
  const src = MASK_URL[kind];
  return (
    <div className={wrapperCls}>
      <div
        aria-hidden="true"
        className={innerCls}
        style={{ WebkitMaskImage: `url(${src})`, maskImage: `url(${src})` }}
      />
    </div>
  );
}
