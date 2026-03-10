# Release Process

## Goals

- Produce native installers for Windows, macOS, and Linux.
- Bundle legal notices for third-party code and FFmpeg.
- Publish release notes and update metadata together.

## Current State

- The desktop app, CLI, and Rust core can now probe videos and render contact sheets locally.
- Release hardening is still pending for signing, notarization, bundled sidecars, updater publishing, and platform packaging.

## Planned Steps

1. Run workspace tests and type checks.
2. Build the desktop app for each target platform.
3. Generate third-party notice artifacts.
4. Attach release notes and diagnostics metadata.
5. Publish installers and updater manifests.
