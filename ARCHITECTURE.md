# Architecture

## Goals

- Keep decode, validation, and export logic in Rust.
- Keep React components presentational and move editor logic into small state modules and pure helpers.
- Share a single project model vocabulary across the CLI, desktop app, and documentation.

## Layers

### `crates/vpg-core`

Owns:

- project schema
- transport and seek math
- grid and tile layout rules
- validation
- diagnostics manifest generation
- FFmpeg/ffprobe-backed probing, frame extraction, sharpness search, and contact-sheet export
- command-facing service interfaces shared by the CLI and Tauri desktop shell

### `crates/vpg-cli`

Provides:

- `inspect`
- `export`
- `batch`

All commands delegate into `vpg-core`.

### `packages/domain`

Contains pure TypeScript equivalents for:

- project and editor types used by the frontend
- range and transport math
- grid helpers
- lightweight validation for optimistic UI flows

### `apps/desktop`

Contains:

- Tauri shell
- React app shell and routing
- Zustand editor store
- visual editor panes and diagnostics/help surfaces
- bounded preview-frame caching in the Tauri layer so repeated scrubs can reuse decoded frames

## Near-term Gaps

- Desktop playback is now backed by Rust-decoded still previews, but continuous playback is still not a Rust-managed stream surface.
- Caching, chunked analysis jobs, and packaged FFmpeg sidecars still need production hardening.
- Updater, diagnostics export polish, and release packaging remain pending platform-specific work.
