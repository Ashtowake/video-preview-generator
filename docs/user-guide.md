# User Guide

## Importing a Video

1. Open the desktop app.
2. Use the media pane to import a local video file.
3. Choose `Quick Preview` for fast setup or `Full Fidelity` for exact frame work when safety limits allow it.

## Building a Sheet

1. Scrub the timeline and set the in/out range.
2. Use the transport buttons for custom jumps, `5s`, `1s`, and frame stepping.
3. Run `Auto-fill grid` to distribute frames evenly inside the selected range.
4. Pin or fine-tune individual tiles from the inspector.
5. Resize tiles by changing their row or column span.

## Editing Tiles

- Set a manual frame index to override automatic selection.
- Use fine-tune controls for local adjustment.
- Run `Find sharpest neighbour` to use the current placeholder analysis flow until the FFmpeg-backed analyzer lands.

## Project and Export

- Save project files as `.vpg.json`.
- Reopen projects to restore range, layout, and styling choices.
- Export and batch processing are scaffolded through the shared Rust CLI and desktop command surface.
