export type LocaleCode = "en" | "de";

export const locales: LocaleCode[] = ["en", "de"];

export type StaticSection =
  | "about"
  | "credits"
  | "third-party-notices"
  | "release-notes"
  | "updates"
  | "diagnostics"
  | "help"
  | "keyboard-shortcuts"
  | "report-issue";

interface SectionCopy {
  title: string;
  description: string;
  bullets: string[];
}

interface Catalog {
  shell: {
    eyebrow: string;
    title: string;
    description: string;
    languageLabel: string;
  };
  nav: {
    editor: string;
    preferences: string;
    updates: string;
    diagnostics: string;
    help: string;
    shortcuts: string;
    releaseNotes: string;
    credits: string;
    notices: string;
    about: string;
    reportIssue: string;
  };
  editor: {
    eyebrow: string;
    validationIssues: string;
    quickPreview: string;
    fullFidelity: string;
    loadProject: string;
    saveProject: string;
    exportSheet: string;
    busy: string;
  };
  media: {
    eyebrow: string;
    title: string;
    importVideo: string;
    browseVideo: string;
    emptyState: string;
    emptyHint: string;
    backendPreviewHint: string;
    loadingPreview: string;
    previewAlt: string;
    playhead: string;
    rangeStart: string;
    rangeEnd: string;
    startPosition: string;
    rangeLabel: string;
    frameLabel: string;
    customJump: string;
    frameStep: string;
    importMode: string;
    estimatedMemory: string;
    autoFill: string;
    stepCustomBack: string;
    stepCustomForward: string;
    stepFiveBack: string;
    stepFiveForward: string;
    stepOneBack: string;
    stepOneForward: string;
    stepFrameBack: string;
    stepFrameForward: string;
  };
  canvas: {
    eyebrow: string;
    title: string;
    description: string;
    pinned: string;
    auto: string;
    frame: string;
  };
  inspector: {
    eyebrow: string;
    title: string;
    selectedTile: string;
    span: string;
    fineTune: string;
    pin: string;
    unpin: string;
    manualFrame: string;
    taller: string;
    wider: string;
    sharpestNeighbour: string;
    globalStyle: string;
    exportSettings: string;
    rows: string;
    columns: string;
    roundedCorners: string;
    shadow: string;
    border: string;
    gutter: string;
    margin: string;
    metadataBar: string;
    timestamps: string;
    darkMode: string;
    format: string;
    scale: string;
    watermarkText: string;
  };
  preferences: {
    title: string;
    description: string;
    languageTitle: string;
    languageDescription: string;
    localeLabel: string;
    quickPreviewDefault: string;
    noTelemetry: string;
    localizationNote: string;
  };
  staticPages: Record<StaticSection, SectionCopy>;
}

const en: Catalog = {
  shell: {
    eyebrow: "FOSS Desktop Rewrite",
    title: "Video Preview Generator",
    description: "Polished contact sheets, frame control, and batch-ready exports.",
    languageLabel: "Language",
  },
  nav: {
    editor: "Editor",
    preferences: "Preferences",
    updates: "Check for Updates",
    diagnostics: "Diagnostics",
    help: "Help",
    shortcuts: "Shortcuts",
    releaseNotes: "Release Notes",
    credits: "Credits",
    notices: "Third-Party Notices",
    about: "About",
    reportIssue: "Report Issue",
  },
  editor: {
    eyebrow: "Editor-first workflow",
    validationIssues: "validation issue(s)",
    quickPreview: "Quick Preview",
    fullFidelity: "Full Fidelity",
    loadProject: "Load Project",
    saveProject: "Save Project",
    exportSheet: "Export Sheet",
    busy: "Working",
  },
  media: {
    eyebrow: "Media & Range",
    title: "Preview transport",
    importVideo: "Import video",
    browseVideo: "Browse video",
    emptyState: "Load a local video to preview transport and range controls.",
    emptyHint:
      "The browser shell uses an object URL now. The Tauri build can later swap to backend-driven decode.",
    backendPreviewHint: "The desktop app decodes the visible preview through the Rust backend.",
    loadingPreview: "Refreshing preview frame...",
    previewAlt: "Current decoded preview frame",
    playhead: "Playhead",
    rangeStart: "Range start",
    rangeEnd: "Range end",
    startPosition: "Start position",
    rangeLabel: "Range",
    frameLabel: "Frame",
    customJump: "Custom jump",
    frameStep: "Frame step",
    importMode: "Import mode",
    estimatedMemory: "Estimated memory",
    autoFill: "Auto-fill grid",
    stepCustomBack: "- custom",
    stepCustomForward: "+ custom",
    stepFiveBack: "- 5s",
    stepFiveForward: "+ 5s",
    stepOneBack: "- 1s",
    stepOneForward: "+ 1s",
    stepFrameBack: "- frame",
    stepFrameForward: "+ frame",
  },
  canvas: {
    eyebrow: "Sheet canvas",
    title: "Grid composition",
    description: "Rounded edges, frame shadows, metadata, and watermark options are in the inspector.",
    pinned: "Pinned",
    auto: "Auto",
    frame: "Frame",
  },
  inspector: {
    eyebrow: "Inspector",
    title: "Tile and style controls",
    selectedTile: "Selected tile",
    span: "Span",
    fineTune: "Fine tune",
    pin: "Pin tile",
    unpin: "Unpin tile",
    manualFrame: "Manual frame",
    taller: "Taller",
    wider: "Wider",
    sharpestNeighbour: "Find sharpest neighbour",
    globalStyle: "Global style",
    exportSettings: "Export and watermark",
    rows: "Rows",
    columns: "Columns",
    roundedCorners: "Rounded corners",
    shadow: "Shadow",
    border: "Border",
    gutter: "Gutter",
    margin: "Margin",
    metadataBar: "Metadata bar",
    timestamps: "Timestamps",
    darkMode: "Dark mode",
    format: "Format",
    scale: "Scale",
    watermarkText: "Watermark text",
  },
  preferences: {
    title: "Preferences",
    description: "User defaults, localization, and safety settings.",
    languageTitle: "Localization",
    languageDescription:
      "Language catalogs are isolated from components so adding a new locale is a data-only change.",
    localeLabel: "UI language",
    quickPreviewDefault: "Quick Preview stays the default import mode for safer memory usage.",
    noTelemetry: "Telemetry remains off by default.",
    localizationNote: "Add new locales in `src/i18n/messages.ts` and they become available here automatically.",
  },
  staticPages: {
    about: {
      title: "About",
      description: "Ship-ready desktop app shell information.",
      bullets: [
        "Video Preview Generator is a FOSS contact-sheet editor rewrite.",
        "The desktop shell is built around Rust, Tauri, React, and a shared typed project model.",
        "Diagnostics and legal surfaces are designed to ship with the app from the first milestone.",
      ],
    },
    credits: {
      title: "Credits",
      description: "Core project acknowledgements.",
      bullets: [
        "Prototype inspiration and initial layout logic came from the preserved Python reference app.",
        "The rewrite is structured for open-source contributors and maintainers.",
        "Bundled dependencies such as Tauri and FFmpeg require explicit notices and release hygiene.",
      ],
    },
    "third-party-notices": {
      title: "Third-Party Notices",
      description: "Generated legal and attribution surface.",
      bullets: [
        "FFmpeg licensing and source-link compliance must ship with release artifacts.",
        "Tauri is licensed under MIT OR Apache-2.0.",
        "This page is already wired as a real application route and can be backed by build artifacts next.",
      ],
    },
    "release-notes": {
      title: "Release Notes",
      description: "User-visible changes should land here for updater flows.",
      bullets: [
        "Unreleased: workspace rewrite scaffold.",
        "Desktop shell, Rust core, and CLI interfaces are now defined.",
        "Decode and export pipelines remain the next implementation milestone.",
      ],
    },
    updates: {
      title: "Check for Updates",
      description: "Updater surface and channel management.",
      bullets: [
        "Stable and beta channels are planned in the Tauri updater setup.",
        "Signed update metadata and release notes need to be published together.",
        "This route exists now so the app shell does not need to be redesigned later.",
      ],
    },
    diagnostics: {
      title: "Diagnostics",
      description: "Support-oriented state and bundle export preview.",
      bullets: [
        "Diagnostics should include app version, mode, source path, and third-party notices.",
        "Crash or issue reporting stays user-triggered and opt-in.",
        "Logs and generated support bundles remain local.",
      ],
    },
    help: {
      title: "Help",
      description: "End-user guidance surfaces.",
      bullets: [
        "Import a video, set a range, auto-fill the grid, then fine-tune or pin frames.",
        "Use Quick Preview for safe browsing and Full Fidelity for exact frame tools.",
        "Save project files to preserve layout and export settings.",
      ],
    },
    "keyboard-shortcuts": {
      title: "Keyboard Shortcuts",
      description: "Documented input model placeholder.",
      bullets: [
        "J / L: coarse seek backward and forward.",
        "Arrow Left / Right: frame step using the configured step size.",
        "Cmd/Ctrl+S: save project once persistence commands are wired.",
      ],
    },
    "report-issue": {
      title: "Report Issue",
      description: "User support and issue template surface.",
      bullets: [
        "Include the diagnostics bundle, project file, and source video metadata when possible.",
        "Describe whether the issue occurs in Quick Preview or Full Fidelity.",
        "Attach the exported result if the renderer differs from the editor preview.",
      ],
    },
  },
};

const de: Catalog = {
  shell: {
    eyebrow: "FOSS Desktop-Neubau",
    title: "Video Preview Generator",
    description: "Polierte Kontaktbögen, Frame-Kontrolle und batchfähige Exporte.",
    languageLabel: "Sprache",
  },
  nav: {
    editor: "Editor",
    preferences: "Einstellungen",
    updates: "Nach Updates suchen",
    diagnostics: "Diagnose",
    help: "Hilfe",
    shortcuts: "Kurzbefehle",
    releaseNotes: "Versionshinweise",
    credits: "Danksagungen",
    notices: "Drittanbieter-Hinweise",
    about: "Info",
    reportIssue: "Problem melden",
  },
  editor: {
    eyebrow: "Editor-zentrierter Workflow",
    validationIssues: "Validierungsproblem(e)",
    quickPreview: "Schnellvorschau",
    fullFidelity: "Volle Genauigkeit",
    loadProject: "Projekt laden",
    saveProject: "Projekt speichern",
    exportSheet: "Bogen exportieren",
    busy: "Beschäftigt",
  },
  media: {
    eyebrow: "Medien & Bereich",
    title: "Vorschau-Steuerung",
    importVideo: "Video importieren",
    browseVideo: "Video auswählen",
    emptyState: "Lokales Video laden, um Vorschau- und Bereichssteuerung zu testen.",
    emptyHint:
      "Die Browser-Shell nutzt aktuell eine Objekt-URL. Im Tauri-Build kann das später durch Backend-Decoding ersetzt werden.",
    backendPreviewHint: "Die Desktop-App dekodiert die sichtbare Vorschau über das Rust-Backend.",
    loadingPreview: "Vorschaubild wird aktualisiert...",
    previewAlt: "Aktuell dekodiertes Vorschaubild",
    playhead: "Abspielposition",
    rangeStart: "Bereichsbeginn",
    rangeEnd: "Bereichsende",
    startPosition: "Startposition",
    rangeLabel: "Bereich",
    frameLabel: "Frame",
    customJump: "Benutzerdefinierter Sprung",
    frameStep: "Frame-Schritt",
    importMode: "Importmodus",
    estimatedMemory: "Geschätzter Speicher",
    autoFill: "Raster automatisch füllen",
    stepCustomBack: "- benutzerdefiniert",
    stepCustomForward: "+ benutzerdefiniert",
    stepFiveBack: "- 5s",
    stepFiveForward: "+ 5s",
    stepOneBack: "- 1s",
    stepOneForward: "+ 1s",
    stepFrameBack: "- Frame",
    stepFrameForward: "+ Frame",
  },
  canvas: {
    eyebrow: "Bogenfläche",
    title: "Raster-Komposition",
    description: "Abgerundete Ecken, Frame-Schatten, Metadaten und Wasserzeichen liegen im Inspector.",
    pinned: "Fixiert",
    auto: "Auto",
    frame: "Frame",
  },
  inspector: {
    eyebrow: "Inspector",
    title: "Kachel- und Stilsteuerung",
    selectedTile: "Ausgewählte Kachel",
    span: "Spanne",
    fineTune: "Feinabstimmung",
    pin: "Kachel fixieren",
    unpin: "Kachel lösen",
    manualFrame: "Manueller Frame",
    taller: "Höher",
    wider: "Breiter",
    sharpestNeighbour: "Schärfsten Nachbarn finden",
    globalStyle: "Globaler Stil",
    exportSettings: "Export und Wasserzeichen",
    rows: "Zeilen",
    columns: "Spalten",
    roundedCorners: "Runde Ecken",
    shadow: "Schatten",
    border: "Rahmen",
    gutter: "Abstand",
    margin: "Rand",
    metadataBar: "Metadatenleiste",
    timestamps: "Zeitstempel",
    darkMode: "Dunkler Modus",
    format: "Format",
    scale: "Skalierung",
    watermarkText: "Wasserzeichentext",
  },
  preferences: {
    title: "Einstellungen",
    description: "Benutzervorgaben, Lokalisierung und Sicherheitsgrenzen.",
    languageTitle: "Lokalisierung",
    languageDescription:
      "Sprachkataloge sind von den Komponenten getrennt, damit neue Sprachen nur Datenänderungen erfordern.",
    localeLabel: "UI-Sprache",
    quickPreviewDefault: "Schnellvorschau bleibt der Standard-Importmodus für sichere Speichernutzung.",
    noTelemetry: "Telemetrie ist standardmäßig deaktiviert.",
    localizationNote:
      "Neue Sprachen werden in `src/i18n/messages.ts` ergänzt und stehen dann hier automatisch bereit.",
  },
  staticPages: {
    about: {
      title: "Info",
      description: "Informationen zur auslieferbaren Desktop-Shell.",
      bullets: [
        "Video Preview Generator ist ein FOSS-Neubau für Kontaktbögen.",
        "Die Desktop-Shell basiert auf Rust, Tauri, React und einem getypten Projektmodell.",
        "Diagnose- und Rechtsseiten sind von Anfang an Teil der App.",
      ],
    },
    credits: {
      title: "Danksagungen",
      description: "Anerkennungen für das Projekt.",
      bullets: [
        "Die Inspiration und erste Layout-Logik stammen aus der erhaltenen Python-Referenz-App.",
        "Der Neubau ist für Open-Source-Mitwirkende und Maintainer strukturiert.",
        "Gebündelte Abhängigkeiten wie Tauri und FFmpeg benötigen saubere Hinweise und Release-Prozesse.",
      ],
    },
    "third-party-notices": {
      title: "Drittanbieter-Hinweise",
      description: "Erzeugte Rechts- und Attributionsansicht.",
      bullets: [
        "FFmpeg-Lizenzhinweise und Source-Links müssen mit Releases ausgeliefert werden.",
        "Tauri ist unter MIT OR Apache-2.0 lizenziert.",
        "Diese Seite ist bereits als echte Route verdrahtet und kann als Nächstes Build-Artefakte lesen.",
      ],
    },
    "release-notes": {
      title: "Versionshinweise",
      description: "Benutzersichtbare Änderungen für Update-Flows.",
      bullets: [
        "Unveröffentlicht: Workspace-Neuaufbau.",
        "Desktop-Shell, Rust-Core und CLI-Schnittstellen sind definiert.",
        "Decode- und Export-Pipelines sind der nächste Meilenstein.",
      ],
    },
    updates: {
      title: "Nach Updates suchen",
      description: "Update-Oberfläche und Kanalverwaltung.",
      bullets: [
        "Stable- und Beta-Kanäle sind im Tauri-Updater vorgesehen.",
        "Signierte Update-Metadaten und Release Notes müssen gemeinsam veröffentlicht werden.",
        "Diese Route existiert jetzt, damit die App-Shell später nicht neu gestaltet werden muss.",
      ],
    },
    diagnostics: {
      title: "Diagnose",
      description: "Support-orientierte Zustands- und Bundle-Vorschau.",
      bullets: [
        "Diagnosen sollen App-Version, Modus, Quelldatei und Drittanbieter-Hinweise enthalten.",
        "Crash- oder Fehlerberichte bleiben nutzergesteuert und optional.",
        "Logs und Support-Bundles bleiben lokal.",
      ],
    },
    help: {
      title: "Hilfe",
      description: "Oberflächen für Endbenutzer-Hinweise.",
      bullets: [
        "Video importieren, Bereich setzen, Raster füllen und Frames anschließend fein abstimmen oder fixieren.",
        "Schnellvorschau für sicheres Browsing, volle Genauigkeit für exakte Frame-Werkzeuge nutzen.",
        "Projektdateien speichern, um Layout und Exporteinstellungen zu erhalten.",
      ],
    },
    "keyboard-shortcuts": {
      title: "Kurzbefehle",
      description: "Platzhalter für das dokumentierte Eingabemodell.",
      bullets: [
        "J / L: grobes Zurück- und Vorspringen.",
        "Pfeil links / rechts: Frame-Schritte mit der konfigurierten Schrittweite.",
        "Cmd/Ctrl+S: Projekt speichern, sobald Persistenz verdrahtet ist.",
      ],
    },
    "report-issue": {
      title: "Problem melden",
      description: "Oberfläche für Support und Issue-Templates.",
      bullets: [
        "Wenn möglich Diagnose-Bundle, Projektdatei und Video-Metadaten beilegen.",
        "Beschreiben, ob das Problem in Schnellvorschau oder voller Genauigkeit auftritt.",
        "Den Export anhängen, falls Renderer und Editor-Vorschau abweichen.",
      ],
    },
  },
};

export const messages: Record<LocaleCode, Catalog> = { en, de };
