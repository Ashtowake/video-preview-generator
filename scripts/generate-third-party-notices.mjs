import { mkdir, writeFile } from "node:fs/promises";
import path from "node:path";

const notices = [
  {
    name: "libmpv / mpv",
    license: "GPL-2.0-or-later or LGPL-2.1-or-later depending on build configuration",
    url: "https://github.com/mpv-player/mpv",
  },
  {
    name: "Qt 6",
    license: "LGPL-3.0-only, GPL-2.0-only, GPL-3.0-only, or commercial depending on module and distribution",
    url: "https://doc.qt.io/qt-6/licensing.html",
  },
  {
    name: "FFmpeg",
    license: "LGPL-2.1-or-later by default, or GPL when built with GPL components enabled",
    url: "https://ffmpeg.org/legal.html",
  },
  {
    name: "Tauri",
    license: "MIT OR Apache-2.0",
    url: "https://v2.tauri.app",
  },
];

const outputDirectory = path.resolve("build");
const outputFile = path.join(outputDirectory, "third-party-notices.json");

await mkdir(outputDirectory, { recursive: true });
await writeFile(outputFile, `${JSON.stringify(notices, null, 2)}\n`, "utf8");

console.log(`Wrote ${outputFile}`);
