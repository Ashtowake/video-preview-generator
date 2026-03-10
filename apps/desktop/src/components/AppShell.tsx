import type { PropsWithChildren } from "react";
import { NavLink } from "react-router-dom";

import { useI18n } from "../i18n/provider";

export const AppShell = ({ children }: PropsWithChildren) => {
  const { copy, locale, locales, setLocale } = useI18n();

  const navItems = [
    { path: "/editor", label: copy.nav.editor },
    { path: "/preferences", label: copy.nav.preferences },
    { path: "/updates", label: copy.nav.updates },
    { path: "/diagnostics", label: copy.nav.diagnostics },
    { path: "/help", label: copy.nav.help },
    { path: "/keyboard-shortcuts", label: copy.nav.shortcuts },
    { path: "/release-notes", label: copy.nav.releaseNotes },
    { path: "/credits", label: copy.nav.credits },
    { path: "/third-party-notices", label: copy.nav.notices },
    { path: "/about", label: copy.nav.about },
    { path: "/report-issue", label: copy.nav.reportIssue },
  ];

  return (
    <div className="app-shell">
      <aside className="sidebar">
        <div className="sidebar__brand">
          <p className="eyebrow">{copy.shell.eyebrow}</p>
          <h1>{copy.shell.title}</h1>
          <p className="muted">{copy.shell.description}</p>
        </div>
        <nav className="sidebar__nav" aria-label="Primary">
          {navItems.map((item) => (
            <NavLink
              key={item.path}
              className={({ isActive }) => `nav-link${isActive ? " nav-link--active" : ""}`}
              to={item.path}
            >
              {item.label}
            </NavLink>
          ))}
        </nav>
        <label className="sidebar__locale">
          <span>{copy.shell.languageLabel}</span>
          <select onChange={(event) => setLocale(event.currentTarget.value as typeof locale)} value={locale}>
            {locales.map((entry) => (
              <option key={entry} value={entry}>
                {entry.toUpperCase()}
              </option>
            ))}
          </select>
        </label>
      </aside>
      <main className="content">{children}</main>
    </div>
  );
};
