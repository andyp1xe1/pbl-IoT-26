/** Prefixed console logger. Two %c specifiers: one for the prefix, one to
 *  reset before the message. Anything passed in `...rest` is logged as a
 *  separate arg so DevTools renders it inspectable, not flattened to a string. */

const PREFIX = "[airglove]";
const PFX_STYLE = "color:#6e84ce;font-weight:600";

export const log = {
  info(msg: string, ...rest: unknown[]) {
    console.log(`%c${PREFIX}%c ${msg}`, PFX_STYLE, "", ...rest);
  },
  warn(msg: string, ...rest: unknown[]) {
    console.warn(`%c${PREFIX}%c ${msg}`, PFX_STYLE, "", ...rest);
  },
  error(msg: string, ...rest: unknown[]) {
    console.error(`%c${PREFIX}%c ${msg}`, PFX_STYLE, "", ...rest);
  },
  step(stage: string, ...rest: unknown[]) {
    console.log(
      `%c${PREFIX}%c → ${stage}`,
      PFX_STYLE,
      "color:#8b8fa6",
      ...rest,
    );
  },
};
