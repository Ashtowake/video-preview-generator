/**
 * Central editor state for the desktop app.
 *
 * Presentational React components read from this store and dispatch actions, while media I/O stays
 * behind the backend wrappers and pure project math stays in `@video-preview/domain`.
 */
import {
  autoFillTiles,
  buildDiagnosticsBundle,
  clampFrameIndex,
  clampPlayheadMs,
  createStarterProject,
  estimateFullFidelity,
  frameIndexAtTimeMs,
  formatTimeMs,
  parseTimeDelta,
  reshapeTilesForGrid,
  seekTimeForFrameIndex,
  stepByFrames,
  stepByTime,
  validateProject,
  type AnalysisMode,
  type DiagnosticsBundle,
  type ExportFormat,
  type GridSettings,
  type PreviewFrame,
  type ProjectFile,
  type ProjectStyle,
} from "@video-preview/domain";
import { create } from "zustand";

import {
  diagnosticsBundle,
  exportProject,
  fetchPreviewFrame,
  fetchSheetPreview,
  isDesktopRuntime,
  loadProject,
  playbackBlobUrl,
  prepareVideoPlayback,
  probeVideo,
  projectFileUrl,
  saveProject,
  sharpestNeighbours,
  type SheetPreview,
} from "../lib/backend";

/** User-facing notification surfaced in the editor panes. */
export type EditorAlert = { tone: "info" | "warning"; message: string } | null;

/** Public store contract for editor actions and derived state. */
export interface EditorState {
  project: ProjectFile;
  selectedTileId: string | null;
  loadedVideoUrl: string | null;
  playbackPreparing: boolean;
  loadedVideoSizeBytes: number;
  previewFrame: PreviewFrame | null;
  previewBusy: boolean;
  previewError: string | null;
  sheetPreview: SheetPreview | null;
  sheetPreviewBusy: boolean;
  sheetPreviewError: string | null;
  alert: EditorAlert;
  diagnostics: DiagnosticsBundle;
  busy: boolean;
  setLoadedVideo: (file: File) => void;
  loadVideoFromPath: (path: string) => Promise<void>;
  ensurePlaybackReady: () => Promise<boolean>;
  setDurationMs: (durationMs: number) => void;
  setPlayheadMs: (playheadMs: number) => void;
  setRangeStart: (startMs: number) => void;
  setRangeEnd: (endMs: number) => void;
  setSampleStartOffset: (offsetMs: number) => void;
  setCustomSkip: (value: string) => void;
  setFrameStep: (step: number) => void;
  stepCustom: (direction: -1 | 1) => void;
  stepSeconds: (seconds: 1 | 5, direction: -1 | 1) => void;
  stepFrames: (direction: -1 | 1) => void;
  setAnalysisMode: (mode: AnalysisMode) => void;
  autoFill: () => void;
  setGridDimensions: (rows: number, columns: number) => void;
  setGridSpacing: (gutterPx: number, outerMarginPx: number) => void;
  updateStyle: (style: Partial<ProjectStyle>) => void;
  setExportFormat: (format: ExportFormat) => void;
  setExportScale: (scale: number) => void;
  setWatermarkText: (value: string) => void;
  selectTile: (tileId: string) => void;
  focusTile: (tileId: string) => void;
  toggleTilePin: (tileId: string) => void;
  fineTuneTile: (tileId: string, deltaMs: number) => void;
  setTileManualFrame: (tileId: string, frameIndex: number) => void;
  resizeTile: (tileId: string, rowSpan: number, columnSpan: number) => void;
  runSharpestNeighbour: () => Promise<void>;
  saveProjectToPath: (path: string) => Promise<void>;
  loadProjectFromPath: (path: string) => Promise<void>;
  exportProjectToPath: (path?: string) => Promise<string | null>;
  refreshPreviewFrame: (maxWidth?: number) => Promise<void>;
  refreshSheetPreview: (maxWidth?: number) => Promise<void>;
}

const starterProject = createStarterProject();
let previewRequestSequence = 0;
let sheetPreviewRequestSequence = 0;
let playbackRequestSequence = 0;
let managedVideoUrl: string | null = null;

/** Keeps browser-only diagnostics available when the Rust shell is not active. */
const localDiagnostics = (project: ProjectFile): DiagnosticsBundle => buildDiagnosticsBundle(project);

const errorMessage = (error: unknown, fallback: string): string => {
  if (error instanceof Error) {
    return error.message;
  }

  if (typeof error === "string") {
    return error;
  }

  if (
    error &&
    typeof error === "object" &&
    "message" in error &&
    typeof error.message === "string"
  ) {
    return error.message;
  }

  return fallback;
};

const replaceManagedVideoUrl = (url: string | null): string | null => {
  if (managedVideoUrl && managedVideoUrl !== url) {
    URL.revokeObjectURL(managedVideoUrl);
  }

  managedVideoUrl = url?.startsWith("blob:") ? url : null;
  return url;
};

const resolvePlaybackUrl = async (
  path: string,
): Promise<{ url: string; alert: Exclude<EditorAlert, null> }> => {
  try {
    const playback = await prepareVideoPlayback(path);
    return {
      url: replaceManagedVideoUrl(playbackBlobUrl(playback)) ?? projectFileUrl(path),
      alert: { tone: "info", message: `Playback ready for ${path}` },
    };
  } catch (error) {
    return {
      url: replaceManagedVideoUrl(projectFileUrl(path)) ?? projectFileUrl(path),
      alert: {
        tone: "warning",
        message: `${errorMessage(
          error,
          "Failed to prepare preview playback.",
        )} Falling back to direct playback.`,
      },
    };
  }
};

/** Recomputes derived store fields after any project mutation. */
const syncState = (project: ProjectFile, selectedTileId: string | null = null) => ({
  project,
  diagnostics: localDiagnostics(project),
  selectedTileId: project.tiles.some((tile) => tile.id === selectedTileId)
    ? selectedTileId
    : project.tiles[0]?.id ?? null,
});

const clampGrid = (value: number): number => Math.min(12, Math.max(1, Math.round(value)));

const clampGridSpacing = (value: number): number => Math.max(0, Math.round(value));

const normalizedGrid = (grid: GridSettings): GridSettings => ({
  ...grid,
  rows: clampGrid(grid.rows),
  columns: clampGrid(grid.columns),
  gutterPx: clampGridSpacing(grid.gutterPx),
  outerMarginPx: clampGridSpacing(grid.outerMarginPx),
});

const normalizedStyle = (style: ProjectStyle): ProjectStyle => ({
  ...style,
  frameRoundingPx: clampGridSpacing(style.frameRoundingPx),
  frameShadowPx: clampGridSpacing(style.frameShadowPx),
  frameBorderPx: clampGridSpacing(style.frameBorderPx),
});

const clampSampleStart = (startMs: number, endMs: number, sampleStartMs: number): number =>
  Math.min(Math.max(sampleStartMs, startMs), endMs);

const sampleStartOffsetMs = (project: ProjectFile): number =>
  Math.max(0, project.range.sampleStartMs - project.range.startMs);

const resetTileSelections = (tiles: ProjectFile["tiles"]): ProjectFile["tiles"] =>
  tiles.map((tile) => ({
    ...tile,
    selection: { kind: "auto" as const },
    pinned: false,
    fineTuneOffsetMs: 0,
  }));

const inheritImportedProject = (current: ProjectFile, imported: ProjectFile): ProjectFile => {
  const grid = normalizedGrid({ ...imported.grid, ...current.grid });
  const range = {
    ...imported.range,
    startMs: 0,
    sampleStartMs: 0,
  };
  const layoutTiles =
    current.tiles.length > 0
      ? resetTileSelections(current.tiles)
      : resetTileSelections(reshapeTilesForGrid(imported.tiles, grid.rows, grid.columns));

  return {
    ...imported,
    analysisMode: current.analysisMode,
    playback: {
      ...imported.playback,
      customSkipMs: current.playback.customSkipMs,
      frameStep: current.playback.frameStep,
    },
    range,
    grid,
    tiles: autoFillTiles(
      layoutTiles,
      range.startMs,
      range.endMs,
      range.sampleStartMs,
      imported.video.fps,
      imported.video.frameCount,
    ),
    style: current.style,
    watermark: current.watermark,
    export: current.export,
    batch: current.batch,
  };
};

const resolvedTileTimeMs = (
  project: ProjectFile,
  tile: ProjectFile["tiles"][number],
): number => {
  const durationMs = project.video.durationMs ?? 60_000;
  const baseTimeMs =
    tile.selection.kind === "manual"
      ? seekTimeForFrameIndex(tile.selection.frameIndex, project.video.fps, durationMs)
      : 0;

  return clampPlayheadMs(baseTimeMs + tile.fineTuneOffsetMs, Math.max(durationMs - 1, 0));
};

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
  playbackPreparing: false,
  loadedVideoSizeBytes: 0,
  previewFrame: null,
  previewBusy: false,
  previewError: null,
  sheetPreview: null,
  sheetPreviewBusy: false,
  sheetPreviewError: null,
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
      loadedVideoUrl: replaceManagedVideoUrl(URL.createObjectURL(file)),
      playbackPreparing: false,
      loadedVideoSizeBytes: file.size,
      previewFrame: null,
      previewBusy: false,
      previewError: null,
      sheetPreview: null,
      sheetPreviewBusy: false,
      sheetPreviewError: null,
      ...syncState(nextProject, get().selectedTileId),
      alert: { tone: "info", message: `Loaded ${file.name}` },
    });
  },
  loadVideoFromPath: async (path) => {
    playbackRequestSequence += 1;
    set({
      busy: true,
      alert: null,
      previewFrame: null,
      previewBusy: false,
      previewError: null,
      sheetPreview: null,
      sheetPreviewBusy: false,
      sheetPreviewError: null,
    });
    try {
      const project = inheritImportedProject(get().project, await probeVideo(path));
      const initialVideoUrl = isDesktopRuntime()
        ? replaceManagedVideoUrl(null)
        : replaceManagedVideoUrl(path);
      const requestId = playbackRequestSequence;
      set({
        busy: false,
        loadedVideoUrl: initialVideoUrl,
        playbackPreparing: isDesktopRuntime(),
        loadedVideoSizeBytes: 0,
        previewFrame: null,
        previewBusy: false,
        previewError: null,
        sheetPreview: null,
        sheetPreviewBusy: false,
        sheetPreviewError: null,
        ...syncState(project, get().selectedTileId),
        alert: {
          tone: "info",
          message: `Loaded ${path}`,
        },
      });

      if (isDesktopRuntime()) {
        void resolvePlaybackUrl(path).then(({ url, alert }) => {
          if (requestId !== playbackRequestSequence) {
            return;
          }

          set({
            loadedVideoUrl: url,
            playbackPreparing: false,
            alert,
          });
        });
      }
    } catch (error) {
      set({
        busy: false,
        playbackPreparing: false,
        alert: {
          tone: "warning",
          message: errorMessage(error, "Failed to load video."),
        },
      });
    }
  },
  ensurePlaybackReady: async () => {
    const { loadedVideoUrl, playbackPreparing, project } = get();
    if (loadedVideoUrl) {
      return true;
    }

    if (!project.video.path || project.video.path === "unloaded-video.mp4") {
      return false;
    }

    if (!isDesktopRuntime()) {
      set({
        loadedVideoUrl: replaceManagedVideoUrl(project.video.path),
        playbackPreparing: false,
      });
      return true;
    }

    if (playbackPreparing) {
      return false;
    }

    const requestId = playbackRequestSequence + 1;
    playbackRequestSequence = requestId;
    set({
      playbackPreparing: true,
      alert: { tone: "info", message: `Preparing preview playback for ${project.video.path}...` },
    });

    const { url, alert } = await resolvePlaybackUrl(project.video.path);
    if (requestId !== playbackRequestSequence) {
      return false;
    }

    set({
      loadedVideoUrl: url,
      playbackPreparing: false,
      alert,
    });
    return true;
  },
  setDurationMs: (durationMs) => {
    const previousOffsetMs = sampleStartOffsetMs(get().project);
    const nextProject = {
      ...get().project,
      video: {
        ...get().project.video,
        durationMs,
        frameCount: Math.round((durationMs / 1000) * (get().project.video.fps ?? 24)),
      },
      range: {
        ...get().project.range,
        endMs: durationMs,
        sampleStartMs: clampSampleStart(
          get().project.range.startMs,
          durationMs,
          get().project.range.startMs + previousOffsetMs,
        ),
      },
    };
    set(syncState(nextProject, get().selectedTileId));
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
        activeFrameIndex: clampFrameIndex(
          frameIndexAtTimeMs(nextPlayheadMs, fps),
          get().project.video.frameCount,
        ),
      },
    };
    set(syncState(nextProject, get().selectedTileId));
  },
  setRangeStart: (startMs) => {
    const { project } = get();
    const previousOffsetMs = sampleStartOffsetMs(project);
    const nextStartMs = Math.min(startMs, project.range.endMs - 250);
    const nextProject = {
      ...project,
      range: {
        startMs: nextStartMs,
        endMs: project.range.endMs,
        sampleStartMs: clampSampleStart(
          nextStartMs,
          project.range.endMs,
          nextStartMs + previousOffsetMs,
        ),
      },
    };
    set(syncState(nextProject, get().selectedTileId));
  },
  setRangeEnd: (endMs) => {
    const { project } = get();
    const durationMs = project.video.durationMs ?? 60_000;
    const nextEndMs = Math.max(Math.min(endMs, durationMs), project.range.startMs + 250);
    const previousOffsetMs = sampleStartOffsetMs(project);
    const nextProject = {
      ...project,
      range: {
        startMs: project.range.startMs,
        endMs: nextEndMs,
        sampleStartMs: clampSampleStart(
          project.range.startMs,
          nextEndMs,
          project.range.startMs + previousOffsetMs,
        ),
      },
    };
    set(syncState(nextProject, get().selectedTileId));
  },
  setSampleStartOffset: (offsetMs) => {
    const { project } = get();
    const rangeSpanMs = Math.max(0, project.range.endMs - project.range.startMs);
    const nextProject = {
      ...project,
      range: {
        ...project.range,
        sampleStartMs: clampSampleStart(
          project.range.startMs,
          project.range.endMs,
          project.range.startMs + Math.min(Math.max(offsetMs, 0), rangeSpanMs),
        ),
      },
    };
    set(syncState(nextProject, get().selectedTileId));
  },
  setCustomSkip: (value) => {
    try {
      const customSkipMs = parseTimeDelta(value);
      const nextProject = {
        ...get().project,
        playback: { ...get().project.playback, customSkipMs },
      };
      set({
        ...syncState(nextProject, get().selectedTileId),
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
    set(syncState(nextProject, get().selectedTileId));
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
      ...syncState(nextProject, get().selectedTileId),
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
        project.range.sampleStartMs,
        project.video.fps,
        project.video.frameCount,
      ),
    };
    set(syncState(nextProject, get().selectedTileId));
  },
  setGridDimensions: (rows, columns) => {
    const { project, selectedTileId } = get();
    const grid = normalizedGrid({ ...project.grid, rows, columns });
    const reshapedTiles = reshapeTilesForGrid(project.tiles, grid.rows, grid.columns);
    const nextProject = {
      ...project,
      grid,
      tiles: autoFillTiles(
        reshapedTiles,
        project.range.startMs,
        project.range.endMs,
        project.range.sampleStartMs,
        project.video.fps,
        project.video.frameCount,
      ),
    };
    set(syncState(nextProject, selectedTileId));
  },
  setGridSpacing: (gutterPx, outerMarginPx) => {
    const { project, selectedTileId } = get();
    const nextProject = {
      ...project,
      grid: normalizedGrid({
        ...project.grid,
        gutterPx,
        outerMarginPx,
      }),
    };
    set(syncState(nextProject, selectedTileId));
  },
  updateStyle: (style) => {
    const { project, selectedTileId } = get();
    const nextProject = {
      ...project,
      style: normalizedStyle({
        ...project.style,
        ...style,
      }),
    };
    set(syncState(nextProject, selectedTileId));
  },
  setExportFormat: (format) => {
    const { project, selectedTileId } = get();
    const nextProject = {
      ...project,
      export: {
        ...project.export,
        format,
      },
    };
    set(syncState(nextProject, selectedTileId));
  },
  setExportScale: (scale) => {
    const { project, selectedTileId } = get();
    const nextProject = {
      ...project,
      export: {
        ...project.export,
        scale: Math.max(0.25, Number.isFinite(scale) ? scale : project.export.scale),
      },
    };
    set(syncState(nextProject, selectedTileId));
  },
  setWatermarkText: (value) => {
    const { project, selectedTileId } = get();
    const nextProject = {
      ...project,
      watermark: {
        ...project.watermark,
        text: {
          value,
          opacity: project.watermark.text?.opacity ?? 0.55,
        },
      },
    };
    set(syncState(nextProject, selectedTileId));
  },
  selectTile: (tileId) => set({ selectedTileId: tileId }),
  focusTile: (tileId) => {
    const { project } = get();
    const tile = project.tiles.find((entry) => entry.id === tileId);
    if (!tile) {
      return;
    }

    get().setPlayheadMs(resolvedTileTimeMs(project, tile));
    set({ selectedTileId: tileId });
  },
  toggleTilePin: (tileId) => {
    const nextProject = {
      ...get().project,
      tiles: get().project.tiles.map((tile) =>
        tile.id === tileId ? { ...tile, pinned: !tile.pinned } : tile,
      ),
    };
    set(syncState(nextProject, get().selectedTileId));
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
    set(syncState(nextProject, get().selectedTileId));
  },
  setTileManualFrame: (tileId, frameIndex) => {
    const fps = get().project.video.fps ?? 24;
    const normalizedFrameIndex = clampFrameIndex(frameIndex, get().project.video.frameCount);
    const timeMs = seekTimeForFrameIndex(
      normalizedFrameIndex,
      fps,
      get().project.video.durationMs ?? 60_000,
    );
    const nextProject = {
      ...get().project,
      tiles: get().project.tiles.map((tile) =>
        tile.id === tileId
          ? {
              ...tile,
              selection: { kind: "manual" as const, frameIndex: normalizedFrameIndex, timeMs },
              pinned: true,
            }
          : tile,
      ),
    };
    set(syncState(nextProject, tileId));
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
    set(syncState(nextProject, tileId));
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
        ...syncState(nextProject, selectedTileId),
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
    playbackRequestSequence += 1;
    set({ busy: true, alert: null });
    try {
      const project = await loadProject(path);
      const diagnostics = isDesktopRuntime()
        ? await diagnosticsBundle(project)
        : localDiagnostics(project);
      const initialVideoUrl =
        isDesktopRuntime() || !project.video.path
          ? replaceManagedVideoUrl(null)
          : replaceManagedVideoUrl(project.video.path);
      const requestId = playbackRequestSequence;
      set({
        busy: false,
        loadedVideoUrl: initialVideoUrl,
        playbackPreparing: isDesktopRuntime() && Boolean(project.video.path),
        previewFrame: null,
        previewBusy: false,
        previewError: null,
        sheetPreview: null,
        sheetPreviewBusy: false,
        sheetPreviewError: null,
        ...syncState(project, get().selectedTileId),
        diagnostics,
        alert: {
          tone: "info",
          message: `Loaded project from ${path}`,
        },
      });

      if (isDesktopRuntime() && project.video.path) {
        void resolvePlaybackUrl(project.video.path).then(({ url, alert }) => {
          if (requestId !== playbackRequestSequence) {
            return;
          }

          set({
            loadedVideoUrl: url,
            playbackPreparing: false,
            alert,
          });
        });
      }
    } catch (error) {
      set({
        busy: false,
        playbackPreparing: false,
        alert: {
          tone: "warning",
          message: errorMessage(error, "Failed to load project."),
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
          message: errorMessage(error, "Export failed."),
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

      const correctedProject =
        previewFrame.timeMs !== project.playback.playheadMs ||
        previewFrame.frameIndex !== project.playback.activeFrameIndex
          ? {
              ...project,
              playback: {
                ...project.playback,
                playheadMs: previewFrame.timeMs,
                activeFrameIndex: previewFrame.frameIndex,
              },
            }
          : project;

      set({
        previewFrame,
        previewBusy: false,
        previewError: null,
        ...syncState(correctedProject, get().selectedTileId),
      });
    } catch (error) {
      if (requestId !== previewRequestSequence) {
        return;
      }

      set({
        previewBusy: false,
        previewError: errorMessage(error, "Failed to refresh preview."),
      });
    }
  },
  refreshSheetPreview: async (maxWidth = 1080) => {
    if (!isDesktopRuntime()) {
      set({ sheetPreview: null, sheetPreviewBusy: false, sheetPreviewError: null });
      return;
    }

    const { project } = get();
    if (!project.video.path || project.video.path === "unloaded-video.mp4") {
      set({ sheetPreview: null, sheetPreviewBusy: false, sheetPreviewError: null });
      return;
    }

    const requestId = sheetPreviewRequestSequence + 1;
    sheetPreviewRequestSequence = requestId;
    set({ sheetPreviewBusy: true, sheetPreviewError: null });

    try {
      const sheetPreview = await fetchSheetPreview(project, maxWidth);
      if (requestId !== sheetPreviewRequestSequence) {
        return;
      }

      set({
        sheetPreview,
        sheetPreviewBusy: false,
        sheetPreviewError: null,
      });
    } catch (error) {
      if (requestId !== sheetPreviewRequestSequence) {
        return;
      }

      set({
        sheetPreviewBusy: false,
        sheetPreviewError: errorMessage(error, "Failed to refresh sheet preview."),
      });
    }
  },
}));
