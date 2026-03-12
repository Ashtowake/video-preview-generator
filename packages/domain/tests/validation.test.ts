import { describe, expect, it } from "vitest";

import {
  createStarterProject,
  estimateFullFidelity,
  validateProject,
} from "../src/index";

describe("project validation", () => {
  it("reports invalid ranges", () => {
    const project = createStarterProject();
    project.range.startMs = 1000;
    project.range.endMs = 500;

    expect(validateProject(project)[0]?.message).toContain("Range end");
  });

  it("estimates decode cost with safety checks", () => {
    const estimate = estimateFullFidelity(createStarterProject(), 10_000_000);
    expect(estimate.projectedMemoryMb).toBeGreaterThan(0);
    expect(estimate.projectedCacheMb).toBeGreaterThan(0);
  });

  it("accepts an in-bounds crop rectangle", () => {
    const project = createStarterProject("movie.mp4");
    project.video.crop = { x: 0.1, y: 0.1, width: 0.6, height: 0.7 };

    expect(validateProject(project)).toEqual([]);
  });

  it("rejects a crop rectangle that extends beyond the source frame", () => {
    const project = createStarterProject("movie.mp4");
    project.video.crop = { x: 0.8, y: 0.2, width: 0.3, height: 0.5 };

    expect(validateProject(project)).toEqual(
      expect.arrayContaining([
        expect.objectContaining({
          level: "error",
          path: "video.crop",
        }),
      ]),
    );
  });
});
