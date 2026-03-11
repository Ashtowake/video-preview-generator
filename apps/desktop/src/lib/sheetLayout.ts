import {
  inferTileLabel,
  seekTimeForFrameIndex,
  type ProjectFile,
  type ProjectTile,
} from "@video-preview/domain";

/** Simple display label for local paths shown in the UI. */
export const displayPathName = (path: string): string => {
  const trimmed = path.trim();
  if (!trimmed) {
    return "Untitled";
  }

  const segments = trimmed.split(/[/\\]/);
  return segments.at(-1) || trimmed;
};

/** Layout metrics shared by the interactive sheet preview overlay. */
export interface SheetMetrics {
  canvasWidth: number;
  canvasHeight: number;
  cellWidth: number;
  cellHeight: number;
  gutter: number;
  outer: number;
  metadataHeight: number;
}

/** Click target that sits over a rendered tile preview. */
export interface SheetTileHitbox {
  tileId: string;
  xPct: number;
  yPct: number;
  widthPct: number;
  heightPct: number;
  label: string;
  timeMs: number;
  pinned: boolean;
}

const sourceAspectRatio = (project: ProjectFile): number => {
  const width = project.video.width ?? 1920;
  const height = project.video.height ?? 1080;
  return Math.max(width / height, 0.25);
};

const deriveCellSize = (scale: number, aspectRatio: number): [number, number] => {
  let width = Math.max(Math.round(240 * scale), 96);
  let height = width / aspectRatio;
  if (height > 240 * scale) {
    height = Math.round(240 * scale);
    width = height * aspectRatio;
  }

  return [Math.round(width), Math.round(height)];
};

/** Rebuilds the same coarse sheet geometry used by the Rust renderer. */
export const deriveSheetMetrics = (project: ProjectFile): SheetMetrics => {
  const scale = Math.max(project.export.scale, 0.25);
  const [cellWidth, cellHeight] = deriveCellSize(scale, sourceAspectRatio(project));
  const gutter = Math.round(project.grid.gutterPx * scale);
  const outer = Math.round(project.grid.outerMarginPx * scale);
  const metadataHeight = project.style.showMetadataBar ? Math.round(72 * scale) : 0;
  const canvasWidth = outer * 2 + (project.grid.columns * cellWidth) + ((project.grid.columns - 1) * gutter);
  const canvasHeight =
    outer * 2 +
    metadataHeight +
    (project.grid.rows * cellHeight) +
    ((project.grid.rows - 1) * gutter);

  return {
    canvasWidth,
    canvasHeight,
    cellWidth,
    cellHeight,
    gutter,
    outer,
    metadataHeight,
  };
};

/** Resolves the effective tile time after fine-tune offsets for playhead sync. */
export const effectiveTileTimeMs = (project: ProjectFile, tile: ProjectTile): number => {
  const durationMs = Math.max((project.video.durationMs ?? 60_000) - 1, 0);
  const baseTimeMs =
    tile.selection.kind === "manual"
      ? seekTimeForFrameIndex(
          tile.selection.frameIndex,
          project.video.fps,
          project.video.durationMs ?? 60_000,
        )
      : 0;

  return Math.min(Math.max(baseTimeMs + tile.fineTuneOffsetMs, 0), durationMs);
};

/** Builds percentage-based hitboxes so the overlay scales with the preview image. */
export const deriveSheetTileHitboxes = (project: ProjectFile): SheetTileHitbox[] => {
  const metrics = deriveSheetMetrics(project);

  return project.tiles.map((tile) => {
    const x = metrics.outer + (tile.span.column * (metrics.cellWidth + metrics.gutter));
    const y = metrics.outer + metrics.metadataHeight + (tile.span.row * (metrics.cellHeight + metrics.gutter));
    const width = (tile.span.columnSpan * metrics.cellWidth) + ((tile.span.columnSpan - 1) * metrics.gutter);
    const height = (tile.span.rowSpan * metrics.cellHeight) + ((tile.span.rowSpan - 1) * metrics.gutter);

    return {
      tileId: tile.id,
      xPct: (x / metrics.canvasWidth) * 100,
      yPct: (y / metrics.canvasHeight) * 100,
      widthPct: (width / metrics.canvasWidth) * 100,
      heightPct: (height / metrics.canvasHeight) * 100,
      label: inferTileLabel(tile),
      timeMs: effectiveTileTimeMs(project, tile),
      pinned: tile.pinned,
    };
  });
};
