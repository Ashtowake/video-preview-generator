import { mkdir, writeFile } from "node:fs/promises";
import path from "node:path";

const notices = [
  {
    name: "FFmpeg",
    license: "LGPL/GPL depending on distribution",
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
