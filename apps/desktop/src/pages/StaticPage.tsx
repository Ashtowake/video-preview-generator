import { useEditorStore } from "../store/editorStore";
import { useI18n } from "../i18n/provider";
import type { StaticSection } from "../i18n/messages";

export const StaticPage = ({ section }: { section: StaticSection }) => {
  const diagnostics = useEditorStore((state) => state.diagnostics);
  const { copy } = useI18n();
  const content = copy.staticPages[section];

  return (
    <div className="page">
      <header className="page__header">
        <div>
          <p className="eyebrow">{copy.shell.eyebrow}</p>
          <h2>{content.title}</h2>
        </div>
      </header>
      <section className="panel">
        <p className="panel__lead">{content.description}</p>
        <ul className="static-list">
          {content.bullets.map((bullet) => (
            <li key={bullet}>{bullet}</li>
          ))}
        </ul>
        {section === "diagnostics" ? (
          <pre className="code-block">{JSON.stringify(diagnostics, null, 2)}</pre>
        ) : null}
      </section>
    </div>
  );
};
