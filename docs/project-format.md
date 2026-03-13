# Project Format

Projects are stored as versioned `.vpg.json` files.

Reusable layouts are stored separately as `.vpg-layout.json` files.

## Top-Level Fields

- `version`: schema version
- `analysisMode`: `quick_preview` or `full_fidelity`
- `video`: source path, known media metadata, and optional normalized crop rectangle
- `playback`: playhead state, custom jump, and frame-step configuration
- `range`: selected in/out range plus `sampleStartMs`, which shifts the first auto-filled frame before the remaining samples are evenly distributed
- `grid`: row/column configuration and spacing
- `tiles`: per-tile selection, span, pin state, and fine-tune offset
- `style`: sheet-level visual settings
- `watermark`: text and image watermark configuration
- `export`: format, scale, and output path
- `batch`: shared export settings for multiple sources

## Tile Model

Each tile stores:

- an `id`
- an `order`
- a `span` with `row`, `column`, `rowSpan`, and `columnSpan`
- a `selection` value that is either automatic or a manual frame/time override
- a `pinned` flag
- a `fineTuneOffsetMs` value

Manual selections are serialized as:

```json
{
  "kind": "manual",
  "frameIndex": 42,
  "timeMs": 1750
}
```

Automatic selections are serialized as:

```json
{
  "kind": "auto"
}
```

## Crop Model

Projects can optionally store a normalized crop rectangle under `video.crop`:

```json
{
  "x": 0.125,
  "y": 0.10,
  "width": 0.75,
  "height": 0.8
}
```

Notes:

- `x` and `y` are normalized offsets from the top-left corner of the decoded source frame.
- `width` and `height` are normalized sizes relative to the full source frame.
- Validation rejects crop rectangles that extend outside `[0, 1]`.
- Preview rendering, sharpness analysis, and export should all respect the same stored crop.

## Stability Notes

- The project file is language-neutral.
- New schema versions should be additive when possible.
- Validation should reject overlapping spans, invalid time ranges, and invalid crop rectangles before export.

## Layout Presets

Layout presets intentionally exclude source-specific state. A `.vpg-layout.json` file contains:

- `version`
- `name`
- `grid`
- `tiles`
- `style`
- `watermark`
- `export`

It must not contain:

- `video`
- `playback`
- `range`
- source-specific crop or frame assignments
- batch input lists
