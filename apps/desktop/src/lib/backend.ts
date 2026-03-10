/**
 * Thin wrappers around Tauri APIs so UI components stay focused on presentation instead of IPC.
 */
import { convertFileSrc, invoke, isTauri } from "@tauri-apps/api/core";
import { open, save } from "@tauri-apps/plugin-dialog";
import type {
  DecodeEstimate,
  DiagnosticsBundle,
  ExportResult,
  PreviewFrame,
  ProjectFile,
} from "@video-preview/domain";

/** Returns `true` when the app is running inside the Tauri desktop shell. */
export const isDesktopRuntime = (): boolean => isTauri();

/** Converts a local file path into a URL that the webview can load. */
export const projectFileUrl = (path: string): string =>
  isDesktopRuntime() ? convertFileSrc(path) : path;

/** Opens the native file dialog for selecting a source video. */
export const openVideoDialog = async (): Promise<string | null> => {
  if (!isDesktopRuntime()) {
    return null;
  }

  const selected = await open({
    multiple: false,
    filters: [
      {
        name: "Videos",
        extensions: ["mp4", "mov", "mkv", "avi", "webm", "m4v"],
      },
    ],
  });

  return typeof selected === "string" ? selected : null;
};

/** Opens the native file dialog for loading a saved project file. */
export const openProjectDialog = async (): Promise<string | null> => {
  if (!isDesktopRuntime()) {
    return null;
  }

  const selected = await open({
    multiple: false,
    filters: [
      {
        name: "Video Preview Project",
        extensions: ["json"],
      },
    ],
  });

  return typeof selected === "string" ? selected : null;
};

/** Opens the native save dialog for project or export destinations. */
export const savePathDialog = async (
  defaultPath: string,
  filters: { name: string; extensions: string[] }[],
): Promise<string | null> => {
  if (!isDesktopRuntime()) {
    return null;
  }

  return save({
    defaultPath,
    filters,
  });
};

/** Probes a video on the Rust side and returns a populated starter project. */
export const probeVideo = async (videoPath: string): Promise<ProjectFile> =>
  invoke("probe_video", { videoPath });

/** Persists a project JSON file through the Rust shell. */
export const saveProject = async (path: string, project: ProjectFile): Promise<void> =>
  invoke("save_project", { path, project });

/** Loads a saved project JSON file through the Rust shell. */
export const loadProject = async (path: string): Promise<ProjectFile> =>
  invoke("load_project", { path });

/** Estimates the resource footprint of enabling full-fidelity mode. */
export const estimateProject = async (project: ProjectFile): Promise<DecodeEstimate> =>
  invoke("estimate_full_fidelity", { project });

/** Requests a PNG preview frame for the given project time. */
export const fetchPreviewFrame = async (
  project: ProjectFile,
  timeMs: number,
  maxWidth = 640,
): Promise<PreviewFrame> => invoke("preview_frame", { project, timeMs, maxWidth });

/** Runs the sharpness-neighbour search for the selected tile ids. */
export const sharpestNeighbours = async (
  project: ProjectFile,
  tileIds: string[],
): Promise<ProjectFile> => invoke("find_sharpest_neighbours", { project, tileIds: tileIds });

/** Renders and writes the final exported sheet image. */
export const exportProject = async (
  project: ProjectFile,
  outputPath?: string,
): Promise<ExportResult> => invoke("export_project", { project, outputPath });

/** Builds a support-oriented diagnostics bundle from the current project. */
export const diagnosticsBundle = async (
  project: ProjectFile,
): Promise<DiagnosticsBundle> => invoke("diagnostics_bundle", { project });
