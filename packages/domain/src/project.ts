import type {
  BatchSettings,
  GridSettings,
  PlaybackSettings,
  ProjectFile,
  ProjectStyle,
  ProjectTile,
  WatermarkSettings,
} from "./types";

const defaultGrid = (): GridSettings => ({
  rows: 4,
  columns: 5,
  gutterPx: 12,
  outerMarginPx: 24,
  defaultSharpnessWindow: 12,
});

const defaultPlayback = (): PlaybackSettings => ({
  playheadMs: 0,
  activeFrameIndex: 0,
  customSkipMs: 10_000,
  frameStep: 1,
});

const defaultStyle = (): ProjectStyle => ({
  frameRoundingPx: 20,
  frameShadowPx: 18,
  frameBorderPx: 2,
  showMetadataBar: true,
  showTimestamps: true,
  darkMode: true,
});

const defaultWatermark = (): WatermarkSettings => ({
  text: { value: "Preview", opacity: 0.55 },
});

const defaultBatch = (): BatchSettings => ({
  enabled: false,
  retainManualOverrides: false,
  inputs: [],
});

const autoTile = (order: number): ProjectTile => ({
  id: `tile-${order}`,
  order,
  span: { row: Math.floor(order / 5), column: order % 5, rowSpan: 1, columnSpan: 1 },
  selection: { kind: "auto" },
  pinned: false,
  fineTuneOffsetMs: 0,
});

/** Creates a new starter project for a single imported video. */
export const createStarterProject = (videoPath = "unloaded-video.mp4"): ProjectFile => {
  const grid = defaultGrid();
  const tileCount = grid.rows * grid.columns;

  return {
    version: 1,
    analysisMode: "quick_preview",
    video: {
      path: videoPath,
      durationMs: 60_000,
      fps: 24,
      frameCount: 1_440,
      width: 1920,
      height: 1080,
    },
    playback: defaultPlayback(),
    range: { startMs: 0, endMs: 60_000, sampleStartMs: 0 },
    grid,
    tiles: Array.from({ length: tileCount }, (_, index) => autoTile(index)),
    style: defaultStyle(),
    watermark: defaultWatermark(),
    export: { format: "png", scale: 1 },
    batch: defaultBatch(),
  };
};
