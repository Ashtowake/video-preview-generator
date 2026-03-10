import type { DecodeEstimate, DiagnosticsBundle, ProjectFile, ValidationIssue } from "./types";
import { validateGridSpans } from "./grid";

export const validateProject = (project: ProjectFile): ValidationIssue[] => {
  const issues: ValidationIssue[] = [];

  if (project.range.endMs <= project.range.startMs) {
    issues.push({
      level: "error",
      path: "range",
      message: "Range end must be greater than range start.",
    });
  }

  if (project.playback.frameStep < 1) {
    issues.push({
      level: "error",
      path: "playback.frameStep",
      message: "Frame step must be at least 1.",
    });
  }

  if (project.analysisMode === "quick_preview" && project.playback.frameStep > 1) {
    issues.push({
      level: "warning",
      path: "analysisMode",
      message: "Exact frame stepping stays approximate until full fidelity mode is enabled.",
    });
  }

  return issues.concat(validateGridSpans(project.tiles, project.grid));
};

export const estimateFullFidelity = (
  project: ProjectFile,
  fileSizeBytes = 0,
): DecodeEstimate => {
  const width = project.video.width ?? 1920;
  const height = project.video.height ?? 1080;
  const durationSeconds = (project.video.durationMs ?? 60_000) / 1000;
  const fps = project.video.fps ?? 24;
  const projectedMemoryMb = Math.max(32, Math.ceil((width * height * 4) / 1_048_576));
  const projectedCacheMb = Math.max(
    64,
    Math.ceil((durationSeconds * fps) / 8) + Math.ceil(fileSizeBytes / 1_048_576 / 4),
  );
  const canUpgradeToFullFidelity = projectedCacheMb < 2_048;

  return {
    projectedMemoryMb,
    projectedCacheMb,
    canUpgradeToFullFidelity,
    reason: canUpgradeToFullFidelity
      ? undefined
      : "Estimated cache size exceeds the current safety limit.",
  };
};

export const buildDiagnosticsBundle = (project: ProjectFile): DiagnosticsBundle => ({
  appVersion: "0.1.0",
  analysisMode: project.analysisMode,
  videoPath: project.video.path,
  cacheDirectory: "./cache",
  notices: [
    {
      name: "FFmpeg",
      license: "LGPL/GPL depending on distribution",
      url: "https://ffmpeg.org/legal.html",
    },
    {
      name: "Tauri",
      license: "MIT OR Apache-2.0",
      url: "https://v2.tauri.app",
    },
  ],
});
