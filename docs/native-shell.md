# Native Shell

## Why the shell changed

The Tauri/webview shell was workable for menus, forms, and diagnostics, but it was the wrong place to solve frame-accurate playback. The new native shell uses Qt 6 plus `libmpv` so:

- playback, scrubbing, and frame stepping all use one decoder state
- transport responsiveness is no longer limited by HTML media behavior
- the editor can grow toward a more conventional post-production layout without webview constraints

## Current shape

The native shell is in `apps/native-shell`.

It currently provides:

- two top-level workspaces: `Review` and `Layouts`
- a `libmpv`-backed preview surface
- a source bin for multi-file review and quick source switching
- playback, stop, seek, custom jump, frame-step, mute, and volume controls
- a native transport timeline with filmstrip, hover preview, range handles, and sampling start
- an interactive sheet preview for per-video tile selection and frame assignment
- a batch queue panel inside the review workspace
- a reusable layout preset workspace with `.vpg-layout.json` save/load/apply flow
- a Rust CLI bridge for video metadata probing
- Rust-backed sheet preview/export from full project JSON

## Build

From the repository root:

```bash
cmake -S apps/native-shell -B build/native-shell -DCMAKE_BUILD_TYPE=Debug
cmake --build build/native-shell --parallel 4
./build/native-shell/video-preview-native
```

Or:

```bash
pnpm dev:native
```

The CMake target builds `vpg-cli` automatically because the current native bridge shells out to the Rust CLI for `inspect`.

### Windows developer bring-up

Use `MSVC + vcpkg` for Windows. Open an `x64 Native Tools Command Prompt for VS 2022` or `Developer PowerShell for VS 2022`, then:

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

What the Windows setup does:

- `vcpkg.json` installs Qt 6 for the native shell.
- `scripts/windows/prepare-libmpv.ps1` downloads the current `mpv-dev` bundle, extracts headers and `libmpv-2.dll`, and generates an MSVC import library.
- `deploy-video-preview-native` runs `windeployqt` and copies `libmpv-2.dll` next to the executable.

### Windows smoke checklist

Run this on a real Windows machine after the first successful build:

- launch the app
- open one landscape and one portrait H.264 MP4
- verify play, pause, and stop
- verify frame-step and scrubbing
- verify timeline thumbnails and hover preview
- verify crop selection and apply/clear
- verify source switching
- verify sheet preview rendering
- verify export

### Windows troubleshooting

- `Failed to create libmpv instance.` or a missing `libmpv-2.dll` error:
  make sure `scripts/windows/prepare-libmpv.ps1` succeeded and `deploy-video-preview-native` was run for the same build directory.
- CMake cannot find `LIBMPV_DIR`:
  pass `-DLIBMPV_DIR=...` explicitly to CMake and point it at the prepared directory containing `include`, `lib`, and `bin`.
- OpenGL or render-context startup failures:
  use a local desktop session, not Remote Desktop if possible, and verify current GPU drivers are installed.
- The app cannot find the Rust CLI bridge:
  build `vpg-cli` in the repo first or set `VPG_CLI_PATH` to the full path to `vpg-cli.exe`.

## Near-term migration work

1. Replace the temporary CLI bridge with a direct Rust/native interface.
2. Deepen the interactive sheet editor beyond the current per-tile review controls.
3. Strengthen batch export and project save/load flows on top of the native source/layout state.
4. Remove the Tauri shell once the native editor reaches feature parity for the core workflow.
