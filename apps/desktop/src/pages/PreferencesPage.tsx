import { useI18n } from "../i18n/provider";

const localeNames = {
  en: "English",
  de: "Deutsch",
} as const;

export const PreferencesPage = () => {
  const { copy, locale, locales, setLocale } = useI18n();

  return (
    <div className="page">
      <header className="page__header">
        <div>
          <p className="eyebrow">{copy.shell.eyebrow}</p>
          <h2>{copy.preferences.title}</h2>
        </div>
      </header>
      <section className="panel">
        <p className="panel__lead">{copy.preferences.description}</p>

        <div className="inspector-section">
          <h3>{copy.preferences.languageTitle}</h3>
          <p className="muted">{copy.preferences.languageDescription}</p>
          <label>
            <span>{copy.preferences.localeLabel}</span>
            <select onChange={(event) => setLocale(event.currentTarget.value as typeof locale)} value={locale}>
              {locales.map((entry) => (
                <option key={entry} value={entry}>
                  {localeNames[entry]}
                </option>
              ))}
            </select>
          </label>
        </div>

        <ul className="static-list">
          <li>{copy.preferences.quickPreviewDefault}</li>
          <li>{copy.preferences.noTelemetry}</li>
          <li>{copy.preferences.localizationNote}</li>
        </ul>
      </section>
    </div>
  );
};
