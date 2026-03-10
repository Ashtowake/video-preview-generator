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
- A Rust CLI scaffold with `inspect`, `export`, and `batch` commands
- Documentation and CI scaffolding for the new workspace

The FFmpeg-backed decode and render pipeline is scaffolded behind stable interfaces but is not fully implemented yet.

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
```

### Desktop app

The frontend shell can run in a browser during early development:

```bash
pnpm --filter @video-preview/desktop dev
```

Once the Rust toolchain and Tauri prerequisites are installed, the Tauri app can be launched from the desktop workspace.

## Documentation

- [`ARCHITECTURE.md`](./ARCHITECTURE.md)
- [`CONTRIBUTING.md`](./CONTRIBUTING.md)
- [`RELEASE.md`](./RELEASE.md)
- [`docs/user-guide.md`](./docs/user-guide.md)
- [`docs/localization.md`](./docs/localization.md)
- [`docs/project-format.md`](./docs/project-format.md)
- [`CHANGELOG.md`](./CHANGELOG.md)
