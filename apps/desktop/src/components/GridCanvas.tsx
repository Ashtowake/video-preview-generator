import { formatTimeMs } from "@video-preview/domain";
import { useEffect } from "react";

import { useI18n } from "../i18n/provider";
import { deriveSheetTileHitboxes } from "../lib/sheetLayout";
import { useEditorStore } from "../store/editorStore";

const UNLOADED_VIDEO_PATH = "unloaded-video.mp4";

export const GridCanvas = () => {
  const {
    project,
    selectedTileId,
    sheetPreview,
    sheetPreviewBusy,
    sheetPreviewError,
    focusTile,
    refreshSheetPreview,
  } = useEditorStore();
  const { copy } = useI18n();
  const hasVideo = Boolean(project.video.path) && project.video.path !== UNLOADED_VIDEO_PATH;
  const tileHitboxes = deriveSheetTileHitboxes(project);

  useEffect(() => {
    if (!hasVideo) {
      return;
    }

    const timeoutId = window.setTimeout(() => {
      void refreshSheetPreview(1200);
    }, 180);

    return () => window.clearTimeout(timeoutId);
  }, [
    hasVideo,
    project.export,
    project.grid,
    project.range.endMs,
    project.range.sampleStartMs,
    project.range.startMs,
    project.style,
    project.tiles,
    project.video.height,
    project.video.path,
    project.video.width,
    project.watermark,
    refreshSheetPreview,
  ]);

  return (
    <section className="panel panel--canvas">
      <div className="panel__header">
        <div>
          <p className="eyebrow">{copy.canvas.eyebrow}</p>
          <h2>{copy.canvas.title}</h2>
        </div>
        <p className="muted">{copy.canvas.description}</p>
      </div>
      <div className="canvas-wrap canvas-wrap--sheet">
        {!hasVideo ? (
          <div className="canvas-empty-state">
            <p>{copy.canvas.emptyState}</p>
            <p className="muted">{copy.canvas.emptyHint}</p>
          </div>
        ) : sheetPreview ? (
          <div className="sheet-preview-frame">
            <img
              alt={copy.canvas.previewAlt}
              className="sheet-preview"
              height={sheetPreview.height}
              src={sheetPreview.dataUrl}
              width={sheetPreview.width}
            />
            <div className="sheet-preview-overlay" aria-label={copy.canvas.previewAlt}>
              {tileHitboxes.map((tile) => (
                <button
                  key={tile.tileId}
                  aria-label={`${tile.tileId} ${tile.label}`}
                  className={`sheet-tile-hitbox${selectedTileId === tile.tileId ? " sheet-tile-hitbox--active" : ""}`}
                  onClick={() => focusTile(tile.tileId)}
                  style={{
                    left: `${tile.xPct}%`,
                    top: `${tile.yPct}%`,
                    width: `${tile.widthPct}%`,
                    height: `${tile.heightPct}%`,
                  }}
                  title={`${tile.tileId} • ${tile.label} • ${formatTimeMs(tile.timeMs)}`}
                  type="button"
                >
                  <span className="sheet-tile-hitbox__label">{tile.tileId}</span>
                  <span className="sheet-tile-hitbox__meta">
                    {tile.label}
                    {tile.pinned ? ` • ${copy.canvas.pinned}` : ""}
                  </span>
                </button>
              ))}
            </div>
          </div>
        ) : (
          <div className="canvas-empty-state">
            <p>{sheetPreviewBusy ? copy.canvas.loadingPreview : copy.canvas.waitingPreview}</p>
            <p className="muted">{sheetPreviewError ?? copy.canvas.previewHint}</p>
          </div>
        )}
      </div>
    </section>
  );
};
