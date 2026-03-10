import { spawn } from "node:child_process";

const env = { ...process.env };

if (process.platform === "linux") {
  const sessionType = env.XDG_SESSION_TYPE ?? "";
  const hasWaylandDisplay = Boolean(env.WAYLAND_DISPLAY);

  if ((sessionType === "wayland" || hasWaylandDisplay) && !env.WEBKIT_DISABLE_DMABUF_RENDERER) {
    // WebKitGTK can crash on some Wayland stacks when DMA-BUF rendering is
    // enabled. Set the documented override for local development unless the
    // caller already chose a different value.
    env.WEBKIT_DISABLE_DMABUF_RENDERER = "1";
  }
}

const command = process.platform === "win32" ? "tauri.cmd" : "tauri";
const child = spawn(command, ["dev"], {
  env,
  stdio: "inherit",
});

child.on("error", (error) => {
  console.error(`Failed to start '${command} dev': ${error.message}`);
  process.exit(1);
});

child.on("exit", (code, signal) => {
  if (signal) {
    process.kill(process.pid, signal);
    return;
  }

  process.exit(code ?? 0);
});
