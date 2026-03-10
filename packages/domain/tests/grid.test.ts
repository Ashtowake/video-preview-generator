import { describe, expect, it } from "vitest";

import { reshapeTilesForGrid } from "../src/grid";
import { createStarterProject } from "../src/project";

describe("grid helpers", () => {
  it("reshapes tiles for a new grid while preserving manual state", () => {
    const project = createStarterProject();
    project.tiles[0] = {
      ...project.tiles[0],
      pinned: true,
      fineTuneOffsetMs: 250,
      selection: { kind: "manual", frameIndex: 42, timeMs: 1_750 },
      span: { row: 0, column: 0, rowSpan: 2, columnSpan: 2 },
    };

    const tiles = reshapeTilesForGrid(project.tiles, 2, 2);

    expect(tiles).toHaveLength(4);
    expect(tiles[0]?.selection).toEqual({ kind: "manual", frameIndex: 42, timeMs: 1_750 });
    expect(tiles[0]?.pinned).toBe(true);
    expect(tiles[0]?.fineTuneOffsetMs).toBe(250);
    expect(tiles[0]?.span).toEqual({ row: 0, column: 0, rowSpan: 1, columnSpan: 1 });
    expect(tiles[3]?.span).toEqual({ row: 1, column: 1, rowSpan: 1, columnSpan: 1 });
  });
});
