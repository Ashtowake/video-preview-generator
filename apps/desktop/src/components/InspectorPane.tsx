import { formatTimeMs } from "@video-preview/domain";

import { useI18n } from "../i18n/provider";
import { useEditorStore } from "../store/editorStore";

export const InspectorPane = () => {
  const {
    project,
    selectedTileId,
    busy,
    selectTile,
    toggleTilePin,
    fineTuneTile,
    setTileManualFrame,
    resizeTile,
    runSharpestNeighbour,
  } = useEditorStore();
  const { copy } = useI18n();

  const selectedTile = project.tiles.find((tile) => tile.id === selectedTileId) ?? project.tiles[0];

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
        <select disabled={busy} onChange={(event) => selectTile(event.currentTarget.value)} value={selectedTile?.id}>
          {project.tiles.map((tile) => (
            <option key={tile.id} value={tile.id}>
              {tile.id}
            </option>
          ))}
        </select>
        {selectedTile ? (
          <>
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
                min={0}
                onChange={(event) => setTileManualFrame(selectedTile.id, Number(event.currentTarget.value))}
                type="number"
                value={selectedTile.selection.kind === "manual" ? selectedTile.selection.frameIndex : 0}
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
        <div className="stats-grid">
          <span>{copy.inspector.rows} {project.grid.rows}</span>
          <span>{copy.inspector.columns} {project.grid.columns}</span>
          <span>{copy.inspector.roundedCorners} {project.style.frameRoundingPx}px</span>
          <span>{copy.inspector.shadow} {project.style.frameShadowPx}px</span>
          <span>{copy.inspector.gutter} {project.grid.gutterPx}px</span>
          <span>{copy.inspector.margin} {project.grid.outerMarginPx}px</span>
          <span>{copy.inspector.format} {project.export.format.toUpperCase()}</span>
          <span>{copy.inspector.scale} {project.export.scale.toFixed(1)}x</span>
        </div>
      </div>
    </section>
  );
};
