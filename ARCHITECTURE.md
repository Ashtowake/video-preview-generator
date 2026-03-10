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
- command-facing service interfaces for future FFmpeg-backed decode and export

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

## Near-term Gaps

- FFmpeg sidecar execution is still behind a stub service boundary.
- Final sheet rendering is not wired to a real raster pipeline yet.
- Updater and diagnostics are scaffolded as UI and command placeholders pending platform-specific packaging.
