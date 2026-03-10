# Changelog

## Unreleased

- Reorganized the repository into a Rust/Tauri/React workspace.
- Preserved the Python prototype under `legacy/python-prototype`.
- Added a typed project model, editor shell, CLI scaffold, documentation, and CI foundation.
- Added FFmpeg/ffprobe-backed video probing and frame extraction in the Rust core.
- Added contact-sheet export rendering for PNG and JPEG output through the shared Rust service.
- Wired desktop save/load/export actions to Tauri commands and native dialogs.
- Switched the desktop media preview to Rust-decoded still frames instead of relying only on the webview video element.
- Fixed the Rust/TypeScript project schema contract for per-tile manual frame selections.
