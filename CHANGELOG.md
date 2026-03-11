# Changelog

## Unreleased

- Reorganized the repository into a Rust/Tauri/React workspace.
- Preserved the Python prototype under `legacy/python-prototype`.
- Added a typed project model, editor shell, CLI scaffold, documentation, and CI foundation.
- Added FFmpeg/ffprobe-backed video probing and frame extraction in the Rust core.
- Added contact-sheet export rendering for PNG and JPEG output through the shared Rust service.
- Wired desktop save/load/export actions to Tauri commands and native dialogs.
- Switched the desktop media preview to Rust-decoded still frames instead of relying only on the webview video element.
- Added a bounded LRU preview cache in the Tauri shell to avoid redundant decode work while scrubbing.
- Added real inspector controls for grid sizing, spacing, frame styling, export format/scale, and watermark text.
- Added a separate start-position control for auto-filled frame sampling and persisted it in the shared project format.
- Fixed the Rust/TypeScript project schema contract for per-tile manual frame selections.
- Added a native Qt 6 + `libmpv` desktop shell scaffold under `apps/native-shell`.
- Switched the migration strategy away from Tauri for playback-critical workflows.
- Added helper scripts and docs for configuring and running the native shell.
