import { validateProject } from "@video-preview/domain";

import { GridCanvas } from "../components/GridCanvas";
import { InspectorPane } from "../components/InspectorPane";
import { MediaRangePane } from "../components/MediaRangePane";
import { useI18n } from "../i18n/provider";
import { isDesktopRuntime, openProjectDialog, savePathDialog } from "../lib/backend";
import { displayPathName } from "../lib/sheetLayout";
import { useEditorStore } from "../store/editorStore";

const defaultProjectPath = (videoPath: string) =>
  videoPath.replace(/\.[^.]+$/, ".vpg.json").replace(/$/, videoPath.includes(".") ? "" : ".vpg.json");

const defaultExportPath = (videoPath: string) =>
  videoPath.replace(/\.[^.]+$/, "_preview.png").replace(/$/, videoPath.includes(".") ? "" : "_preview.png");

export const EditorPage = () => {
  const project = useEditorStore((state) => state.project);
  const busy = useEditorStore((state) => state.busy);
  const loadProjectFromPath = useEditorStore((state) => state.loadProjectFromPath);
  const saveProjectToPath = useEditorStore((state) => state.saveProjectToPath);
  const exportProjectToPath = useEditorStore((state) => state.exportProjectToPath);
  const issues = validateProject(project);
  const { copy } = useI18n();

  return (
    <div className="page page--editor">
      <header className="page__header">
        <div>
          <p className="eyebrow">{copy.editor.eyebrow}</p>
          <h2>{displayPathName(project.video.path)}</h2>
        </div>
        <div className="status-stack">
          <button
            disabled={busy || !isDesktopRuntime()}
            onClick={() => {
              void (async () => {
                const path = await openProjectDialog();
                if (path) {
                  await loadProjectFromPath(path);
                }
              })();
            }}
            type="button"
          >
            {copy.editor.loadProject}
          </button>
          <button
            disabled={busy || !isDesktopRuntime()}
            onClick={() => {
              void (async () => {
                const path = await savePathDialog(defaultProjectPath(project.video.path), [
                  {
                    name: "Video Preview Project",
                    extensions: ["json"],
                  },
                ]);
                if (path) {
                  await saveProjectToPath(path);
                }
              })();
            }}
            type="button"
          >
            {copy.editor.saveProject}
          </button>
          <button
            disabled={busy || !isDesktopRuntime()}
            onClick={() => {
              void (async () => {
                const path = await savePathDialog(defaultExportPath(project.video.path), [
                  {
                    name: "PNG Image",
                    extensions: ["png"],
                  },
                  {
                    name: "JPEG Image",
                    extensions: ["jpg", "jpeg"],
                  },
                ]);
                if (path) {
                  await exportProjectToPath(path);
                }
              })();
            }}
            type="button"
          >
            {copy.editor.exportSheet}
          </button>
          <span className="status-pill">
            {project.analysisMode === "quick_preview" ? copy.editor.quickPreview : copy.editor.fullFidelity}
          </span>
          {busy ? <span className="status-pill">{copy.editor.busy}</span> : null}
          <span className="status-pill">{issues.length} {copy.editor.validationIssues}</span>
        </div>
      </header>

      <div className="editor-layout">
        <MediaRangePane />
        <div className="workspace-stack">
          <GridCanvas />
          <InspectorPane />
        </div>
      </div>
    </div>
  );
};
