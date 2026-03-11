# Native Shell

## Why the shell changed

The Tauri/webview shell was workable for menus, forms, and diagnostics, but it was the wrong place to solve frame-accurate playback. The new native shell uses Qt 6 plus `libmpv` so:

- playback, scrubbing, and frame stepping all use one decoder state
- transport responsiveness is no longer limited by HTML media behavior
- the editor can grow toward a more conventional post-production layout without webview constraints

## Current shape

The native shell is in `apps/native-shell`.

It currently provides:

- a native main window
- a `libmpv`-backed preview surface
- playback, stop, seek, custom jump, and frame-step controls
- a Rust CLI bridge for video metadata probing
- right-hand workspace placeholders for the upcoming sheet canvas and inspector migration

## Build

From the repository root:

```bash
cmake -S apps/native-shell -B build/native-shell -DCMAKE_BUILD_TYPE=Debug
cmake --build build/native-shell -j4
./build/native-shell/video-preview-native
```

Or:

```bash
pnpm dev:native
```

The CMake target builds `vpg-cli` automatically because the current native bridge shells out to the Rust CLI for `inspect`.

## Near-term migration work

1. Replace the temporary CLI bridge with a direct Rust/native interface.
2. Move sheet preview and export controls into the native shell.
3. Reattach project editing surfaces to the same Rust project model.
4. Remove the Tauri shell once the native editor reaches feature parity for the core workflow.
