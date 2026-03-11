import { displayFrameNumber, formatTimeMs } from "@video-preview/domain";

import { useI18n } from "../i18n/provider";
import { useEditorStore } from "../store/editorStore";

export const InspectorPane = () => {
  const {
    project,
    selectedTileId,
    busy,
    setGridDimensions,
    setGridSpacing,
    updateStyle,
    setExportFormat,
    setExportScale,
    setWatermarkText,
    focusTile,
    toggleTilePin,
    fineTuneTile,
    setTileManualFrame,
    resizeTile,
    runSharpestNeighbour,
  } = useEditorStore();
  const { copy } = useI18n();

  const selectedTile = project.tiles.find((tile) => tile.id === selectedTileId) ?? project.tiles[0];
  const selectedTileIndex = selectedTile
    ? project.tiles.findIndex((tile) => tile.id === selectedTile.id)
    : -1;
  const previousTile = selectedTileIndex > 0 ? project.tiles[selectedTileIndex - 1] : null;
  const nextTile =
    selectedTileIndex >= 0 && selectedTileIndex < project.tiles.length - 1
      ? project.tiles[selectedTileIndex + 1]
      : null;

  return (
    <section className="panel">
      <div className="panel__header">
        <div>
          <p className="eyebrow">{copy.inspector.eyebrow}</p>
          <h2>{copy.inspector.title}</h2>
        </div>
      </div>

      <div className="inspector-section">
        <h3>{copy.inspector.selectedTile}</h3>
        {selectedTile ? (
          <>
            <div className="selected-tile-card">
              <strong>{selectedTile.id}</strong>
              <span>
                {selectedTile.selection.kind === "manual"
                  ? `${copy.canvas.frame} ${displayFrameNumber(selectedTile.selection.frameIndex, project.video.frameCount)}`
                  : copy.canvas.auto}
              </span>
            </div>
            <div className="button-row">
              <button
                disabled={busy || !previousTile}
                onClick={() => previousTile && focusTile(previousTile.id)}
                type="button"
              >
                {copy.inspector.previousTile}
              </button>
              <button
                disabled={busy || !nextTile}
                onClick={() => nextTile && focusTile(nextTile.id)}
                type="button"
              >
                {copy.inspector.nextTile}
              </button>
            </div>
            <div className="stats-row">
              <span>{copy.inspector.span} {selectedTile.span.columnSpan}x{selectedTile.span.rowSpan}</span>
              <span>{copy.inspector.fineTune} {formatTimeMs(Math.abs(selectedTile.fineTuneOffsetMs))}</span>
            </div>
            <div className="button-row">
              <button disabled={busy} onClick={() => toggleTilePin(selectedTile.id)} type="button">
                {selectedTile.pinned ? copy.inspector.unpin : copy.inspector.pin}
              </button>
              <button disabled={busy} onClick={() => fineTuneTile(selectedTile.id, -500)} type="button">
                -500 ms
              </button>
              <button disabled={busy} onClick={() => fineTuneTile(selectedTile.id, 500)} type="button">
                +500 ms
              </button>
            </div>
            <label>
              <span>{copy.inspector.manualFrame}</span>
              <input
                disabled={busy}
                max={project.video.frameCount}
                min={1}
                onChange={(event) =>
                  setTileManualFrame(selectedTile.id, Math.max(0, Number(event.currentTarget.value) - 1))
                }
                type="number"
                value={
                  selectedTile.selection.kind === "manual"
                    ? displayFrameNumber(selectedTile.selection.frameIndex, project.video.frameCount)
                    : 1
                }
              />
            </label>
            <div className="button-row">
              <button
                disabled={busy}
                onClick={() => resizeTile(selectedTile.id, selectedTile.span.rowSpan + 1, selectedTile.span.columnSpan)}
                type="button"
              >
                {copy.inspector.taller}
              </button>
              <button
                disabled={busy}
                onClick={() => resizeTile(selectedTile.id, selectedTile.span.rowSpan, selectedTile.span.columnSpan + 1)}
                type="button"
              >
                {copy.inspector.wider}
              </button>
              <button disabled={busy} onClick={() => void runSharpestNeighbour()} type="button">{copy.inspector.sharpestNeighbour}</button>
            </div>
          </>
        ) : null}
      </div>

      <div className="inspector-section">
        <h3>{copy.inspector.globalStyle}</h3>
        <div className="transport-grid">
          <label>
            <span>{copy.inspector.rows}</span>
            <input
              disabled={busy}
              max={12}
              min={1}
              onChange={(event) => setGridDimensions(Number(event.currentTarget.value), project.grid.columns)}
              type="number"
              value={project.grid.rows}
            />
          </label>
          <label>
            <span>{copy.inspector.columns}</span>
            <input
              disabled={busy}
              max={12}
              min={1}
              onChange={(event) => setGridDimensions(project.grid.rows, Number(event.currentTarget.value))}
              type="number"
              value={project.grid.columns}
            />
          </label>
          <label>
            <span>{copy.inspector.gutter}</span>
            <input
              disabled={busy}
              min={0}
              onChange={(event) => setGridSpacing(Number(event.currentTarget.value), project.grid.outerMarginPx)}
              type="number"
              value={project.grid.gutterPx}
            />
          </label>
          <label>
            <span>{copy.inspector.margin}</span>
            <input
              disabled={busy}
              min={0}
              onChange={(event) => setGridSpacing(project.grid.gutterPx, Number(event.currentTarget.value))}
              type="number"
              value={project.grid.outerMarginPx}
            />
          </label>
          <label>
            <span>{copy.inspector.roundedCorners}</span>
            <input
              disabled={busy}
              min={0}
              onChange={(event) => updateStyle({ frameRoundingPx: Number(event.currentTarget.value) })}
              type="number"
              value={project.style.frameRoundingPx}
            />
          </label>
          <label>
            <span>{copy.inspector.shadow}</span>
            <input
              disabled={busy}
              min={0}
              onChange={(event) => updateStyle({ frameShadowPx: Number(event.currentTarget.value) })}
              type="number"
              value={project.style.frameShadowPx}
            />
          </label>
          <label>
            <span>{copy.inspector.border}</span>
            <input
              disabled={busy}
              min={0}
              onChange={(event) => updateStyle({ frameBorderPx: Number(event.currentTarget.value) })}
              type="number"
              value={project.style.frameBorderPx}
            />
          </label>
        </div>
        <div className="toggle-grid">
          <label className="toggle-row">
            <input
              checked={project.style.showMetadataBar}
              disabled={busy}
              onChange={(event) => updateStyle({ showMetadataBar: event.currentTarget.checked })}
              type="checkbox"
            />
            <span>{copy.inspector.metadataBar}</span>
          </label>
          <label className="toggle-row">
            <input
              checked={project.style.showTimestamps}
              disabled={busy}
              onChange={(event) => updateStyle({ showTimestamps: event.currentTarget.checked })}
              type="checkbox"
            />
            <span>{copy.inspector.timestamps}</span>
          </label>
          <label className="toggle-row">
            <input
              checked={project.style.darkMode}
              disabled={busy}
              onChange={(event) => updateStyle({ darkMode: event.currentTarget.checked })}
              type="checkbox"
            />
            <span>{copy.inspector.darkMode}</span>
          </label>
        </div>
      </div>

      <div className="inspector-section">
        <h3>{copy.inspector.exportSettings}</h3>
        <div className="transport-grid">
          <label>
            <span>{copy.inspector.format}</span>
            <select
              disabled={busy}
              onChange={(event) => setExportFormat(event.currentTarget.value as "png" | "jpeg")}
              value={project.export.format}
            >
              <option value="png">PNG</option>
              <option value="jpeg">JPEG</option>
            </select>
          </label>
          <label>
            <span>{copy.inspector.scale}</span>
            <input
              disabled={busy}
              max={4}
              min={0.25}
              onChange={(event) => setExportScale(Number(event.currentTarget.value))}
              step={0.1}
              type="number"
              value={project.export.scale}
            />
          </label>
        </div>
        <label>
          <span>{copy.inspector.watermarkText}</span>
          <input
            disabled={busy}
            onChange={(event) => setWatermarkText(event.currentTarget.value)}
            type="text"
            value={project.watermark.text?.value ?? ""}
          />
        </label>
      </div>
    </section>
  );
};
