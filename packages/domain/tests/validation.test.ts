import { describe, expect, it } from "vitest";

import { createStarterProject, estimateFullFidelity, validateProject } from "../src/index";

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
});
