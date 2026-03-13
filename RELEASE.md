# Release Process

## Goals

- Produce native installers for Windows, macOS, and Linux.
- Bundle legal notices for third-party code and shipped runtime libraries.
- Publish release notes and update metadata together.

## Licensing Baseline

- Repository license: `GPL-3.0-or-later`
- Runtime stack in the native shell: Qt 6 + `libmpv`
- Media pipeline: FFmpeg/ffprobe

The repository no longer presents itself as MIT-only. Any shipped bundle must include the relevant third-party notices and follow the terms of the actual `libmpv`, Qt, and FFmpeg binaries included in that build.

## Current State

- The desktop app, CLI, and Rust core can now probe videos and render contact sheets locally.
- Release hardening is still pending for signing, notarization, bundled sidecars, updater publishing, and platform packaging.
- Windows now has a defined developer bring-up path: `MSVC + vcpkg` for Qt, plus a prepared `libmpv` bundle for the native shell runtime.

## Windows Developer Runtime Bring-Up

The first Windows milestone is not installer/signing work. It is:

- build the native shell with `MSVC + vcpkg`
- prepare `libmpv` for the native shell runtime
- deploy Qt runtime files beside the executable
- run a real Windows smoke test for playback, scrubbing, crop, sheet preview, and export

This is the baseline before later packaging work.

## Later Windows Packaging Work

Still pending after developer bring-up:

- installer generation
- signing and notarization equivalents where applicable
- fully bundled release artifact verification
- updater publishing and manifest generation

## Planned Steps

1. Run workspace tests and type checks.
2. Build the desktop app for each target platform using the intended open-source distribution configuration.
3. Generate third-party notice artifacts.
4. Verify the bundled `libmpv`, Qt, and FFmpeg builds match the documented license assumptions for that release.
5. Attach release notes and diagnostics metadata.
6. Publish installers and updater manifests.
