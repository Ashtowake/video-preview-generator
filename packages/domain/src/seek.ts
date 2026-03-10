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

  return stepByTime(playheadMs, Math.round((frames / fps) * 1000), durationMs);
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
