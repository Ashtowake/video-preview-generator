/** Parses `h:m:s.ms`, `m:s`, or `s` into milliseconds. */
export const parseTimeDelta = (input: string): number => {
  const trimmed = input.trim();
  if (!trimmed) {
    throw new Error("Time delta cannot be empty.");
  }

  const parts = trimmed.split(":");
  const [hours, minutes, secondsText] =
    parts.length === 1
      ? [0, 0, parts[0]]
      : parts.length === 2
        ? [0, Number(parts[0]), parts[1]]
        : parts.length === 3
          ? [Number(parts[0]), Number(parts[1]), parts[2]]
          : [NaN, NaN, ""];

  if (!Number.isFinite(hours) || !Number.isFinite(minutes) || !secondsText) {
    throw new Error("Unsupported time delta format.");
  }

  const seconds = Number(secondsText);
  if (!Number.isFinite(seconds)) {
    throw new Error("Time delta must contain numeric values.");
  }

  return Math.round(((hours * 3600) + (minutes * 60) + seconds) * 1000);
};

/** Formats a time delta into `h:mm:ss.mmm` or `m:ss`. */
export const formatTimeMs = (timeMs: number): string => {
  const clamped = Math.max(0, Math.round(timeMs));
  const totalSeconds = Math.floor(clamped / 1000);
  const hours = Math.floor(totalSeconds / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  const seconds = totalSeconds % 60;
  const milliseconds = clamped % 1000;

  if (hours > 0) {
    return `${hours}:${String(minutes).padStart(2, "0")}:${String(seconds).padStart(2, "0")}.${String(milliseconds).padStart(3, "0")}`;
  }

  return `${minutes}:${String(seconds).padStart(2, "0")}.${String(milliseconds).padStart(3, "0")}`;
};

export const clampPlayheadMs = (playheadMs: number, durationMs: number): number =>
  Math.min(Math.max(playheadMs, 0), durationMs);

/** Clamps a computed frame index to the actual frame range when frame count is known. */
export const clampFrameIndex = (
  frameIndex: number,
  frameCount: number | undefined,
): number => {
  const safeIndex = Math.max(0, Math.round(frameIndex));
  if (!frameCount || frameCount <= 0) {
    return safeIndex;
  }

  return Math.min(safeIndex, Math.max(0, Math.round(frameCount) - 1));
};

/** Formats a zero-based frame index for user-facing labels. */
export const displayFrameNumber = (
  frameIndex: number,
  frameCount: number | undefined,
): number => clampFrameIndex(frameIndex, frameCount) + 1;

/** Maps a timestamp to the nearest frame position used by the exact-preview path. */
export const frameIndexAtTimeMs = (timeMs: number, fps: number | undefined): number => {
  if (!fps || fps <= 0) {
    return 0;
  }

  return Math.max(0, Math.round((Math.max(timeMs, 0) / 1000) * fps));
};

/** Returns a seek timestamp at the start of the requested frame. */
export const seekTimeForFrameIndex = (
  frameIndex: number,
  fps: number | undefined,
  durationMs: number,
): number => {
  if (!fps || fps <= 0) {
    return 0;
  }

  const safeFrameIndex = Math.max(0, Math.floor(frameIndex));
  if (safeFrameIndex === 0) {
    return 0;
  }

  const frameStartMs = Math.round(((safeFrameIndex / fps) * 1000));
  const latestSeekMs = Math.max(0, durationMs - 1);
  return clampPlayheadMs(frameStartMs, latestSeekMs);
};

export const stepByTime = (playheadMs: number, deltaMs: number, durationMs: number): number =>
  clampPlayheadMs(playheadMs + deltaMs, durationMs);

export const stepByFrames = (
  playheadMs: number,
  frames: number,
  fps: number | undefined,
  durationMs: number,
): number => {
  if (!fps || fps <= 0) {
    return playheadMs;
  }

  const currentFrameIndex = frameIndexAtTimeMs(playheadMs, fps);
  return seekTimeForFrameIndex(currentFrameIndex + frames, fps, durationMs);
};

/** Evenly spreads sample times inside the selected range using center-of-bin spacing. */
export const centerOfBinSamples = (
  rangeStartMs: number,
  rangeEndMs: number,
  count: number,
): number[] => {
  if (count <= 0 || rangeEndMs <= rangeStartMs) {
    return [];
  }

  const span = rangeEndMs - rangeStartMs;
  return Array.from({ length: count }, (_, index) =>
    Math.round(rangeStartMs + (((index + 0.5) * span) / count)),
  );
};

/** Evenly spreads sample times from an explicit start position to the end of the range. */
export const evenlySpacedSamplesFromStart = (
  rangeStartMs: number,
  rangeEndMs: number,
  sampleStartMs: number,
  count: number,
): number[] => {
  if (count <= 0 || rangeEndMs <= rangeStartMs) {
    return [];
  }

  const safeStart =
    count === 1
      ? Math.min(Math.max(sampleStartMs, rangeStartMs), rangeEndMs)
      : Math.min(Math.max(sampleStartMs, rangeStartMs), Math.max(rangeStartMs, rangeEndMs - 1));

  if (count === 1) {
    return [safeStart];
  }

  const span = rangeEndMs - safeStart;
  return Array.from({ length: count }, (_, index) =>
    Math.round(safeStart + ((index * span) / (count - 1))),
  );
};
