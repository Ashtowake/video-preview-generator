export type AnalysisMode = "quick_preview" | "full_fidelity";

export interface VideoSource {
  path: string;
  durationMs?: number;
  fps?: number;
  width?: number;
  height?: number;
}

export interface PlaybackSettings {
  playheadMs: number;
  activeFrameIndex: number;
  customSkipMs: number;
  frameStep: number;
}

export interface TimeRange {
  startMs: number;
  endMs: number;
}

export interface GridSettings {
  rows: number;
  columns: number;
  gutterPx: number;
  outerMarginPx: number;
  defaultSharpnessWindow: number;
}

export type TileSelection =
  | { kind: "auto" }
  | { kind: "manual"; frameIndex: number; timeMs: number };

export interface TileSpan {
  row: number;
  column: number;
  rowSpan: number;
  columnSpan: number;
}

export interface ProjectTile {
  id: string;
  order: number;
  span: TileSpan;
  selection: TileSelection;
  pinned: boolean;
  fineTuneOffsetMs: number;
}

export interface ProjectStyle {
  frameRoundingPx: number;
  frameShadowPx: number;
  frameBorderPx: number;
  showMetadataBar: boolean;
  showTimestamps: boolean;
  darkMode: boolean;
}

export interface WatermarkText {
  value: string;
  opacity: number;
}

export interface WatermarkImage {
  path: string;
  opacity: number;
}

export interface WatermarkSettings {
  text?: WatermarkText;
  image?: WatermarkImage;
}

export type ExportFormat = "png" | "jpeg";

export interface ExportSettings {
  format: ExportFormat;
  scale: number;
  outputPath?: string;
}

export interface BatchSettings {
  enabled: boolean;
  retainManualOverrides: boolean;
  inputs: string[];
}

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

export interface DiagnosticsBundle {
  appVersion: string;
  analysisMode: AnalysisMode;
  videoPath: string;
  cacheDirectory: string;
  notices: { name: string; license: string; url: string }[];
}

export interface DecodeEstimate {
  projectedMemoryMb: number;
  projectedCacheMb: number;
  canUpgradeToFullFidelity: boolean;
  reason?: string;
}

export interface ValidationIssue {
  level: "error" | "warning";
  path: string;
  message: string;
}
