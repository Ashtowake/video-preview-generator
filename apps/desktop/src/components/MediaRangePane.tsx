import { estimateFullFidelity, formatTimeMs } from "@video-preview/domain";
import { useEffect, useRef } from "react";

import { useI18n } from "../i18n/provider";
import { useEditorStore } from "../store/editorStore";

export const MediaRangePane = () => {
  const videoRef = useRef<HTMLVideoElement>(null);
  const {
    project,
    loadedVideoUrl,
    loadedVideoSizeBytes,
    alert,
    setLoadedVideo,
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
    if (!videoRef.current) {
      return;
    }

    videoRef.current.currentTime = project.playback.playheadMs / 1000;
  }, [project.playback.playheadMs]);

  return (
    <section className="panel">
      <div className="panel__header">
        <div>
          <p className="eyebrow">{copy.media.eyebrow}</p>
          <h2>{copy.media.title}</h2>
        </div>
        <label className="file-input">
          <input
            accept="video/*"
            onChange={(event) => {
              const file = event.currentTarget.files?.[0];
              if (file) {
                setLoadedVideo(file);
              }
            }}
            type="file"
          />
          {copy.media.importVideo}
        </label>
      </div>

      <div className="video-frame">
        {loadedVideoUrl ? (
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
            onBlur={(event) => setCustomSkip(event.currentTarget.value)}
            type="text"
          />
        </label>
        <label>
          <span>{copy.media.frameStep}</span>
          <input
            min={1}
            onChange={(event) => setFrameStep(Number(event.currentTarget.value))}
            type="number"
            value={project.playback.frameStep}
          />
        </label>
      </div>

      <div className="button-row">
        <button onClick={() => stepCustom(-1)} type="button">{copy.media.stepCustomBack}</button>
        <button onClick={() => stepCustom(1)} type="button">{copy.media.stepCustomForward}</button>
        <button onClick={() => stepSeconds(5, -1)} type="button">{copy.media.stepFiveBack}</button>
        <button onClick={() => stepSeconds(5, 1)} type="button">{copy.media.stepFiveForward}</button>
        <button onClick={() => stepSeconds(1, -1)} type="button">{copy.media.stepOneBack}</button>
        <button onClick={() => stepSeconds(1, 1)} type="button">{copy.media.stepOneForward}</button>
        <button onClick={() => stepFrames(-1)} type="button">{copy.media.stepFrameBack}</button>
        <button onClick={() => stepFrames(1)} type="button">{copy.media.stepFrameForward}</button>
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
            onClick={() => setAnalysisMode("quick_preview")}
            type="button"
          >
            {copy.editor.quickPreview}
          </button>
          <button
            className={project.analysisMode === "full_fidelity" ? "button--active" : ""}
            onClick={() => setAnalysisMode("full_fidelity")}
            type="button"
          >
            {copy.editor.fullFidelity}
          </button>
          <button onClick={autoFill} type="button">{copy.media.autoFill}</button>
        </div>
      </div>

      {alert ? <p className={`notice notice--${alert.tone}`}>{alert.message}</p> : null}
    </section>
  );
};
