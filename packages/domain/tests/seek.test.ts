import { describe, expect, it } from "vitest";

import {
  centerOfBinSamples,
  evenlySpacedSamplesFromStart,
  frameIndexAtTimeMs,
  parseTimeDelta,
  seekTimeForFrameIndex,
  stepByFrames,
} from "../src/index";

describe("seek helpers", () => {
  it("distributes samples using center-of-bin spacing", () => {
    expect(centerOfBinSamples(0, 1000, 4)).toEqual([125, 375, 625, 875]);
  });

  it("parses mixed time formats", () => {
    expect(parseTimeDelta("5")).toBe(5000);
    expect(parseTimeDelta("1:05")).toBe(65_000);
    expect(parseTimeDelta("1:02:03.5")).toBe(3_723_500);
  });

  it("distributes samples from an explicit start position", () => {
    expect(evenlySpacedSamplesFromStart(0, 1000, 200, 4)).toEqual([200, 467, 733, 1000]);
  });

  it("steps by frames using fps", () => {
    expect(stepByFrames(1000, 24, 24, 5000)).toBe(2000);
  });

  it("maps times to containing frame indices", () => {
    expect(frameIndexAtTimeMs(0, 30)).toBe(0);
    expect(frameIndexAtTimeMs(33, 30)).toBe(1);
    expect(frameIndexAtTimeMs(50, 30)).toBe(1);
    expect(frameIndexAtTimeMs(67, 30)).toBe(2);
  });

  it("targets the start of a frame when seeking by index", () => {
    expect(seekTimeForFrameIndex(0, 30, 1000)).toBe(0);
    expect(seekTimeForFrameIndex(1, 30, 1000)).toBe(33);
  });
});
