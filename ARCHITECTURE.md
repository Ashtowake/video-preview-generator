# Architecture

## Goals

- Keep decode, validation, and export logic in Rust.
- Use a native desktop shell for playback-critical workflows instead of a webview transport path.
- Share a single project model vocabulary across the CLI, native shell, and documentation.

## Layers

### `crates/vpg-core`

Owns:

- project schema
- transport and seek math
- grid and tile layout rules
- validation
- diagnostics manifest generation
- FFmpeg/ffprobe-backed probing, frame extraction, sharpness search, and contact-sheet export
- command-facing service interfaces shared by the CLI and native shell bridge

### `crates/vpg-cli`

Provides:

- `inspect`
- `export`
- `batch`

All commands delegate into `vpg-core`.

### `packages/domain`

Contains the TypeScript project model and editor math that still support the deprecated Tauri shell during migration. It remains useful as a reference while the native shell grows a direct Rust/native bridge.

### `apps/native-shell`

Contains:

- Qt 6 desktop shell
- `libmpv`-backed transport surface
- native split-pane editor frame
- a temporary Rust CLI bridge for probe metadata while the direct native bridge is built

### `apps/desktop`

Contains the deprecated Tauri shell. It remains in the repository only as a transition reference while the native shell takes over.

## Near-term Gaps

- The native shell currently migrates transport first. Sheet composition and inspector editing still need to move out of the legacy Tauri frontend.
- The Qt shell currently bridges into Rust through `vpg-cli`; a direct Rust/native bridge is the next structural cleanup.
- Caching, chunked analysis jobs, and packaged FFmpeg sidecars still need production hardening.
