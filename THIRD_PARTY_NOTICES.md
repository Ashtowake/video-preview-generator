# Third-Party Notices

`video-preview-generator` is licensed under `GPL-3.0-or-later`.

The shipped application is expected to include or link against third-party components with their own license terms. The exact obligations for a given release depend on the binaries actually distributed with that release.

## Core Runtime Components

- `libmpv / mpv`
  - Upstream: https://github.com/mpv-player/mpv
  - License: `GPL-2.0-or-later` by default, or `LGPL-2.1-or-later` when built without GPL features
  - This project currently assumes GPL-compatible native-shell distribution builds unless documented otherwise for a specific release.

- `Qt 6`
  - Upstream: https://doc.qt.io/qt-6/licensing.html
  - License: available under `LGPL-3.0-only`, `GPL-2.0-only`, `GPL-3.0-only`, or commercial terms depending on module and distribution choice
  - The native shell uses Qt 6 libraries and must ship with the appropriate notice/compliance material for the actual packaged modules.

- `FFmpeg / ffprobe`
  - Upstream: https://ffmpeg.org/legal.html
  - License: `LGPL-2.1-or-later` by default, or GPL when built with GPL components enabled
  - Releases must document which FFmpeg configuration was used.

## Repository Components Still Present During Migration

- `Tauri`
  - Upstream: https://v2.tauri.app
  - License: `MIT OR Apache-2.0`
  - The legacy Tauri shell remains in the repository as a migration reference and is not the intended long-term shipped desktop shell.

## Distribution Notes

- Releases must include third-party notices for the actual runtime artifacts they bundle.
- If a release ships GPL-configured `libmpv` or FFmpeg builds, the release notes and bundled notices must say so explicitly.
- If a future release targets a different `libmpv` or FFmpeg licensing configuration, this file and [`RELEASE.md`](./RELEASE.md) must be updated accordingly.
