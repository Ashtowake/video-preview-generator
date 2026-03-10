# Release Process

## Goals

- Produce native installers for Windows, macOS, and Linux.
- Bundle legal notices for third-party code and FFmpeg.
- Publish release notes and update metadata together.

## Current State

The release pipeline is scaffolded but not yet fully wired for signing, notarization, or updater publishing.

## Planned Steps

1. Run workspace tests and type checks.
2. Build the desktop app for each target platform.
3. Generate third-party notice artifacts.
4. Attach release notes and diagnostics metadata.
5. Publish installers and updater manifests.
