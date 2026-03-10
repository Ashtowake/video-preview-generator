import { inferTileLabel } from "@video-preview/domain";
import { Group, Layer, Rect, Stage, Text } from "react-konva";

import { useI18n } from "../i18n/provider";
import { useEditorStore } from "../store/editorStore";

const CELL_SIZE = 96;

export const GridCanvas = () => {
  const { project, selectedTileId, selectTile } = useEditorStore();
  const { copy } = useI18n();
  const width =
    project.grid.columns * CELL_SIZE + (project.grid.columns - 1) * project.grid.gutterPx;
  const height =
    project.grid.rows * CELL_SIZE + (project.grid.rows - 1) * project.grid.gutterPx;

  return (
    <section className="panel panel--canvas">
      <div className="panel__header">
        <div>
          <p className="eyebrow">{copy.canvas.eyebrow}</p>
          <h2>{copy.canvas.title}</h2>
        </div>
        <p className="muted">{copy.canvas.description}</p>
      </div>
      <div className="canvas-wrap">
        <Stage height={height + 32} width={width + 32}>
          <Layer>
            {project.tiles.map((tile) => {
              const x = 16 + tile.span.column * (CELL_SIZE + project.grid.gutterPx);
              const y = 16 + tile.span.row * (CELL_SIZE + project.grid.gutterPx);
              const tileWidth =
                tile.span.columnSpan * CELL_SIZE + (tile.span.columnSpan - 1) * project.grid.gutterPx;
              const tileHeight =
                tile.span.rowSpan * CELL_SIZE + (tile.span.rowSpan - 1) * project.grid.gutterPx;
              const selected = selectedTileId === tile.id;

              return (
                <Group key={tile.id}>
                  <Rect
                    cornerRadius={project.style.frameRoundingPx / 2}
                    fill={tile.pinned ? "#3e6073" : "#1d3040"}
                    height={tileHeight}
                    onClick={() => selectTile(tile.id)}
                    shadowBlur={project.style.frameShadowPx}
                    shadowColor="rgba(13, 16, 23, 0.55)"
                    stroke={selected ? "#f7d35c" : "#9ec7dd"}
                    strokeWidth={selected ? 3 : project.style.frameBorderPx}
                    width={tileWidth}
                    x={x}
                    y={y}
                  />
                  <Text
                    fill="#f8fafc"
                    fontSize={15}
                    text={`${tile.id}\n${inferTileLabel(tile).replace("Auto", copy.canvas.auto).replace("Frame", copy.canvas.frame)}${tile.pinned ? `\n${copy.canvas.pinned}` : ""}`}
                    width={tileWidth - 16}
                    x={x + 8}
                    y={y + 12}
                  />
                </Group>
              );
            })}
          </Layer>
        </Stage>
      </div>
    </section>
  );
};
