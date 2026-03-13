# Video Preview Generator

`video-preview-generator` is being rebuilt as a cross-platform desktop editor for high-quality video contact sheets. The codebase now centers on a Rust core plus CLI, with the desktop shell migrating to a native Qt 6 + `libmpv` stack so transport fidelity and responsiveness are owned by one decoder path.

The repository is licensed under `GPL-3.0-or-later`. The shipped desktop application also carries third-party obligations from `libmpv`, Qt 6, and FFmpeg; see [`THIRD_PARTY_NOTICES.md`](./THIRD_PARTY_NOTICES.md) and [`RELEASE.md`](./RELEASE.md).

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
- A native `Review` workspace with source bin, transport, interactive sheet preview, and batch queue
- A native `Layouts` workspace for reusable sheet presets stored as `.vpg-layout.json`
- Ship-ready application shell sections such as About, Credits, Help, Diagnostics, and Third-Party Notices
- A Rust CLI with working `inspect` and `export` commands backed by the Rust core
- FFmpeg/ffprobe-backed video probing, frame extraction, and contact-sheet rendering in `vpg-core`
- A legacy Tauri shell that remains in the repo only as a transition reference
- Sharpness-neighbour analysis for selected manual tiles in the desktop editor
- Documentation and CI scaffolding for the new workspace

Still pending for later milestones: deeper native sheet-canvas editing, a direct Rust/native bridge instead of the current CLI bridge in the Qt shell, disk-backed decode caches, packaged FFmpeg sidecars for releases, richer batch execution, and updater/signing release work.

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

Linux build:

```bash
cmake -S apps/native-shell -B build/native-shell -DCMAKE_BUILD_TYPE=Debug
cmake --build build/native-shell --parallel 4
```

Then launch it with:

```bash
./build/native-shell/video-preview-native
```

Windows build:

Open an `x64 Native Tools Command Prompt for VS 2022` or `Developer PowerShell for VS 2022`, then:

```powershell
git clone https://github.com/microsoft/vcpkg $env:VCPKG_ROOT
& "$env:VCPKG_ROOT\bootstrap-vcpkg.bat" -disableMetrics
& "$env:VCPKG_ROOT\vcpkg.exe" install --triplet x64-windows
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\windows\prepare-libmpv.ps1 -Destination "$PWD\build\windows-libmpv" -ReleaseTag 20260307
cmake -S apps/native-shell -B build/native-shell-win -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows -DLIBMPV_DIR="$PWD\build\windows-libmpv"
cmake --build build/native-shell-win --config Debug --parallel 4
cmake --build build/native-shell-win --target deploy-video-preview-native --config Debug
.\build\native-shell-win\Debug\video-preview-native.exe
```

Or use the helper script on Linux/macOS:

```bash
pnpm dev:native
```

The native shell currently migrates transport first: `libmpv` owns playback, scrubbing, and frame-step responsiveness, while the Rust CLI/core still provide metadata probing and export logic.

For Windows-specific setup notes and troubleshooting, see [`docs/native-shell.md`](./docs/native-shell.md).

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

## License

- Project source: `GPL-3.0-or-later`
- Third-party runtime notices: [`THIRD_PARTY_NOTICES.md`](./THIRD_PARTY_NOTICES.md)
- Release/distribution compliance notes: [`RELEASE.md`](./RELEASE.md)
