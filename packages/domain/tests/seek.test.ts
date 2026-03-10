import { describe, expect, it } from "vitest";

import { centerOfBinSamples, parseTimeDelta, stepByFrames } from "../src/index";

describe("seek helpers", () => {
  it("distributes samples using center-of-bin spacing", () => {
    expect(centerOfBinSamples(0, 1000, 4)).toEqual([125, 375, 625, 875]);
  });

  it("parses mixed time formats", () => {
    expect(parseTimeDelta("5")).toBe(5000);
    expect(parseTimeDelta("1:05")).toBe(65_000);
    expect(parseTimeDelta("1:02:03.5")).toBe(3_723_500);
  });

  it("steps by frames using fps", () => {
    expect(stepByFrames(1000, 24, 24, 5000)).toBe(2000);
  });
});
