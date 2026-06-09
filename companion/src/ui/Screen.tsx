import { ReactNode } from "react";

export function Screen({
  title,
  subtitle,
  children,
  gridClass = "grid",
}: {
  title: string;
  subtitle?: string;
  children: ReactNode;
  /** Override the layout wrapper class. Defaults to the generic
   *  responsive auto-fit grid; screens with strong layout opinions
   *  (e.g. Tune) pass their own named-area grid class. */
  gridClass?: string;
}) {
  return (
    <div className="screen">
      <header className="screen-head">
        <h1 className="screen-title">{title}</h1>
        {subtitle && <p className="screen-sub">{subtitle}</p>}
      </header>
      <div className={gridClass}>{children}</div>
    </div>
  );
}
