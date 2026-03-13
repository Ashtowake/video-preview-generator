import {
  clampPlayheadMs,
  displayFrameNumber,
  estimateFullFidelity,
  frameIndexAtTimeMs,
  formatTimeMs,
  stepByFrames,
  stepByTime,
} from "@video-preview/domain";
import { useEffect, useRef, useState } from "react";

import { useI18n } from "../i18n/provider";
import { isDesktopRuntime, openVideoDialog } from "../lib/backend";
import { useEditorStore } from "../store/editorStore";

export const MediaRangePane = () => {
  const videoRef = useRef<HTMLVideoElement>(null);
  const fileInputRef = useRef<HTMLInputElement>(null);
  const desktopRuntime = isDesktopRuntime();
  const {
    project,
    loadedVideoUrl,
    playbackPreparing,
    loadedVideoSizeBytes,
    previewFrame,
    previewBusy,
    previewError,
    alert,
    busy,
    setLoadedVideo,
    loadVideoFromPath,
    ensurePlaybackReady,
    refreshPreviewFrame,
    setDurationMs,
    setPlayheadMs,
    setRangeStart,
    setRangeEnd,
    setSampleStartOffset,
    setCustomSkip,
    setFrameStep,
    setAnalysisMode,
    autoFill,
  } = useEditorStore();
  const { copy } = useI18n();

  const durationMs = project.video.durationMs ?? 60_000;
  const fps = project.video.fps ?? 24;
  const hasVideo = Boolean(project.video.path) && project.video.path !== "unloaded-video.mp4";
  const rangeSpanMs = Math.max(250, project.range.endMs - project.range.startMs);
  const sampleOffsetMs = Math.max(0, project.range.sampleStartMs - project.range.startMs);
  const decodeEstimate = estimateFullFidelity(project, loadedVideoSizeBytes);
  const [playerPlayheadMs, setPlayerPlayheadMs] = useState(project.playback.playheadMs);
  const [isPlaying, setIsPlaying] = useState(false);
  const [playerError, setPlayerError] = useState<string | null>(null);
  const isScrubbingRef = useRef(false);
  const scrubbedPlayheadMsRef = useRef(project.playback.playheadMs);
  const pendingPauseMsRef = useRef<number | null>(null);
  const pendingPlaybackStartRef = useRef(false);
  const frameToleranceMs = Math.max(10, Math.round(500 / fps));
  const previewFrameFresh =
    previewFrame && Math.abs(previewFrame.timeMs - playerPlayheadMs) <= frameToleranceMs
      ? previewFrame
      : null;
  const showExactPreview = !isPlaying && Boolean(previewFrameFresh);
  const showPreviewSyncState = !isPlaying && !previewFrameFresh;
  const previewPrimaryMessage = playbackPreparing
    ? copy.media.preparingPlayback
    : previewBusy
      ? copy.media.loadingPreview
      : copy.media.previewAlt;
  const previewSecondaryMessage = playbackPreparing
    ? copy.media.preparingPlaybackHint
    : previewBusy
      ? copy.media.backendPreviewHint
      : copy.media.emptyHint;

  const applyVideoTime = (nextMs: number) => {
    if (!videoRef.current) {
      return;
    }

    const currentMs = Math.round(videoRef.current.currentTime * 1000);
    if (Math.abs(currentMs - nextMs) > 2) {
      videoRef.current.currentTime = nextMs / 1000;
    }
  };

  const commitPlayhead = (nextMs: number) => {
    const clampedMs = clampPlayheadMs(nextMs, durationMs);
    scrubbedPlayheadMsRef.current = clampedMs;
    setPlayerPlayheadMs(clampedMs);
    applyVideoTime(clampedMs);
    setPlayheadMs(clampedMs);
  };

  const effectivePlayheadMs = !isPlaying && previewFrameFresh ? previewFrameFresh.timeMs : playerPlayheadMs;
  const previewFrameIndex = !isPlaying && previewFrameFresh
    ? previewFrameFresh.frameIndex
    : frameIndexAtTimeMs(playerPlayheadMs, fps);

  useEffect(() => {
    setIsPlaying(false);
    setPlayerError(null);
    pendingPauseMsRef.current = null;
    if (!loadedVideoUrl) {
      pendingPlaybackStartRef.current = false;
    }
  }, [loadedVideoUrl]);

  useEffect(() => {
    if (!loadedVideoUrl || !pendingPlaybackStartRef.current || !videoRef.current) {
      return;
    }

    pendingPlaybackStartRef.current = false;
    void videoRef.current.play().catch((error) => {
      setIsPlaying(false);
      const reason = error instanceof Error ? ` ${error.message}` : "";
      setPlayerError(`${copy.media.playerLoadError}${reason}`);
    });
  }, [copy.media.playerLoadError, loadedVideoUrl]);

  useEffect(() => {
    if (isPlaying || isScrubbingRef.current) {
      return;
    }

    if (pendingPauseMsRef.current !== null) {
      if (Math.abs(project.playback.playheadMs - pendingPauseMsRef.current) > 2) {
        return;
      }

      pendingPauseMsRef.current = null;
    }

    scrubbedPlayheadMsRef.current = project.playback.playheadMs;
    setPlayerPlayheadMs(project.playback.playheadMs);
    applyVideoTime(project.playback.playheadMs);
  }, [isPlaying, project.playback.playheadMs, loadedVideoUrl]);

  useEffect(() => {
    if (!desktopRuntime || isPlaying || !project.video.path || project.video.path === "unloaded-video.mp4") {
      return;
    }

    const timeoutId = window.setTimeout(() => {
      void refreshPreviewFrame(480);
    }, 120);

    return () => window.clearTimeout(timeoutId);
  }, [
    desktopRuntime,
    isPlaying,
    project.playback.playheadMs,
    project.video.path,
    refreshPreviewFrame,
  ]);

  const loaded = hasVideo;
  const canPlayVideo = hasVideo;

  return (
    <section className="panel panel--media">
      <div className="panel__header">
        <div>
          <p className="eyebrow">{copy.media.eyebrow}</p>
          <h2>{copy.media.title}</h2>
        </div>
        <input
          accept="video/*"
          hidden
          onChange={(event) => {
            const file = event.currentTarget.files?.[0];
            if (file) {
              setLoadedVideo(file);
            }
            event.currentTarget.value = "";
          }}
          ref={fileInputRef}
          type="file"
        />
        <button
          onClick={() => {
            if (desktopRuntime) {
              void (async () => {
                const path = await openVideoDialog();
                if (path) {
                  await loadVideoFromPath(path);
                }
              })();
              return;
            }

            fileInputRef.current?.click();
          }}
          type="button"
          disabled={busy}
        >
          {desktopRuntime ? copy.media.browseVideo : copy.media.importVideo}
        </button>
      </div>

      <div className="video-frame">
        {hasVideo ? (
          <div className="video-frame__media">
            {loadedVideoUrl ? (
              <video
                className={isPlaying ? "video-frame__player" : "video-frame__player video-frame__player--hidden"}
                controls={false}
                key={loadedVideoUrl}
                onEnded={() => {
                  setIsPlaying(false);
                  commitPlayhead(durationMs);
                }}
                onError={(event) => {
                  setIsPlaying(false);
                  const mediaError = event.currentTarget.error;
                  const details = mediaError ? ` (HTMLMediaError ${mediaError.code})` : "";
                  setPlayerError(`${copy.media.playerLoadError}${details}`);
                }}
                onLoadedMetadata={(event) => {
                  const element = event.currentTarget;
                  setPlayerError(null);
                  setDurationMs(Math.round(element.duration * 1000));
                  applyVideoTime(project.playback.playheadMs);
                }}
                onPause={(event) => {
                  if (!isScrubbingRef.current) {
                    const pausedMs = Math.round(event.currentTarget.currentTime * 1000);
                    pendingPauseMsRef.current = pausedMs;
                    scrubbedPlayheadMsRef.current = pausedMs;
                    setPlayerPlayheadMs(pausedMs);
                    setPlayheadMs(pausedMs);
                  }
                  setIsPlaying(false);
                }}
                onPlay={() => setIsPlaying(true)}
                onTimeUpdate={(event) => {
                  if (isScrubbingRef.current) {
                    return;
                  }

                  const nextMs = Math.round(event.currentTarget.currentTime * 1000);
                  scrubbedPlayheadMsRef.current = nextMs;
                  setPlayerPlayheadMs(nextMs);
                  setPlayheadMs(nextMs);
                }}
                poster={previewFrameFresh?.dataUrl}
                preload="metadata"
                ref={videoRef}
                src={loadedVideoUrl}
              />
            ) : null}
            {previewFrameFresh ? (
              <img
                alt={copy.media.previewAlt}
                className="video-frame__exact-preview"
                src={previewFrameFresh.dataUrl}
              />
            ) : null}
            {showPreviewSyncState ? (
              <div className="video-frame__syncing">
                <p>{previewPrimaryMessage}</p>
                <p className="muted">{previewSecondaryMessage}</p>
              </div>
            ) : null}
          </div>
        ) : (
          <div className="video-frame__empty">
            <p>{copy.media.emptyState}</p>
            <p className="muted">{copy.media.emptyHint}</p>
          </div>
        )}
      </div>

      <div className="player-toolbar">
        <div className="player-toolbar__cluster">
          <button
            disabled={busy || !canPlayVideo}
            onClick={() => {
              void (async () => {
                if (!loadedVideoUrl) {
                  pendingPlaybackStartRef.current = true;
                  if (playbackPreparing) {
                    return;
                  }
                  const ready = await ensurePlaybackReady();
                  if (!ready && !playbackPreparing) {
                    pendingPlaybackStartRef.current = false;
                  }
                  return;
                }

                if (!videoRef.current) {
                  return;
                }

                if (videoRef.current.paused) {
                  void videoRef.current.play().catch((error) => {
                    setIsPlaying(false);
                    const reason = error instanceof Error ? ` ${error.message}` : "";
                    setPlayerError(`${copy.media.playerLoadError}${reason}`);
                  });
                  return;
                }

                videoRef.current.pause();
              })();
            }}
            type="button"
          >
            {isPlaying ? copy.media.pause : copy.media.play}
          </button>
          <button
            disabled={busy || !canPlayVideo}
            onClick={() => {
              if (videoRef.current && !videoRef.current.paused) {
                videoRef.current.pause();
              }
              setIsPlaying(false);
              commitPlayhead(project.range.startMs);
            }}
            type="button"
          >
            {copy.media.stop}
          </button>
        </div>
        <div className="player-toolbar__cluster player-toolbar__cluster--meta">
          <span>{formatTimeMs(effectivePlayheadMs)} / {formatTimeMs(durationMs)}</span>
          <span>{copy.media.frameLabel} {displayFrameNumber(previewFrameIndex, project.video.frameCount)}</span>
        </div>
      </div>

      <div className="timeline">
        <label>
          <span>{copy.media.playhead}</span>
          <input
            disabled={busy || !loaded}
            max={durationMs}
            min={0}
            onBlur={() => {
              if (isScrubbingRef.current) {
                isScrubbingRef.current = false;
                commitPlayhead(scrubbedPlayheadMsRef.current);
              }
            }}
            onChange={(event) => {
              const nextMs = Number(event.currentTarget.value);
              isScrubbingRef.current = true;
              scrubbedPlayheadMsRef.current = nextMs;
              setPlayerPlayheadMs(nextMs);
              applyVideoTime(nextMs);
              setPlayheadMs(nextMs);
            }}
            onKeyUp={() => {
              if (isScrubbingRef.current) {
                isScrubbingRef.current = false;
                commitPlayhead(scrubbedPlayheadMsRef.current);
              }
            }}
            onMouseUp={() => {
              if (isScrubbingRef.current) {
                isScrubbingRef.current = false;
                commitPlayhead(scrubbedPlayheadMsRef.current);
              }
            }}
            onTouchEnd={() => {
              if (isScrubbingRef.current) {
                isScrubbingRef.current = false;
                commitPlayhead(scrubbedPlayheadMsRef.current);
              }
            }}
            type="range"
            value={playerPlayheadMs}
          />
        </label>
        <div className="timeline__range">
          <label>
            <span>{copy.media.rangeStart}</span>
            <input
              disabled={busy || !loaded}
              max={Math.max(project.range.endMs - 250, 0)}
              min={0}
              onChange={(event) => setRangeStart(Number(event.currentTarget.value))}
              type="range"
              value={project.range.startMs}
            />
          </label>
          <label>
            <span>{copy.media.rangeEnd}</span>
            <input
              disabled={busy || !loaded}
              max={durationMs}
              min={project.range.startMs + 250}
              onChange={(event) => setRangeEnd(Number(event.currentTarget.value))}
              type="range"
              value={project.range.endMs}
            />
          </label>
        </div>
        <label>
          <span>{copy.media.startPosition}</span>
          <input
            disabled={busy || !loaded}
            max={rangeSpanMs}
            min={0}
            onChange={(event) => setSampleStartOffset(Number(event.currentTarget.value))}
            type="range"
            value={sampleOffsetMs}
          />
          <span className="muted timeline__hint">{copy.media.startPositionHint}</span>
        </label>
        <div className="stats-row">
          <span>{copy.media.playhead} {formatTimeMs(effectivePlayheadMs)}</span>
          <span>
            {copy.media.rangeLabel} {formatTimeMs(project.range.startMs)} - {formatTimeMs(project.range.endMs)}
          </span>
          <span>{copy.media.startPosition} +{formatTimeMs(sampleOffsetMs)}</span>
          <span>{copy.media.frameLabel} {displayFrameNumber(previewFrameIndex, project.video.frameCount)}</span>
        </div>
      </div>

      <div className="transport-grid">
        <label>
          <span>{copy.media.customJump}</span>
          <input
            defaultValue={formatTimeMs(project.playback.customSkipMs)}
            disabled={busy}
            onBlur={(event) => setCustomSkip(event.currentTarget.value)}
            type="text"
          />
        </label>
        <label>
          <span>{copy.media.frameStep}</span>
          <input
            disabled={busy}
            min={1}
            onChange={(event) => setFrameStep(Number(event.currentTarget.value))}
            type="number"
            value={project.playback.frameStep}
          />
        </label>
      </div>

      <div className="button-row">
        <button
          disabled={busy || !loaded}
          onClick={() => commitPlayhead(stepByTime(playerPlayheadMs, -project.playback.customSkipMs, durationMs))}
          type="button"
        >
          {copy.media.stepCustomBack}
        </button>
        <button
          disabled={busy || !loaded}
          onClick={() => commitPlayhead(stepByTime(playerPlayheadMs, project.playback.customSkipMs, durationMs))}
          type="button"
        >
          {copy.media.stepCustomForward}
        </button>
        <button
          disabled={busy || !loaded}
          onClick={() => commitPlayhead(stepByTime(playerPlayheadMs, -5_000, durationMs))}
          type="button"
        >
          {copy.media.stepFiveBack}
        </button>
        <button
          disabled={busy || !loaded}
          onClick={() => commitPlayhead(stepByTime(playerPlayheadMs, 5_000, durationMs))}
          type="button"
        >
          {copy.media.stepFiveForward}
        </button>
        <button
          disabled={busy || !loaded}
          onClick={() => commitPlayhead(stepByTime(playerPlayheadMs, -1_000, durationMs))}
          type="button"
        >
          {copy.media.stepOneBack}
        </button>
        <button
          disabled={busy || !loaded}
          onClick={() => commitPlayhead(stepByTime(playerPlayheadMs, 1_000, durationMs))}
          type="button"
        >
          {copy.media.stepOneForward}
        </button>
        <button
          disabled={busy || !loaded}
          onClick={() =>
            commitPlayhead(
              stepByFrames(
                playerPlayheadMs,
                -project.playback.frameStep,
                project.video.fps,
                durationMs,
              ),
            )
          }
          type="button"
        >
          {copy.media.stepFrameBack}
        </button>
        <button
          disabled={busy || !loaded}
          onClick={() =>
            commitPlayhead(
              stepByFrames(
                playerPlayheadMs,
                project.playback.frameStep,
                project.video.fps,
                durationMs,
              ),
            )
          }
          type="button"
        >
          {copy.media.stepFrameForward}
        </button>
      </div>

      <div className="mode-card">
        <div>
          <p className="eyebrow">{copy.media.importMode}</p>
          <h3>{project.analysisMode === "quick_preview" ? copy.editor.quickPreview : copy.editor.fullFidelity}</h3>
        </div>
        <p className="muted">
          {copy.media.estimatedMemory} {decodeEstimate.projectedMemoryMb} MB, cache {decodeEstimate.projectedCacheMb} MB
        </p>
        <div className="button-row">
          <button
            className={project.analysisMode === "quick_preview" ? "button--active" : ""}
            disabled={busy}
            onClick={() => setAnalysisMode("quick_preview")}
            type="button"
          >
            {copy.editor.quickPreview}
          </button>
          <button
            className={project.analysisMode === "full_fidelity" ? "button--active" : ""}
            disabled={busy}
            onClick={() => setAnalysisMode("full_fidelity")}
            type="button"
          >
            {copy.editor.fullFidelity}
          </button>
          <button disabled={busy || !loaded} onClick={autoFill} type="button">{copy.media.autoFill}</button>
        </div>
      </div>

      {desktopRuntime ? <p className="muted">{copy.media.backendPreviewHint}</p> : null}
      {playerError ? <p className="notice notice--warning">{playerError}</p> : null}
      {previewError ? <p className="notice notice--warning">{previewError}</p> : null}
      {alert ? <p className={`notice notice--${alert.tone}`}>{alert.message}</p> : null}
    </section>
  );
};
