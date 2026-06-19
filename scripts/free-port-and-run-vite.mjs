import { execSync, spawn } from "node:child_process";

const PORT = Number(process.env.DEV_PORT || 5173);

function getPidsOnPort(port) {
  if (process.platform === "win32") {
    const output = execSync(`netstat -ano -p tcp | findstr :${port}`, {
      encoding: "utf8",
      stdio: ["ignore", "pipe", "ignore"],
    });

    const pids = new Set();
    for (const line of output.split(/\r?\n/)) {
      const trimmed = line.trim();
      if (!trimmed || !trimmed.includes("LISTENING")) {
        continue;
      }
      const parts = trimmed.split(/\s+/);
      const pid = Number(parts[parts.length - 1]);
      if (Number.isInteger(pid) && pid > 0) {
        pids.add(pid);
      }
    }
    return [...pids];
  }

  const output = execSync(`lsof -ti tcp:${port}`, {
    encoding: "utf8",
    stdio: ["ignore", "pipe", "ignore"],
  });

  return output
    .split(/\r?\n/)
    .map((value) => Number(value.trim()))
    .filter((value) => Number.isInteger(value) && value > 0);
}

function killPid(pid) {
  if (process.platform === "win32") {
    execSync(`taskkill /PID ${pid} /F`, { stdio: "ignore" });
    return;
  }
  process.kill(pid, "SIGKILL");
}

function freePort(port) {
  let pids = [];
  try {
    pids = getPidsOnPort(port);
  } catch {
    pids = [];
  }

  if (pids.length === 0) {
    console.log(`[dev] Port ${port} is already free.`);
    return;
  }

  console.log(`[dev] Killing ${pids.length} process(es) on port ${port}: ${pids.join(", ")}`);
  for (const pid of pids) {
    try {
      killPid(pid);
    } catch (error) {
      console.warn(`[dev] Failed to kill PID ${pid}: ${error.message}`);
    }
  }
}

freePort(PORT);

const vite = spawn("npx", ["vite", "--host", "--port", String(PORT), "--strictPort"], {
  stdio: "inherit",
  shell: process.platform === "win32",
});

vite.on("exit", (code) => {
  process.exit(code ?? 0);
});
