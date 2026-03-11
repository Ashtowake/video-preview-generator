# Video Preview Generator

`video-preview-generator` is being rebuilt as a cross-platform desktop editor for high-quality video contact sheets. The codebase now centers on a Rust core plus CLI, with the desktop shell migrating to a native Qt 6 + `libmpv` stack so transport fidelity and responsiveness are owned by one decoder path.

The original Python/Tkinter prototype is preserved in [`legacy/python-prototype`](./legacy/python-prototype) as a reference implementation.

## Workspace Layout

- `apps/native-shell`: native Qt 6 desktop shell with `libmpv` playback
- `apps/desktop`: legacy Tauri shell kept temporarily as a migration reference
- `crates/vpg-core`: Rust domain logic, project model, and command-facing services
- `crates/vpg-cli`: Headless CLI entrypoint backed by the same Rust core
- `packages/domain`: Shared TypeScript project model and editor math helpers
- `docs`: User, contributor, and release documentation
- `fixtures`: Test and example assets

## Current Status

This repository now contains:

- A typed project model in Rust and TypeScript
- A native Qt/libmpv transport shell under `apps/native-shell`
- Ship-ready application shell sections such as About, Credits, Help, Diagnostics, and Third-Party Notices
- A Rust CLI with working `inspect` and `export` commands backed by the Rust core
- FFmpeg/ffprobe-backed video probing, frame extraction, and contact-sheet rendering in `vpg-core`
- A legacy Tauri shell that remains in the repo only as a transition reference
- Sharpness-neighbour analysis for selected manual tiles in the desktop editor
- Documentation and CI scaffolding for the new workspace

Still pending for later milestones: native sheet-canvas editing on top of the Rust renderer, a direct Rust/native bridge instead of the current CLI bridge in the Qt shell, disk-backed decode caches, packaged FFmpeg sidecars for releases, richer batch execution, and updater/signing release work.

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

### Native desktop shell

Configure and build the native Qt shell:

```bash
cmake -S apps/native-shell -B build/native-shell -DCMAKE_BUILD_TYPE=Debug
cmake --build build/native-shell -j4
```

Then launch it with:

```bash
./build/native-shell/video-preview-native
```

Or use the helper script:

```bash
pnpm dev:native
```

The native shell currently migrates transport first: `libmpv` owns playback, scrubbing, and frame-step responsiveness, while the Rust CLI/core still provide metadata probing and export logic.

## Documentation

- [`ARCHITECTURE.md`](./ARCHITECTURE.md)
- [`CONTRIBUTING.md`](./CONTRIBUTING.md)
- [`RELEASE.md`](./RELEASE.md)
- [`docs/user-guide.md`](./docs/user-guide.md)
- [`docs/native-shell.md`](./docs/native-shell.md)
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
