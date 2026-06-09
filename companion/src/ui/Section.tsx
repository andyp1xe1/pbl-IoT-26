import { ReactNode } from "react";

export function Section({
  title,
  children,
  footer,
}: {
  title?: string;
  children: ReactNode;
  footer?: ReactNode;
}) {
  return (
    <section className="section">
      {title && <h2 className="section-title">{title}</h2>}
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
