# Project Format

Projects are stored as versioned `.vpg.json` files.

## Top-Level Fields

- `version`: schema version
- `analysisMode`: `quick_preview` or `full_fidelity`
- `video`: source path and known media metadata
- `playback`: playhead state, custom jump, and frame-step configuration
- `range`: selected in/out range in milliseconds
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

## Stability Notes

- The project file is language-neutral.
- New schema versions should be additive when possible.
- Validation should reject overlapping spans and invalid time ranges before export.
