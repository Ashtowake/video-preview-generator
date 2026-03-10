/**
 * Central editor state for the desktop app.
 *
 * Presentational React components read from this store and dispatch actions, while media I/O stays
 * behind the backend wrappers and pure project math stays in `@video-preview/domain`.
 */
import {
  autoFillTiles,
  buildDiagnosticsBundle,
  clampPlayheadMs,
  createStarterProject,
  estimateFullFidelity,
  formatTimeMs,
  parseTimeDelta,
  stepByFrames,
  stepByTime,
  validateProject,
  type AnalysisMode,
  type DiagnosticsBundle,
  type PreviewFrame,
  type ProjectFile,
} from "@video-preview/domain";
import { create } from "zustand";

import {
  diagnosticsBundle,
  exportProject,
  fetchPreviewFrame,
  isDesktopRuntime,
  loadProject,
  probeVideo,
  projectFileUrl,
  saveProject,
  sharpestNeighbours,
} from "../lib/backend";

/** User-facing notification surfaced in the editor panes. */
export type EditorAlert = { tone: "info" | "warning"; message: string } | null;

/** Public store contract for editor actions and derived state. */
export interface EditorState {
  project: ProjectFile;
  selectedTileId: string | null;
  loadedVideoUrl: string | null;
  loadedVideoSizeBytes: number;
  previewFrame: PreviewFrame | null;
  previewBusy: boolean;
  previewError: string | null;
  alert: EditorAlert;
  diagnostics: DiagnosticsBundle;
  busy: boolean;
  setLoadedVideo: (file: File) => void;
  loadVideoFromPath: (path: string) => Promise<void>;
  setDurationMs: (durationMs: number) => void;
  setPlayheadMs: (playheadMs: number) => void;
  setRangeStart: (startMs: number) => void;
  setRangeEnd: (endMs: number) => void;
  setCustomSkip: (value: string) => void;
  setFrameStep: (step: number) => void;
  stepCustom: (direction: -1 | 1) => void;
  stepSeconds: (seconds: 1 | 5, direction: -1 | 1) => void;
  stepFrames: (direction: -1 | 1) => void;
  setAnalysisMode: (mode: AnalysisMode) => void;
  autoFill: () => void;
  selectTile: (tileId: string) => void;
  toggleTilePin: (tileId: string) => void;
  fineTuneTile: (tileId: string, deltaMs: number) => void;
  setTileManualFrame: (tileId: string, frameIndex: number) => void;
  resizeTile: (tileId: string, rowSpan: number, columnSpan: number) => void;
  runSharpestNeighbour: () => Promise<void>;
  saveProjectToPath: (path: string) => Promise<void>;
  loadProjectFromPath: (path: string) => Promise<void>;
  exportProjectToPath: (path?: string) => Promise<string | null>;
  refreshPreviewFrame: (maxWidth?: number) => Promise<void>;
}

const starterProject = createStarterProject();
let previewRequestSequence = 0;

/** Keeps browser-only diagnostics available when the Rust shell is not active. */
const localDiagnostics = (project: ProjectFile): DiagnosticsBundle => buildDiagnosticsBundle(project);

/** Recomputes derived store fields after any project mutation. */
const syncState = (project: ProjectFile) => ({
  project,
  diagnostics: localDiagnostics(project),
  selectedTileId: project.tiles[0]?.id ?? null,
});

/**
 * Zustand hook for all editor state.
 *
 * The store is intentionally action-heavy: validation, range math, and backend calls are kept
 * here so React components stay close to declarative view code.
 */
export const useEditorStore = create<EditorState>((set, get) => ({
  project: starterProject,
  selectedTileId: starterProject.tiles[0]?.id ?? null,
  loadedVideoUrl: null,
  loadedVideoSizeBytes: 0,
  previewFrame: null,
  previewBusy: false,
  previewError: null,
  alert: null,
  diagnostics: localDiagnostics(starterProject),
  busy: false,
  setLoadedVideo: (file) => {
    const nextProject = {
      ...get().project,
      video: {
        ...get().project.video,
        path: file.name,
      },
    };

    set({
      loadedVideoUrl: URL.createObjectURL(file),
      loadedVideoSizeBytes: file.size,
      previewFrame: null,
      previewBusy: false,
      previewError: null,
      ...syncState(nextProject),
      alert: { tone: "info", message: `Loaded ${file.name}` },
    });
  },
  loadVideoFromPath: async (path) => {
    set({ busy: true, alert: null, previewFrame: null, previewBusy: false, previewError: null });
    try {
      const project = await probeVideo(path);
      set({
        busy: false,
        loadedVideoUrl: projectFileUrl(path),
        loadedVideoSizeBytes: 0,
        previewFrame: null,
        previewBusy: false,
        previewError: null,
        ...syncState(project),
        alert: { tone: "info", message: `Loaded ${path}` },
      });
    } catch (error) {
      set({
        busy: false,
        alert: {
          tone: "warning",
          message: error instanceof Error ? error.message : "Failed to load video.",
        },
      });
    }
  },
  setDurationMs: (durationMs) => {
    const nextProject = {
      ...get().project,
      video: {
        ...get().project.video,
        durationMs,
        frameCount: Math.round((durationMs / 1000) * (get().project.video.fps ?? 24)),
      },
      range: { ...get().project.range, endMs: durationMs },
    };
    set(syncState(nextProject));
  },
  setPlayheadMs: (playheadMs) => {
    const durationMs = get().project.video.durationMs ?? 60_000;
    const nextPlayheadMs = clampPlayheadMs(playheadMs, durationMs);
    const fps = get().project.video.fps ?? 24;
    const nextProject = {
      ...get().project,
      playback: {
        ...get().project.playback,
        playheadMs: nextPlayheadMs,
        activeFrameIndex: Math.round((nextPlayheadMs / 1000) * fps),
      },
    };
    set(syncState(nextProject));
  },
  setRangeStart: (startMs) => {
    const { project } = get();
    const nextProject = {
      ...project,
      range: {
        startMs: Math.min(startMs, project.range.endMs - 250),
        endMs: project.range.endMs,
      },
    };
    set(syncState(nextProject));
  },
  setRangeEnd: (endMs) => {
    const { project } = get();
    const durationMs = project.video.durationMs ?? 60_000;
    const nextProject = {
      ...project,
      range: {
        startMs: project.range.startMs,
        endMs: Math.max(Math.min(endMs, durationMs), project.range.startMs + 250),
      },
    };
    set(syncState(nextProject));
  },
  setCustomSkip: (value) => {
    try {
      const customSkipMs = parseTimeDelta(value);
      const nextProject = {
        ...get().project,
        playback: { ...get().project.playback, customSkipMs },
      };
      set({
        ...syncState(nextProject),
        alert: { tone: "info", message: `Custom jump set to ${formatTimeMs(customSkipMs)}` },
      });
    } catch (error) {
      set({
        alert: {
          tone: "warning",
          message: error instanceof Error ? error.message : "Invalid custom time delta.",
        },
      });
    }
  },
  setFrameStep: (step) => {
    const nextProject = {
      ...get().project,
      playback: {
        ...get().project.playback,
        frameStep: Math.max(1, Math.round(step)),
      },
    };
    set(syncState(nextProject));
  },
  stepCustom: (direction) => {
    const { project } = get();
    const durationMs = project.video.durationMs ?? 60_000;
    get().setPlayheadMs(
      stepByTime(
        project.playback.playheadMs,
        project.playback.customSkipMs * direction,
        durationMs,
      ),
    );
  },
  stepSeconds: (seconds, direction) => {
    const { project } = get();
    const durationMs = project.video.durationMs ?? 60_000;
    get().setPlayheadMs(
      stepByTime(project.playback.playheadMs, seconds * 1000 * direction, durationMs),
    );
  },
  stepFrames: (direction) => {
    const { project } = get();
    const durationMs = project.video.durationMs ?? 60_000;
    get().setPlayheadMs(
      stepByFrames(
        project.playback.playheadMs,
        project.playback.frameStep * direction,
        project.video.fps,
        durationMs,
      ),
    );
  },
  setAnalysisMode: (mode) => {
    const { project, loadedVideoSizeBytes } = get();
    if (mode === "full_fidelity") {
      // Browser-loaded object URLs only know the local file size, so the estimate remains coarse.
      const estimate = estimateFullFidelity(project, loadedVideoSizeBytes);
      if (!estimate.canUpgradeToFullFidelity) {
        set({
          alert: {
            tone: "warning",
            message: estimate.reason ?? "Full fidelity mode is blocked by current safety limits.",
          },
        });
        return;
      }
    }

    const nextProject = { ...project, analysisMode: mode };
    set({
      ...syncState(nextProject),
      alert: {
        tone: "info",
        message:
          mode === "full_fidelity"
            ? "Full fidelity mode enabled."
            : "Quick preview mode keeps memory and cache use low.",
      },
    });
  },
  autoFill: () => {
    const { project } = get();
    const nextProject = {
      ...project,
      tiles: autoFillTiles(
        project.tiles,
        project.range.startMs,
        project.range.endMs,
        project.video.fps,
      ),
    };
    set(syncState(nextProject));
  },
  selectTile: (tileId) => set({ selectedTileId: tileId }),
  toggleTilePin: (tileId) => {
    const nextProject = {
      ...get().project,
      tiles: get().project.tiles.map((tile) =>
        tile.id === tileId ? { ...tile, pinned: !tile.pinned } : tile,
      ),
    };
    set(syncState(nextProject));
  },
  fineTuneTile: (tileId, deltaMs) => {
    const nextProject = {
      ...get().project,
      tiles: get().project.tiles.map((tile) =>
        tile.id === tileId
          ? { ...tile, fineTuneOffsetMs: tile.fineTuneOffsetMs + deltaMs }
          : tile,
      ),
    };
    set(syncState(nextProject));
  },
  setTileManualFrame: (tileId, frameIndex) => {
    const fps = get().project.video.fps ?? 24;
    const timeMs = Math.round((frameIndex / fps) * 1000);
    const nextProject = {
      ...get().project,
      tiles: get().project.tiles.map((tile) =>
        tile.id === tileId
          ? {
              ...tile,
              selection: { kind: "manual" as const, frameIndex, timeMs },
              pinned: true,
            }
          : tile,
      ),
    };
    set(syncState(nextProject));
  },
  resizeTile: (tileId, rowSpan, columnSpan) => {
    const nextProject = {
      ...get().project,
      tiles: get().project.tiles.map((tile) =>
        tile.id === tileId
          ? {
              ...tile,
              span: {
                ...tile.span,
                rowSpan: Math.max(1, rowSpan),
                columnSpan: Math.max(1, columnSpan),
              },
            }
          : tile,
      ),
    };
    const issues = validateProject(nextProject);
    // Reject invalid spans before mutating the store so the UI never enters an overlapping state.
    const overlapIssue = issues.find((issue) => issue.path === `tiles.${tileId}`);
    if (overlapIssue) {
      set({ alert: { tone: "warning", message: overlapIssue.message } });
      return;
    }
    set(syncState(nextProject));
  },
  runSharpestNeighbour: async () => {
    const { project, selectedTileId } = get();
    if (!selectedTileId) {
      set({ alert: { tone: "warning", message: "Select a tile first." } });
      return;
    }

    if (!isDesktopRuntime()) {
      set({
        alert: {
          tone: "warning",
          message: "Sharpness analysis requires the Tauri runtime.",
        },
      });
      return;
    }

    set({ busy: true, alert: null });
    try {
      const nextProject = await sharpestNeighbours(project, [selectedTileId]);
      set({
        busy: false,
        ...syncState(nextProject),
        alert: { tone: "info", message: "Updated the selected tile to the sharpest nearby frame." },
      });
    } catch (error) {
      set({
        busy: false,
        alert: {
          tone: "warning",
          message:
            error instanceof Error ? error.message : "Sharpness analysis failed.",
        },
      });
    }
  },
  saveProjectToPath: async (path) => {
    set({ busy: true, alert: null });
    try {
      await saveProject(path, get().project);
      const diagnostics = isDesktopRuntime()
        ? await diagnosticsBundle(get().project)
        : localDiagnostics(get().project);
      set({
        busy: false,
        diagnostics,
        alert: { tone: "info", message: `Saved project to ${path}` },
      });
    } catch (error) {
      set({
        busy: false,
        alert: {
          tone: "warning",
          message: error instanceof Error ? error.message : "Failed to save project.",
        },
      });
    }
  },
  loadProjectFromPath: async (path) => {
    set({ busy: true, alert: null });
    try {
      const project = await loadProject(path);
      const diagnostics = isDesktopRuntime()
        ? await diagnosticsBundle(project)
        : localDiagnostics(project);
      set({
        busy: false,
        loadedVideoUrl: project.video.path ? projectFileUrl(project.video.path) : null,
        previewFrame: null,
        previewBusy: false,
        previewError: null,
        ...syncState(project),
        diagnostics,
        alert: { tone: "info", message: `Loaded project from ${path}` },
      });
    } catch (error) {
      set({
        busy: false,
        alert: {
          tone: "warning",
          message: error instanceof Error ? error.message : "Failed to load project.",
        },
      });
    }
  },
  exportProjectToPath: async (path) => {
    set({ busy: true, alert: null });
    try {
      const result = await exportProject(get().project, path);
      set({
        busy: false,
        alert: { tone: "info", message: `Exported sheet to ${result.outputPath}` },
      });
      return result.outputPath;
    } catch (error) {
      set({
        busy: false,
        alert: {
          tone: "warning",
          message: error instanceof Error ? error.message : "Export failed.",
        },
      });
      return null;
    }
  },
  refreshPreviewFrame: async (maxWidth = 960) => {
    if (!isDesktopRuntime()) {
      set({ previewFrame: null, previewBusy: false, previewError: null });
      return;
    }

    const { project } = get();
    if (!project.video.path || project.video.path === "unloaded-video.mp4") {
      set({ previewFrame: null, previewBusy: false, previewError: null });
      return;
    }

    const requestId = previewRequestSequence + 1;
    previewRequestSequence = requestId;
    set({ previewBusy: true, previewError: null });

    try {
      const previewFrame = await fetchPreviewFrame(project, project.playback.playheadMs, maxWidth);
      if (requestId !== previewRequestSequence) {
        return;
      }

      set({
        previewFrame,
        previewBusy: false,
        previewError: null,
      });
    } catch (error) {
      if (requestId !== previewRequestSequence) {
        return;
      }

      set({
        previewBusy: false,
        previewError: error instanceof Error ? error.message : "Failed to refresh preview.",
      });
    }
  },
}));
