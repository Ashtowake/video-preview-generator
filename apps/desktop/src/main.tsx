import React from "react";
import ReactDOM from "react-dom/client";
import { createHashRouter, Navigate, Outlet, RouterProvider } from "react-router-dom";

import { AppShell } from "./components/AppShell";
import { I18nProvider } from "./i18n/provider";
import { EditorPage } from "./pages/EditorPage";
import { PreferencesPage } from "./pages/PreferencesPage";
import { StaticPage } from "./pages/StaticPage";
import "./styles.css";

const router = createHashRouter([
  {
    path: "/",
    element: (
      <AppShell>
        <Outlet />
      </AppShell>
    ),
    children: [
      { index: true, element: <Navigate replace to="/editor" /> },
      { path: "editor", element: <EditorPage /> },
      { path: "about", element: <StaticPage section="about" /> },
      { path: "credits", element: <StaticPage section="credits" /> },
      { path: "third-party-notices", element: <StaticPage section="third-party-notices" /> },
      { path: "release-notes", element: <StaticPage section="release-notes" /> },
      { path: "updates", element: <StaticPage section="updates" /> },
      { path: "preferences", element: <PreferencesPage /> },
      { path: "diagnostics", element: <StaticPage section="diagnostics" /> },
      { path: "help", element: <StaticPage section="help" /> },
      { path: "keyboard-shortcuts", element: <StaticPage section="keyboard-shortcuts" /> },
      { path: "report-issue", element: <StaticPage section="report-issue" /> },
    ],
  },
]);

ReactDOM.createRoot(document.getElementById("root")!).render(
  <React.StrictMode>
    <I18nProvider>
      <RouterProvider router={router} />
    </I18nProvider>
  </React.StrictMode>,
);
