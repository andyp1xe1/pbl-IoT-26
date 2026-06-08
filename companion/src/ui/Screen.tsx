import { ReactNode } from "react";

export function Screen({
  title,
  subtitle,
  children,
}: {
  title: string;
  subtitle?: string;
  children: ReactNode;
}) {
  return (
    <div className="screen">
      <header className="screen-head">
        <h1 className="screen-title">{title}</h1>
        {subtitle && <p className="screen-sub">{subtitle}</p>}
      </header>
      <div className="grid">{children}</div>
    </div>
  );
}
