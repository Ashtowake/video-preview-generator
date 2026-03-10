/** Runtime mode for video interaction and decode depth. */
export type AnalysisMode = "quick_preview" | "full_fidelity";

/** Source video metadata shared between the desktop UI, Rust core, and project files. */
export interface VideoSource {
  path: string;
  durationMs?: number;
  fps?: number;
  frameCount?: number;
  width?: number;
  height?: number;
}

/** Transport state kept with the project so seeking survives save/load. */
export interface PlaybackSettings {
  playheadMs: number;
  activeFrameIndex: number;
  customSkipMs: number;
  frameStep: number;
}

/** Inclusive working range used for grid sampling and export. */
export interface TimeRange {
  startMs: number;
  endMs: number;
  sampleStartMs: number;
}

/** Sheet-wide grid settings and sharpness window defaults. */
export interface GridSettings {
  rows: number;
  columns: number;
  gutterPx: number;
  outerMarginPx: number;
  defaultSharpnessWindow: number;
}

/** Per-tile frame selection, either auto-generated or explicitly pinned by the user. */
export type TileSelection =
  | { kind: "auto" }
  | { kind: "manual"; frameIndex: number; timeMs: number };

/** Grid occupancy for a tile, including support for row/column spanning. */
export interface TileSpan {
  row: number;
  column: number;
  rowSpan: number;
  columnSpan: number;
}

/** One cell or spanning block inside the sheet layout. */
export interface ProjectTile {
  id: string;
  order: number;
  span: TileSpan;
  selection: TileSelection;
  pinned: boolean;
  fineTuneOffsetMs: number;
}

/** Global frame and sheet presentation settings. */
export interface ProjectStyle {
  frameRoundingPx: number;
  frameShadowPx: number;
  frameBorderPx: number;
  showMetadataBar: boolean;
  showTimestamps: boolean;
  darkMode: boolean;
}

/** Text watermark configuration rendered into the final output. */
export interface WatermarkText {
  value: string;
  opacity: number;
}

/** Image watermark configuration rendered into the final output. */
export interface WatermarkImage {
  path: string;
  opacity: number;
}

/** Optional watermark layers used by the renderer. */
export interface WatermarkSettings {
  text?: WatermarkText;
  image?: WatermarkImage;
}

/** Supported raster export formats for rendered sheets. */
export type ExportFormat = "png" | "jpeg";

/** Output settings shared by the desktop UI, CLI, and renderer. */
export interface ExportSettings {
  format: ExportFormat;
  scale: number;
  outputPath?: string;
}

/** Shared settings for future multi-input export workflows. */
export interface BatchSettings {
  enabled: boolean;
  retainManualOverrides: boolean;
  inputs: string[];
}

/** Versioned project file persisted as `.vpg.json`. */
export interface ProjectFile {
  version: number;
  analysisMode: AnalysisMode;
  video: VideoSource;
  playback: PlaybackSettings;
  range: TimeRange;
  grid: GridSettings;
  tiles: ProjectTile[];
  style: ProjectStyle;
  watermark: WatermarkSettings;
  export: ExportSettings;
  batch: BatchSettings;
}

/** Support-oriented snapshot of local app state and bundled notices. */
export interface DiagnosticsBundle {
  appVersion: string;
  analysisMode: AnalysisMode;
  videoPath: string;
  cacheDirectory: string;
  notices: { name: string; license: string; url: string }[];
}

/** Safety estimate shown before upgrading to full-fidelity mode. */
export interface DecodeEstimate {
  projectedMemoryMb: number;
  projectedCacheMb: number;
  canUpgradeToFullFidelity: boolean;
  reason?: string;
}

/** Inline preview image returned from the Rust backend. */
export interface PreviewFrame {
  dataUrl: string;
  timeMs: number;
  frameIndex: number;
}

/** Successful export metadata returned after rendering. */
export interface ExportResult {
  outputPath: string;
}

/** Validation issue produced by optimistic frontend checks. */
export interface ValidationIssue {
  level: "error" | "warning";
  path: string;
  message: string;
}
