# Video Preview Generator

`video-preview-generator` is being rebuilt as a cross-platform desktop editor for high-quality video contact sheets. The new codebase is organized as a Rust core plus CLI, a Tauri desktop shell, and a React/TypeScript frontend.

The original Python/Tkinter prototype is preserved in [`legacy/python-prototype`](./legacy/python-prototype) as a reference implementation.

## Workspace Layout

- `apps/desktop`: Tauri desktop app and React frontend
- `crates/vpg-core`: Rust domain logic, project model, and command-facing services
- `crates/vpg-cli`: Headless CLI entrypoint backed by the same Rust core
- `packages/domain`: Shared TypeScript project model and editor math helpers
- `docs`: User, contributor, and release documentation
- `fixtures`: Test and example assets

## Current Status

This repository now contains the first implementation pass of the rewrite:

- A typed project model in Rust and TypeScript
- Editor state and transport controls in the new desktop frontend
- Ship-ready application shell sections such as About, Credits, Help, Diagnostics, and Third-Party Notices
- A Rust CLI with working `inspect` and `export` commands backed by the Rust core
- FFmpeg/ffprobe-backed video probing, frame extraction, and contact-sheet rendering in `vpg-core`
- Desktop save/load/export wiring through Tauri commands and native file dialogs
- Rust-decoded still-frame preview in the desktop media pane for seek and frame-step accuracy
- Bounded desktop preview-frame caching in the Tauri shell to reduce repeated decode work while scrubbing
- Sharpness-neighbour analysis for selected manual tiles in the desktop editor
- Documentation and CI scaffolding for the new workspace

Still pending for later milestones: disk-backed decode caches, packaged FFmpeg sidecars for releases, richer batch execution, and updater/signing release work.

## Quick Start

### JavaScript workspace

```bash
pnpm install
pnpm --filter @video-preview/domain test
pnpm --filter @video-preview/domain typecheck
```

### Rust workspace

Install Rust with `rustup`, then:

```bash
cargo test -p vpg-core
cargo run -p vpg-cli -- inspect ./example.mp4
cargo run -p vpg-cli -- export ./example.vpg.json --out ./example.png
```

### Desktop app

The frontend shell can run in a browser during early development:

```bash
pnpm --filter @video-preview/desktop dev
```

Once the Rust toolchain and Tauri prerequisites are installed, launch the desktop app with:

```bash
pnpm dev:desktop
```

On Linux Wayland sessions, the wrapper enables the WebKitGTK DMA-BUF workaround automatically for local development.

## Documentation

- [`ARCHITECTURE.md`](./ARCHITECTURE.md)
- [`CONTRIBUTING.md`](./CONTRIBUTING.md)
- [`RELEASE.md`](./RELEASE.md)
- [`docs/user-guide.md`](./docs/user-guide.md)
- [`docs/localization.md`](./docs/localization.md)
- [`docs/project-format.md`](./docs/project-format.md)
- [`CHANGELOG.md`](./CHANGELOG.md)

## API Docs

Rust API documentation is generated with `rustdoc`:

```bash
pnpm docs:api:rust
```

TypeScript API documentation is generated with `TypeDoc` from TSDoc comments:

```bash
pnpm docs:api:ts
```

Run `pnpm docs:api` to build both.
