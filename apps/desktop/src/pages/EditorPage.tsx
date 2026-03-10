import { validateProject } from "@video-preview/domain";

import { GridCanvas } from "../components/GridCanvas";
import { InspectorPane } from "../components/InspectorPane";
import { MediaRangePane } from "../components/MediaRangePane";
import { useI18n } from "../i18n/provider";
import { useEditorStore } from "../store/editorStore";

export const EditorPage = () => {
  const project = useEditorStore((state) => state.project);
  const issues = validateProject(project);
  const { copy } = useI18n();

  return (
    <div className="page page--editor">
      <header className="page__header">
        <div>
          <p className="eyebrow">{copy.editor.eyebrow}</p>
          <h2>{project.video.path}</h2>
        </div>
        <div className="status-stack">
          <span className="status-pill">
            {project.analysisMode === "quick_preview" ? copy.editor.quickPreview : copy.editor.fullFidelity}
          </span>
          <span className="status-pill">{issues.length} {copy.editor.validationIssues}</span>
        </div>
      </header>

      <div className="editor-layout">
        <MediaRangePane />
        <GridCanvas />
        <InspectorPane />
      </div>
    </div>
  );
};
