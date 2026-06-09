import { ReactNode } from "react";

export function Section({
  title,
  action,
  children,
  footer,
  className,
}: {
  title?: string;
  /** Inline content rendered at the right of the title row — e.g. a small
   *  diagnostic badge or status chip. Requires `title` to be set. */
  action?: ReactNode;
  children: ReactNode;
  footer?: ReactNode;
  /** Extra class for the outer <section> — used by screens with
   *  named-area grids to pin this section into a specific cell. */
  className?: string;
}) {
  return (
    <section className={className ? `section ${className}` : "section"}>
      {title && (
        <div className="section-head">
          <h2 className="section-title">{title}</h2>
          {action}
        </div>
      )}
      <div className="card">{children}</div>
      {footer && <p className="section-footer">{footer}</p>}
    </section>
  );
}

export function Row({
  label,
  value,
  onClick,
}: {
  label: ReactNode;
  value?: ReactNode;
  onClick?: () => void;
}) {
  return (
    <div className={`row${onClick ? " row-tappable" : ""}`} onClick={onClick}>
      <span className="row-label">{label}</span>
      {value !== undefined && <span className="row-value">{value}</span>}
    </div>
  );
}
