import { spawn } from "node:child_process";
import { existsSync } from "node:fs";
import { join } from "node:path";

const executableName = process.platform === "win32"
  ? "video-preview-native.exe"
  : "video-preview-native";
const executablePath = join(process.cwd(), "build", "native-shell", executableName);

if (!existsSync(executablePath)) {
  console.error(`Native shell executable not found at ${executablePath}`);
  process.exit(1);
}

const child = spawn(executablePath, {
  stdio: "inherit",
});

child.on("exit", (code, signal) => {
  if (signal) {
    process.kill(process.pid, signal);
    return;
  }

  process.exit(code ?? 0);
});
