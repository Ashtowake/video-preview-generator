# User Guide

## Importing a Video

1. Open the desktop app.
2. Use the media pane to import a local video file.
3. Choose `Quick Preview` for fast setup or `Full Fidelity` for exact frame work when safety limits allow it.
4. In the desktop build, the visible preview frame is decoded by the Rust backend so transport jumps match exported frames more closely.

## Building a Sheet

1. Scrub the timeline and set the in/out range.
2. Use the transport buttons for custom jumps, `5s`, `1s`, and frame stepping.
3. Adjust `Start position` if the first extracted frame should begin later inside the selected range.
4. Run `Auto-fill grid` to distribute frames evenly from that start position through the end of the range.
5. Pin or fine-tune individual tiles from the inspector.
6. Resize tiles by changing their row or column span.
7. Use the inspector to change grid size, gutter, margins, rounded corners, shadows, export format, scale, and watermark text.

## Editing Tiles

- Set a manual frame index to override automatic selection.
- Use fine-tune controls for local adjustment.
- Run `Find sharpest neighbour` to scan the selected tile across the configured frame window and replace it with the sharpest nearby frame.

## Project and Export

- Save project files as `.vpg.json`.
- Reopen projects to restore range, layout, and styling choices.
- Export PNG or JPEG sheets from the desktop app or the shared Rust CLI.
- Batch processing remains a planned follow-up; the `batch` command and project fields are present but still placeholder-level.
