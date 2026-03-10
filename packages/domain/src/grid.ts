import type { GridSettings, ProjectTile, TileSelection, ValidationIssue } from "./types";
import { centerOfBinSamples } from "./seek";

export const autoFillTiles = (
  tiles: ProjectTile[],
  rangeStartMs: number,
  rangeEndMs: number,
  fps: number | undefined,
): ProjectTile[] => {
  const nextTiles = tiles.map((tile) => ({ ...tile }));
  const autoTileIndices = nextTiles.flatMap((tile, index) => (tile.pinned ? [] : [index]));
  const samples = centerOfBinSamples(rangeStartMs, rangeEndMs, autoTileIndices.length);

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
