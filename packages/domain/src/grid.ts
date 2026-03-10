import type { GridSettings, ProjectTile, TileSelection, ValidationIssue } from "./types";
import { evenlySpacedSamplesFromStart } from "./seek";

/**
 * Rebuilds the tile list for a new grid size while preserving per-tile selection state where
 * possible.
 *
 * Spans are reset to `1x1` because larger spans are often invalid after a row/column change.
 */
export const reshapeTilesForGrid = (
  tiles: ProjectTile[],
  rows: number,
  columns: number,
): ProjectTile[] => {
  const safeRows = Math.max(1, Math.round(rows));
  const safeColumns = Math.max(1, Math.round(columns));
  const totalTiles = safeRows * safeColumns;

  return Array.from({ length: totalTiles }, (_, index) => {
    const existing = tiles[index];

    return {
      id: existing?.id ?? `tile-${index}`,
      order: index,
      span: {
        row: Math.floor(index / safeColumns),
        column: index % safeColumns,
        rowSpan: 1,
        columnSpan: 1,
      },
      selection: existing?.selection ?? { kind: "auto" },
      pinned: existing?.pinned ?? false,
      fineTuneOffsetMs: existing?.fineTuneOffsetMs ?? 0,
    };
  });
};

export const autoFillTiles = (
  tiles: ProjectTile[],
  rangeStartMs: number,
  rangeEndMs: number,
  sampleStartMs: number,
  fps: number | undefined,
): ProjectTile[] => {
  const nextTiles = tiles.map((tile) => ({ ...tile }));
  const autoTileIndices = nextTiles.flatMap((tile, index) => (tile.pinned ? [] : [index]));
  const samples = evenlySpacedSamplesFromStart(
    rangeStartMs,
    rangeEndMs,
    sampleStartMs,
    autoTileIndices.length,
  );

  autoTileIndices.forEach((tileIndex, sampleIndex) => {
    const timeMs = samples[sampleIndex] ?? rangeStartMs;
    const frameIndex = fps ? Math.round((timeMs / 1000) * fps) : 0;

    nextTiles[tileIndex] = {
      ...nextTiles[tileIndex],
      selection: { kind: "manual", frameIndex, timeMs },
      fineTuneOffsetMs: 0,
    };
  });

  return nextTiles;
};

export const validateGridSpans = (
  tiles: ProjectTile[],
  grid: GridSettings,
): ValidationIssue[] => {
  const occupancy = Array.from({ length: grid.rows }, () =>
    Array.from({ length: grid.columns }, () => null as string | null),
  );
  const issues: ValidationIssue[] = [];

  tiles.forEach((tile) => {
    const { row, column, rowSpan, columnSpan } = tile.span;

    if (
      rowSpan <= 0 ||
      columnSpan <= 0 ||
      row + rowSpan > grid.rows ||
      column + columnSpan > grid.columns
    ) {
      issues.push({
        level: "error",
        path: `tiles.${tile.id}`,
        message: "Tile span falls outside the configured grid.",
      });
      return;
    }

    for (let y = row; y < row + rowSpan; y += 1) {
      for (let x = column; x < column + columnSpan; x += 1) {
        if (occupancy[y][x]) {
          issues.push({
            level: "error",
            path: `tiles.${tile.id}`,
            message: `Tile overlaps with ${occupancy[y][x]}.`,
          });
        } else {
          occupancy[y][x] = tile.id;
        }
      }
    }
  });

  return issues;
};

export const inferTileLabel = (tile: ProjectTile): string => {
  if (tile.selection.kind === "manual") {
    return `Frame ${tile.selection.frameIndex}`;
  }

  return "Auto";
};

export const isManualSelection = (selection: TileSelection): selection is Extract<TileSelection, { kind: "manual" }> =>
  selection.kind === "manual";
