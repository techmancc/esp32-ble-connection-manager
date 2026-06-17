import { rm, readdir } from "node:fs/promises";
import { existsSync } from "node:fs";
import path from "node:path";

const root = process.cwd();
const aggressive = process.argv.includes("--aggressive");

const targets = [
  "dist",
  "bridge_debug.log",
  "upload.log",
];

async function removeIfExists(relativePath) {
  const fullPath = path.join(root, relativePath);
  if (!existsSync(fullPath)) {
    return;
  }

  await rm(fullPath, { recursive: true, force: true });
  console.log(`[clean] removed ${relativePath}`);
}

async function cleanLogsFolder() {
  const logsDir = path.join(root, "logs");
  if (!existsSync(logsDir)) {
    return;
  }

  const entries = await readdir(logsDir, { withFileTypes: true });
  for (const entry of entries) {
    if (!entry.isFile()) {
      continue;
    }
    await removeIfExists(path.join("logs", entry.name));
  }
}

async function main() {
  for (const target of targets) {
    await removeIfExists(target);
  }

  await cleanLogsFolder();

  if (aggressive) {
    // Optional heavier cleanup for local development machines.
    await removeIfExists("node_modules");
    await removeIfExists(".local");
    console.log("[clean] aggressive mode complete");
  }

  console.log("[clean] workspace cleanup complete");
}

main().catch((error) => {
  console.error(`[clean] failed: ${error instanceof Error ? error.message : String(error)}`);
  process.exit(1);
});
