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
  type ProjectFile,
} from "@video-preview/domain";
import { create } from "zustand";

type EditorAlert = { tone: "info" | "warning"; message: string } | null;

interface EditorState {
  project: ProjectFile;
  selectedTileId: string | null;
  loadedVideoUrl: string | null;
  loadedVideoSizeBytes: number;
  alert: EditorAlert;
  diagnostics: DiagnosticsBundle;
  setLoadedVideo: (file: File) => void;
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
  runSharpestNeighbour: () => void;
}

const starterProject = createStarterProject();

const syncDiagnostics = (project: ProjectFile): DiagnosticsBundle => buildDiagnosticsBundle(project);

export const useEditorStore = create<EditorState>((set, get) => ({
  project: starterProject,
  selectedTileId: starterProject.tiles[0]?.id ?? null,
  loadedVideoUrl: null,
  loadedVideoSizeBytes: 0,
  alert: null,
  diagnostics: syncDiagnostics(starterProject),
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
      project: nextProject,
      diagnostics: syncDiagnostics(nextProject),
      alert: { tone: "info", message: `Loaded ${file.name}` },
    });
  },
  setDurationMs: (durationMs) => {
    const nextProject = {
      ...get().project,
      video: { ...get().project.video, durationMs },
      range: { ...get().project.range, endMs: durationMs },
    };
    set({ project: nextProject, diagnostics: syncDiagnostics(nextProject) });
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
    set({ project: nextProject, diagnostics: syncDiagnostics(nextProject) });
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
    set({ project: nextProject, diagnostics: syncDiagnostics(nextProject) });
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
    set({ project: nextProject, diagnostics: syncDiagnostics(nextProject) });
  },
  setCustomSkip: (value) => {
    try {
      const customSkipMs = parseTimeDelta(value);
      const nextProject = {
        ...get().project,
        playback: { ...get().project.playback, customSkipMs },
      };
      set({
        project: nextProject,
        diagnostics: syncDiagnostics(nextProject),
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
    set({ project: nextProject, diagnostics: syncDiagnostics(nextProject) });
  },
  stepCustom: (direction) => {
    const { project } = get();
    const durationMs = project.video.durationMs ?? 60_000;
    get().setPlayheadMs(stepByTime(project.playback.playheadMs, project.playback.customSkipMs * direction, durationMs));
  },
  stepSeconds: (seconds, direction) => {
    const { project } = get();
    const durationMs = project.video.durationMs ?? 60_000;
    get().setPlayheadMs(stepByTime(project.playback.playheadMs, seconds * 1000 * direction, durationMs));
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
      project: nextProject,
      diagnostics: syncDiagnostics(nextProject),
      alert: {
        tone: "info",
        message:
          mode === "full_fidelity"
            ? "Full fidelity mode enabled. Exact frame tools can be wired to the backend."
            : "Quick preview mode keeps memory and cache use low.",
      },
    });
  },
  autoFill: () => {
    const { project } = get();
    const nextProject = {
      ...project,
      tiles: autoFillTiles(project.tiles, project.range.startMs, project.range.endMs, project.video.fps),
    };
    set({ project: nextProject, diagnostics: syncDiagnostics(nextProject) });
  },
  selectTile: (tileId) => set({ selectedTileId: tileId }),
  toggleTilePin: (tileId) => {
    const nextProject = {
      ...get().project,
      tiles: get().project.tiles.map((tile) =>
        tile.id === tileId ? { ...tile, pinned: !tile.pinned } : tile,
      ),
    };
    set({ project: nextProject, diagnostics: syncDiagnostics(nextProject) });
  },
  fineTuneTile: (tileId, deltaMs) => {
    const nextProject = {
      ...get().project,
      tiles: get().project.tiles.map((tile) =>
        tile.id === tileId ? { ...tile, fineTuneOffsetMs: tile.fineTuneOffsetMs + deltaMs } : tile,
      ),
    };
    set({ project: nextProject, diagnostics: syncDiagnostics(nextProject) });
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
    set({ project: nextProject, diagnostics: syncDiagnostics(nextProject) });
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
    const overlapIssue = issues.find((issue) => issue.path === `tiles.${tileId}`);
    if (overlapIssue) {
      set({ alert: { tone: "warning", message: overlapIssue.message } });
      return;
    }
    set({ project: nextProject, diagnostics: syncDiagnostics(nextProject) });
  },
  runSharpestNeighbour: () => {
    const { project, selectedTileId } = get();
    if (!selectedTileId) {
      set({ alert: { tone: "warning", message: "Select a tile first." } });
      return;
    }

    const windowSize = project.grid.defaultSharpnessWindow;
    const nextProject = {
      ...project,
      tiles: project.tiles.map((tile) => {
        if (tile.id !== selectedTileId || tile.selection.kind !== "manual") {
          return tile;
        }

        // Placeholder heuristic until the FFmpeg-backed sharpness scan lands.
        const nudgedFrame = tile.selection.frameIndex + Math.max(1, Math.round(windowSize / 3));
        return {
          ...tile,
          selection: {
            kind: "manual" as const,
            frameIndex: nudgedFrame,
            timeMs: Math.round((nudgedFrame / (project.video.fps ?? 24)) * 1000),
          },
        };
      }),
    };

    set({
      project: nextProject,
      diagnostics: syncDiagnostics(nextProject),
      alert: {
        tone: "warning",
        message:
          "Sharpest neighbour currently uses a temporary heuristic. Wire the FFmpeg-backed analyzer next.",
      },
    });
  },
}));
