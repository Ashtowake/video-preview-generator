import { estimateFullFidelity, formatTimeMs } from "@video-preview/domain";
import { useEffect, useRef } from "react";

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
    loadedVideoSizeBytes,
    previewFrame,
    previewBusy,
    previewError,
    alert,
    busy,
    setLoadedVideo,
    loadVideoFromPath,
    refreshPreviewFrame,
    setDurationMs,
    setPlayheadMs,
    setRangeStart,
    setRangeEnd,
    setCustomSkip,
    setFrameStep,
    stepCustom,
    stepSeconds,
    stepFrames,
    setAnalysisMode,
    autoFill,
  } = useEditorStore();
  const { copy } = useI18n();

  const durationMs = project.video.durationMs ?? 60_000;
  const decodeEstimate = estimateFullFidelity(project, loadedVideoSizeBytes);

  useEffect(() => {
    if (desktopRuntime || !videoRef.current) {
      return;
    }

    videoRef.current.currentTime = project.playback.playheadMs / 1000;
  }, [desktopRuntime, project.playback.playheadMs]);

  useEffect(() => {
    if (!desktopRuntime || !project.video.path || project.video.path === "unloaded-video.mp4") {
      return;
    }

    const timeoutId = window.setTimeout(() => {
      void refreshPreviewFrame();
    }, 120);

    return () => window.clearTimeout(timeoutId);
  }, [desktopRuntime, project.playback.playheadMs, project.video.path, refreshPreviewFrame]);

  return (
    <section className="panel">
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
        {desktopRuntime ? (
          previewFrame ? (
            <img alt={copy.media.previewAlt} src={previewFrame.dataUrl} />
          ) : (
            <div className="video-frame__empty">
              <p>{previewBusy ? copy.media.loadingPreview : copy.media.emptyState}</p>
              <p className="muted">{previewError ?? copy.media.backendPreviewHint}</p>
            </div>
          )
        ) : loadedVideoUrl ? (
          <video
            controls={false}
            onLoadedMetadata={(event) => {
              const element = event.currentTarget;
              setDurationMs(Math.round(element.duration * 1000));
            }}
            onTimeUpdate={(event) => setPlayheadMs(Math.round(event.currentTarget.currentTime * 1000))}
            ref={videoRef}
            src={loadedVideoUrl}
          />
        ) : (
          <div className="video-frame__empty">
            <p>{copy.media.emptyState}</p>
            <p className="muted">{copy.media.emptyHint}</p>
          </div>
        )}
      </div>

      <div className="timeline">
        <label>
          <span>{copy.media.playhead}</span>
          <input
            disabled={busy || !loadedVideoUrl}
            max={durationMs}
            min={0}
            onChange={(event) => setPlayheadMs(Number(event.currentTarget.value))}
            type="range"
            value={project.playback.playheadMs}
          />
        </label>
        <div className="timeline__range">
          <label>
            <span>{copy.media.rangeStart}</span>
            <input
              disabled={busy || !loadedVideoUrl}
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
              disabled={busy || !loadedVideoUrl}
              max={durationMs}
              min={project.range.startMs + 250}
              onChange={(event) => setRangeEnd(Number(event.currentTarget.value))}
              type="range"
              value={project.range.endMs}
            />
          </label>
        </div>
        <div className="stats-row">
          <span>{copy.media.playhead} {formatTimeMs(project.playback.playheadMs)}</span>
          <span>
            {copy.media.rangeLabel} {formatTimeMs(project.range.startMs)} - {formatTimeMs(project.range.endMs)}
          </span>
          <span>{copy.media.frameLabel} {project.playback.activeFrameIndex}</span>
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
        <button disabled={busy || !loadedVideoUrl} onClick={() => stepCustom(-1)} type="button">{copy.media.stepCustomBack}</button>
        <button disabled={busy || !loadedVideoUrl} onClick={() => stepCustom(1)} type="button">{copy.media.stepCustomForward}</button>
        <button disabled={busy || !loadedVideoUrl} onClick={() => stepSeconds(5, -1)} type="button">{copy.media.stepFiveBack}</button>
        <button disabled={busy || !loadedVideoUrl} onClick={() => stepSeconds(5, 1)} type="button">{copy.media.stepFiveForward}</button>
        <button disabled={busy || !loadedVideoUrl} onClick={() => stepSeconds(1, -1)} type="button">{copy.media.stepOneBack}</button>
        <button disabled={busy || !loadedVideoUrl} onClick={() => stepSeconds(1, 1)} type="button">{copy.media.stepOneForward}</button>
        <button disabled={busy || !loadedVideoUrl} onClick={() => stepFrames(-1)} type="button">{copy.media.stepFrameBack}</button>
        <button disabled={busy || !loadedVideoUrl} onClick={() => stepFrames(1)} type="button">{copy.media.stepFrameForward}</button>
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
          <button disabled={busy || !loadedVideoUrl} onClick={autoFill} type="button">{copy.media.autoFill}</button>
        </div>
      </div>

      {desktopRuntime && previewBusy ? <p className="muted">{copy.media.loadingPreview}</p> : null}
      {alert ? <p className={`notice notice--${alert.tone}`}>{alert.message}</p> : null}
    </section>
  );
};
