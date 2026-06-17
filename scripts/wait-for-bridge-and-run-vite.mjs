import { spawn } from "node:child_process";

const BRIDGE_BASE_URL = process.env.VITE_API_BASE_URL || "http://127.0.0.1:8787";
const HEALTH_URL = `${BRIDGE_BASE_URL.replace(/\/$/, "")}/api/bridge/status`;
const STARTUP_TIMEOUT_MS = Number(process.env.BRIDGE_HEALTH_TIMEOUT_MS || 45000);
const POLL_INTERVAL_MS = Number(process.env.BRIDGE_HEALTH_POLL_MS || 750);

async function delay(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function waitForBridge() {
  const startedAt = Date.now();
  let lastError = "";

  while (Date.now() - startedAt < STARTUP_TIMEOUT_MS) {
    try {
      const response = await fetch(HEALTH_URL, { method: "GET" });
      if (response.ok) {
        console.log(`[dashboard] Bridge is healthy at ${HEALTH_URL}`);
        return;
      }
      lastError = `HTTP ${response.status}`;
    } catch (error) {
      lastError = error instanceof Error ? error.message : String(error);
    }

    await delay(POLL_INTERVAL_MS);
  }

  throw new Error(
    `Bridge health check timed out after ${STARTUP_TIMEOUT_MS}ms (${HEALTH_URL}). Last error: ${lastError}`,
  );
}

async function main() {
  await waitForBridge();

  const vite = spawn("npm", ["run", "dev"], {
    stdio: "inherit",
    shell: process.platform === "win32",
  });

  vite.on("exit", (code) => {
    process.exit(code ?? 0);
  });
}

main().catch((error) => {
  const message = error instanceof Error ? error.message : String(error);
  console.error(`[dashboard] ${message}`);
  process.exit(1);
});
