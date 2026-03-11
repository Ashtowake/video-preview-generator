import type { PropsWithChildren } from "react";
import { NavLink } from "react-router-dom";

import { useI18n } from "../i18n/provider";

export const AppShell = ({ children }: PropsWithChildren) => {
  const { copy, locale, locales, setLocale } = useI18n();

  const primaryItems = [
    { path: "/editor", label: copy.nav.editor },
    { path: "/preferences", label: copy.nav.preferences },
    { path: "/updates", label: copy.nav.updates },
  ];

  const menuItems = [
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
      <header className="app-bar">
        <div className="app-bar__brand">
          <p className="eyebrow">{copy.shell.eyebrow}</p>
          <h1>{copy.shell.title}</h1>
        </div>
        <nav aria-label="Primary" className="app-bar__nav">
          {primaryItems.map((item) => (
            <NavLink
              key={item.path}
              className={({ isActive }) => `nav-link${isActive ? " nav-link--active" : ""}`}
              to={item.path}
            >
              {item.label}
            </NavLink>
          ))}
          <details className="app-menu">
            <summary>{copy.shell.menuLabel}</summary>
            <div className="app-menu__content">
              {menuItems.map((item) => (
                <NavLink
                  key={item.path}
                  className={({ isActive }) => `menu-link${isActive ? " menu-link--active" : ""}`}
                  to={item.path}
                >
                  {item.label}
                </NavLink>
              ))}
            </div>
          </details>
        </nav>
        <label className="app-bar__locale">
          <span>{copy.shell.languageLabel}</span>
          <select onChange={(event) => setLocale(event.currentTarget.value as typeof locale)} value={locale}>
            {locales.map((entry) => (
              <option key={entry} value={entry}>
                {entry.toUpperCase()}
              </option>
            ))}
          </select>
        </label>
      </header>
      <main className="content">{children}</main>
    </div>
  );
};
