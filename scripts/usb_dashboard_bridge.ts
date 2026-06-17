import cors from "cors";
import express from "express";
import { createServer } from "http";
import { WebSocketServer, WebSocket } from "ws";
import { SerialPort } from "serialport";
import { ReadlineParser } from "@serialport/parser-readline";

type BridgeState = {
  parameters: {
    previous: null;
    current: {
      connectionIntervalMin: number;
      connectionIntervalMax: number;
      peripheralLatency: number;
      supervisionTimeout: number;
    };
    next: {
      connectionIntervalMin: number;
      connectionIntervalMax: number;
      peripheralLatency: number;
      supervisionTimeout: number;
    } | null;
  };
  status: {
    isAdvertising: boolean;
    isConnected: boolean;
    connectedDeviceName: string | null;
    browserConnected: boolean;
    scanFilterEnabled: boolean;
    scanFilterName: string | null;
    isScanning: boolean;
    connectedDeviceAddress?: string | null;
  };
};

type Preset = {
  id: number;
  name: string;
  description: string;
  connectionIntervalMin: number;
  connectionIntervalMax: number;
  peripheralLatency: number;
  supervisionTimeout: number;
};

const BRIDGE_PORT = Number(process.env.USB_BRIDGE_PORT || 8787);
const BAUD_RATE = Number(process.env.USB_BAUD_RATE || 115200);
const POLL_MS = Number(process.env.USB_POLL_INTERVAL_MS || 2000);
const RESET_ON_CONNECT = process.env.USB_RESET_ON_CONNECT === "1";

type BridgePortInfo = {
  path: string;
  manufacturer: string;
  friendlyName: string;
  selected: boolean;
  suggested: boolean;
};

type BridgeStatus = {
  serialConnected: boolean;
  selectedPort: string | null;
  autoSelectEnabled: boolean;
  lastError: string;
  baudRate: number;
  pollIntervalMs: number;
};

const presets: Preset[] = [
  {
    id: 1,
    name: "CGM Preferred",
    description: "Maximum interval",
    connectionIntervalMin: 300,
    connectionIntervalMax: 480,
    peripheralLatency: 3,
    supervisionTimeout: 5760,
  },
  {
    id: 2,
    name: "Production Standard",
    description: "Optimized for product reliability",
    connectionIntervalMin: 270,
    connectionIntervalMax: 340,
    peripheralLatency: 7,
    supervisionTimeout: 5990,
  },
  {
    id: 3,
    name: "iOS Reconn.",
    description: "Fast initial connection setup",
    connectionIntervalMin: 250,
    connectionIntervalMax: 300,
    peripheralLatency: 8,
    supervisionTimeout: 5990,
  },
  {
    id: 4,
    name: "Droid Reconn.",
    description: "Fast data exchange",
    connectionIntervalMin: 200,
    connectionIntervalMax: 240,
    peripheralLatency: 10,
    supervisionTimeout: 5990,
  },
];

const cache = {
  state: {
    parameters: {
      previous: null,
      current: {
        connectionIntervalMin: 280,
        connectionIntervalMax: 350,
        peripheralLatency: 8,
        supervisionTimeout: 3000,
      },
      next: null,
    },
    status: {
      isAdvertising: false,
      isConnected: false,
      connectedDeviceName: null,
      browserConnected: false,
      scanFilterEnabled: false,
      scanFilterName: null,
      isScanning: false,
      connectedDeviceAddress: null,
    },
  } as BridgeState,
  history: [] as unknown[],
  scan: { type: "ble_scan_results", devices: [] as unknown[] },
  services: {
    type: "ble_services_update",
    connected: false,
    inProgress: false,
    mtu: 0,
    error: "",
    services: [] as unknown[],
  },
  security: {
    type: "security_update",
    status: {
      isConnected: false,
      isAuthenticated: false,
      currentDevice: "",
      pairedDeviceCount: 0,
      authRequired: false,
      pairingInProgress: false,
      currentPin: "",
    },
    pairedDevices: [] as unknown[],
  },
};
let serialPort: SerialPort | null = null;
let parser: ReadlineParser | null = null;
let reconnectTimer: NodeJS.Timeout | null = null;
let pollTimer: NodeJS.Timeout | null = null;
let refreshInFlight = false;
let initialPollTimer: NodeJS.Timeout | null = null;
let manualSelectedPort: string | null = process.env.ESP32_SERIAL_PORT?.trim() || null;
let lastError = "";

let pendingResolver: ((value: unknown) => void) | null = null;
let pendingRejecter: ((reason?: unknown) => void) | null = null;
let pendingTimeout: NodeJS.Timeout | null = null;
let pendingCommand: string | null = null;
let serialQueue: Promise<unknown> = Promise.resolve();
let connectDiscoveryInFlight = false;

const app = express();
app.use(cors());
app.use(express.json());

const httpServer = createServer(app);
const wss = new WebSocketServer({ server: httpServer, path: "/ws" });

function getBridgeStatus(): BridgeStatus {
  return {
    serialConnected: serialReady(),
    selectedPort: serialPort?.path || manualSelectedPort,
    autoSelectEnabled: manualSelectedPort === null,
    lastError,
    baudRate: BAUD_RATE,
    pollIntervalMs: POLL_MS,
  };
}

function broadcastBridgeStatus() {
  wsBroadcast({ type: "bridge_status", status: getBridgeStatus() });
}

async function listBridgePorts(): Promise<BridgePortInfo[]> {
  const ports = await SerialPort.list();
  return ports.map((port) => {
    const text = `${port.path} ${port.manufacturer || ""} ${port.friendlyName || ""}`.toLowerCase();
    const suggested =
      text.includes("silicon") ||
      text.includes("cp210") ||
      text.includes("ch340") ||
      text.includes("ch343") ||
      text.includes("ftdi") ||
      text.includes("usb serial") ||
      text.includes("esp32");

    return {
      path: port.path,
      manufacturer: port.manufacturer || "Unknown",
      friendlyName: port.friendlyName || "",
      selected: (serialPort?.path || manualSelectedPort) === port.path,
      suggested,
    };
  });
}

function wsBroadcast(payload: unknown) {
  const message = JSON.stringify(payload);
  wss.clients.forEach((client) => {
    if (client.readyState === WebSocket.OPEN) {
      client.send(message);
    }
  });
}

function serialReady(): boolean {
  return Boolean(serialPort && serialPort.isOpen);
}

function cleanupPending(reason: string) {
  if (pendingTimeout) {
    clearTimeout(pendingTimeout);
    pendingTimeout = null;
  }
  if (pendingRejecter) {
    pendingRejecter(new Error(reason));
  }
  pendingResolver = null;
  pendingRejecter = null;
  pendingCommand = null;
}

function isExpectedCommandResponse(command: string, parsed: any): boolean {
  const commandName = command.trim().split(/\s+/)[0]?.toUpperCase() || "";
  const responseType = typeof parsed?.type === "string" ? parsed.type : "";

  if (Object.prototype.hasOwnProperty.call(parsed, "ok")) {
    return true;
  }

  if (commandName === "GET_STATE") {
    return responseType === "state";
  }
  if (commandName === "GET_HISTORY") {
    return responseType === "history_update";
  }
  if (commandName === "GET_SCAN_RESULTS") {
    return responseType === "ble_scan_results";
  }
  if (commandName === "GET_SERVICES") {
    return responseType === "ble_services_update";
  }
  if (commandName === "GET_SECURITY") {
    return responseType === "security_update";
  }

  return false;
}

async function closeSerial(): Promise<void> {
  if (!serialPort) {
    return;
  }

  if (initialPollTimer) {
    clearTimeout(initialPollTimer);
    initialPollTimer = null;
  }

  const portToClose = serialPort;
  serialPort = null;
  parser = null;

  if (portToClose.isOpen) {
    await new Promise<void>((resolve) => {
      portToClose.close(() => resolve());
    });
  }
}

function handleUnsolicitedJson(data: any) {
  if (!data || typeof data !== "object") {
    return;
  }

  if (data.type === "state" && data.parameters && data.status) {
    cache.state.parameters = data.parameters;
    cache.state.status = {
      ...cache.state.status,
      ...data.status,
      browserConnected: true,
    };
    wsBroadcast({ type: "state_update", state: cache.state });
    return;
  }

  if (data.type === "history_update" && Array.isArray(data.history)) {
    cache.history = data.history;
    wsBroadcast({ type: "history_update", history: cache.history });
    return;
  }

  if (data.type === "ble_scan_results" && Array.isArray(data.devices)) {
    cache.scan = data;
    wsBroadcast(cache.scan);
    return;
  }

  if (data.type === "ble_services_update") {
    cache.services = {
      type: "ble_services_update",
      connected: Boolean(data.connected),
      inProgress: Boolean(data.inProgress),
      mtu: Number(data.mtu || 0),
      error: typeof data.error === "string" ? data.error : "",
      services: Array.isArray(data.services) ? data.services : [],
    };
    wsBroadcast(cache.services);
    return;
  }

  if (data.type === "security_update") {
    cache.security = data;
    wsBroadcast(cache.security);
  }
}

function extractJsonObjectsFromLine(line: string): any[] {
  const objects: any[] = [];
  let depth = 0;
  let inString = false;
  let escaping = false;
  let start = -1;

  for (let i = 0; i < line.length; i++) {
    const ch = line[i];

    if (inString) {
      if (escaping) {
        escaping = false;
      } else if (ch === "\\") {
        escaping = true;
      } else if (ch === '"') {
        inString = false;
      }
      continue;
    }

    if (ch === '"') {
      inString = true;
      continue;
    }

    if (ch === "{") {
      if (depth === 0) {
        start = i;
      }
      depth++;
      continue;
    }

    if (ch === "}" && depth > 0) {
      depth--;
      if (depth === 0 && start >= 0) {
        const candidate = line.slice(start, i + 1);
        try {
          objects.push(JSON.parse(candidate));
        } catch {
          // Ignore malformed candidates and continue scanning the line.
        }
        start = -1;
      }
    }
  }

  return objects;
}

async function sendSerialCommand(command: string, timeoutMs = 3000): Promise<any> {
  if (!serialReady() || !serialPort) {
    throw new Error("ESP32 serial port is not connected");
  }

  const execute = (): Promise<unknown> => {
    if (!serialReady() || !serialPort) {
      return Promise.reject(new Error("ESP32 serial port is not connected"));
    }

    console.log(`[serial-send] "${command}" (timeout=${timeoutMs}ms, isOpen=${serialPort!.isOpen})`);

    return new Promise((resolve, reject) => {
      pendingResolver = resolve;
      pendingRejecter = reject;
      pendingCommand = command;
      pendingTimeout = setTimeout(() => {
        console.log(`[serial-timeout] "${command}" after ${timeoutMs}ms, clearing pending`);
        cleanupPending(`Timeout waiting for response to '${command}'`);
      }, timeoutMs);

      serialPort!.write(`${command}\n`, (err) => {
        if (err) {
          console.log(`[serial-write-err] "${command}": ${err.message}`);
          cleanupPending(`Failed to write serial command '${command}': ${err.message}`);
        } else {
          console.log(`[serial-write-ok] "${command}" written`);
        }
      });
    });
  };

  // Chain onto the queue so commands are serialized; never throw due to concurrency
  const result = serialQueue.then(execute, execute);
  serialQueue = result.catch(() => {});
  return result;
}

async function autoDetectSerialPort(): Promise<string> {
  if (manualSelectedPort) {
    return manualSelectedPort;
  }

  const explicit = process.env.ESP32_SERIAL_PORT;
  if (explicit && explicit.trim() && !manualSelectedPort) {
    return explicit.trim();
  }

  const ports = await SerialPort.list();
  const match = ports.find((port) => {
    const text = `${port.path} ${port.manufacturer || ""} ${port.friendlyName || ""}`.toLowerCase();
    return (
      text.includes("silicon") ||
      text.includes("cp210") ||
      text.includes("ch340") ||
      text.includes("ch343") ||
      text.includes("ftdi") ||
      text.includes("usb serial") ||
      text.includes("esp32")
    );
  });

  if (!match) {
    throw new Error("No ESP32-compatible serial port found. Set ESP32_SERIAL_PORT env var.");
  }

  return match.path;
}

async function connectSerial(): Promise<boolean> {
  try {
    const path = await autoDetectSerialPort();
    lastError = "";
    broadcastBridgeStatus();
    console.log(`[usb-bridge] Connecting to serial port ${path} @ ${BAUD_RATE}...`);

    serialPort = new SerialPort({ path, baudRate: BAUD_RATE, autoOpen: false });

    serialPort.on("error", (err) => {
      lastError = err.message;
      broadcastBridgeStatus();
      console.error(`[usb-bridge] Serial error: ${err.message}`);
    });

    serialPort.on("close", () => {
      lastError = "Serial port closed";
      broadcastBridgeStatus();
      console.warn("[usb-bridge] Serial port closed");
      cleanupPending("Serial port closed");
      scheduleReconnect();
    });

    await new Promise<void>((resolve, reject) => {
      serialPort!.open((err) => (err ? reject(err) : resolve()));
    });

    console.log(`[serial-open] ${path} opened successfully`);

    // Optional hardware reset can help after flashing, but may force some boards into boot mode.
    // Keep it opt-in so normal runtime connections remain stable.
    if (RESET_ON_CONNECT) {
      await new Promise<void>((resolve) => {
        console.log(`[serial-reset] Toggling DTR to reset ESP32...`);
        serialPort!.set({ dtr: false, rts: true }, () => {
          setTimeout(() => {
            serialPort!.set({ dtr: true, rts: true }, () => {
              console.log(`[serial-reset-done] DTR reset complete, waiting for firmware to boot...`);
              resolve();
            });
          }, 500);
        });
      });
    } else {
      console.log(`[serial-reset] skipped (set USB_RESET_ON_CONNECT=1 to enable)`);
    }
    
    // Add raw data event listener before piping to ReadlineParser
    serialPort.on("data", (chunk: Buffer) => {
      console.log(`[serial-raw-bytes] ${chunk.length} bytes: ${chunk.toString('hex')} (${chunk.toString('utf8')})`);
    });

    serialPort.on("error", (err) => {
      console.error(`[serial-port-error] ${err.message}`);
    });

    parser = serialPort.pipe(new ReadlineParser({ delimiter: "\n" }));
    console.log(`[serial-parser] ReadlineParser attached`);

    parser.on("error", (err) => {
      console.error(`[serial-parser-error] ${err.message}`);
    });

    parser.on("close", () => {
      console.log(`[serial-parser-close] ReadlineParser closed`);
    });

    parser.on("data", (raw: string) => {
      const line = raw.trim();
      console.log(`[serial-raw] "${line}" (len=${line.length}, pending=${pendingResolver ? 'yes' : 'no'})`);

      if (!line) {
        console.log(`[serial-skip] empty line`);
        return;
      }

      const parsedObjects = extractJsonObjectsFromLine(line);
      if (parsedObjects.length === 0) {
        console.log(`[esp32] ${line}`);
        return;
      }

      for (const parsed of parsedObjects) {
        if (pendingResolver && pendingCommand && isExpectedCommandResponse(pendingCommand, parsed)) {
          const resolve = pendingResolver;
          pendingResolver = null;
          pendingRejecter = null;
          pendingCommand = null;
          if (pendingTimeout) {
            clearTimeout(pendingTimeout);
            pendingTimeout = null;
          }
          resolve(parsed);
        } else {
          handleUnsolicitedJson(parsed);
        }
      }
    });

    console.log("[usb-bridge] Serial connected");
    lastError = "";
    broadcastBridgeStatus();
    if (initialPollTimer) {
      clearTimeout(initialPollTimer);
    }
    // Wait longer after reset for firmware to fully boot
    initialPollTimer = setTimeout(() => {
      initialPollTimer = null;
      console.log(`[usb-bridge] Initial poll delay complete, starting first GET_STATE`);
      refreshStateFromEsp32().catch((err) => {
        const msg = err instanceof Error ? err.message : String(err);
        console.warn(`[usb-bridge] Initial poll failed: ${msg}`);
      });
      startPolling();
    }, 4000);
    return true;
  } catch (error) {
    const msg = error instanceof Error ? error.message : String(error);
    lastError = msg;
     // Release any partially-opened handle so it does not hold the port
     if (serialPort) {
       try { if (serialPort.isOpen) serialPort.close(() => {}); } catch {}
       serialPort = null;
       parser = null;
     }
    broadcastBridgeStatus();
    console.error(`[usb-bridge] Connect failed: ${msg}`);
    scheduleReconnect();
    return false;
  }
}


function scheduleReconnect() {
  if (reconnectTimer) {
    return;
  }
  if (pollTimer) {
    clearInterval(pollTimer);
    pollTimer = null;
  }
  reconnectTimer = setTimeout(() => {
    reconnectTimer = null;
    broadcastBridgeStatus();
    connectSerial().catch(() => {
      // handled in connectSerial
    });
    }, 3500);
}

function startPolling() {
  if (pollTimer) {
    clearInterval(pollTimer);
  }
  pollTimer = setInterval(() => {
    refreshFromEsp32().catch((err) => {
      const msg = err instanceof Error ? err.message : String(err);
      console.warn(`[usb-bridge] Poll failed: ${msg}`);
    });
  }, POLL_MS);
}

async function refreshStateFromEsp32() {
  if (!serialReady() || refreshInFlight) {
    return;
  }

  refreshInFlight = true;
  try {
    const state = await sendSerialCommand("GET_STATE", 8000);
    if (state?.type === "state") {
      cache.state.parameters = state.parameters;
      cache.state.status = {
        ...cache.state.status,
        ...state.status,
        browserConnected: true,
      };
      wsBroadcast({ type: "state_update", state: cache.state });
    }
  } finally {
    refreshInFlight = false;
  }
}

async function refreshFromEsp32() {
  if (!serialReady() || refreshInFlight) {
    return;
  }

  refreshInFlight = true;
  try {
    const state = await sendSerialCommand("GET_STATE");
    if (state?.type === "state") {
      cache.state.parameters = state.parameters;
      cache.state.status = {
        ...cache.state.status,
        ...state.status,
        browserConnected: true,
      };
      wsBroadcast({ type: "state_update", state: cache.state });
    }

    const history = await sendSerialCommand("GET_HISTORY");
    if (history?.type === "history_update") {
      cache.history = Array.isArray(history.history) ? history.history : [];
      wsBroadcast({ type: "history_update", history: cache.history });
    }

    const scan = await sendSerialCommand("GET_SCAN_RESULTS");
    if (scan?.type === "ble_scan_results") {
      cache.scan = scan;
      wsBroadcast(cache.scan);
    }

    const services = await sendSerialCommand("GET_SERVICES");
    if (services?.type === "ble_services_update") {
      cache.services = {
        type: "ble_services_update",
        connected: Boolean(services.connected),
        inProgress: Boolean(services.inProgress),
        mtu: Number(services.mtu || 0),
        error: typeof services.error === "string" ? services.error : "",
        services: Array.isArray(services.services) ? services.services : [],
      };
      wsBroadcast(cache.services);
    }

    const security = await sendSerialCommand("GET_SECURITY");
    if (security?.type === "security_update") {
      cache.security = security;
      wsBroadcast(cache.security);
    }
  } finally {
    refreshInFlight = false;
  }
}

async function runPostConnectDiscovery(address?: string) {
  if (connectDiscoveryInFlight || !serialReady()) {
    return;
  }

  connectDiscoveryInFlight = true;
  try {
    // Connect is asynchronous on firmware: keep polling briefly until
    // we observe either discovery in progress or populated services.
    for (let attempt = 1; attempt <= 12; attempt++) {
      await refreshFromEsp32();

      const connected = Boolean(cache.state.status.isConnected);
      const discoveryInProgress = Boolean(cache.services.inProgress);
      const hasServices = Array.isArray(cache.services.services) && cache.services.services.length > 0;

      if (connected && (discoveryInProgress || hasServices)) {
        console.log(
          `[usb-bridge] Post-connect discovery ready for ${address || "(unknown)"} ` +
            `(attempt=${attempt}, inProgress=${discoveryInProgress}, services=${cache.services.services.length})`,
        );
        return;
      }

      await new Promise((resolve) => setTimeout(resolve, 500));
    }

    console.warn(
      `[usb-bridge] Post-connect discovery timed out for ${address || "(unknown)"}; ` +
        "device may be connected but has not reported GATT services yet.",
    );
  } catch (err) {
    const msg = err instanceof Error ? err.message : String(err);
    console.warn(`[usb-bridge] Post-connect discovery failed: ${msg}`);
  } finally {
    connectDiscoveryInFlight = false;
  }
}

app.get("/api/state", (_req, res) => {
  res.json(cache.state);
});

app.get("/api/bridge/status", (_req, res) => {
  res.json(getBridgeStatus());
});

app.get("/api/bridge/ports", async (_req, res) => {
  try {
    const ports = await listBridgePorts();
    res.json({ ports });
  } catch (error) {
    const msg = error instanceof Error ? error.message : String(error);
    res.status(500).json({ error: msg });
  }
});

app.post("/api/bridge/select-port", async (req, res) => {
  try {
    const path = typeof req.body?.path === "string" ? req.body.path.trim() : "";
    const autoSelect = Boolean(req.body?.autoSelect);

    if (!autoSelect && !path) {
      res.status(400).json({ error: "Missing port path" });
      return;
    }

    if (reconnectTimer) {
      clearTimeout(reconnectTimer);
      reconnectTimer = null;
    }

    manualSelectedPort = autoSelect ? null : path;
    await closeSerial();
    const connected = await connectSerial();

    res.json({ success: connected, status: getBridgeStatus() });
  } catch (error) {
    const msg = error instanceof Error ? error.message : String(error);
    res.status(500).json({ error: msg });
  }
});

app.get("/api/history", (_req, res) => {
  res.json(cache.history);
});

app.get("/api/presets", (_req, res) => {
  res.json(presets);
});

app.get("/api/security", (_req, res) => {
  res.json({
    status: cache.security.status,
    pairedDevices: cache.security.pairedDevices,
  });
});

app.post("/api/security/remove-device", async (req, res) => {
  try {
    const address = String(req.body?.address || "").trim();
    if (!address) {
      res.status(400).json({ error: "Missing address" });
      return;
    }

    const response = await sendSerialCommand(`REMOVE_PAIRED ${address}`);
    if (!response?.ok) {
      res.status(400).json({ error: response?.error || "Failed to remove device" });
      return;
    }

    await refreshFromEsp32();
    res.json({ success: true });
  } catch (error) {
    const msg = error instanceof Error ? error.message : String(error);
    res.status(500).json({ error: msg });
  }
});

app.post("/api/security/toggle-auth", async (req, res) => {
  try {
    const enabled = Boolean(req.body?.enabled);
    const response = await sendSerialCommand(`SET_AUTH_REQUIRED ${enabled ? 1 : 0}`);
    if (!response?.ok) {
      res.status(400).json({ error: response?.error || "Failed to update auth setting" });
      return;
    }

    await refreshFromEsp32();
    res.json({ success: true, enabled });
  } catch (error) {
    const msg = error instanceof Error ? error.message : String(error);
    res.status(500).json({ error: msg });
  }
});

wss.on("connection", (socket) => {
  cache.state.status.browserConnected = true;
  socket.send(JSON.stringify({ type: "bridge_status", status: getBridgeStatus() }));
  socket.send(JSON.stringify({ type: "state_update", state: cache.state }));
  socket.send(JSON.stringify({ type: "history_update", history: cache.history }));
  socket.send(JSON.stringify(cache.scan));
  socket.send(JSON.stringify(cache.services));
  socket.send(JSON.stringify(cache.security));

  socket.on("message", async (payload) => {
    let message: any;
    try {
      message = JSON.parse(String(payload));
    } catch {
      return;
    }

    try {
      if (message.type === "update_parameters") {
        const current = cache.state.parameters.current;
        const next = {
          connectionIntervalMin: Number(message.parameters?.connectionIntervalMin ?? current.connectionIntervalMin),
          connectionIntervalMax: Number(message.parameters?.connectionIntervalMax ?? current.connectionIntervalMax),
          peripheralLatency: Number(message.parameters?.peripheralLatency ?? current.peripheralLatency),
          supervisionTimeout: Number(message.parameters?.supervisionTimeout ?? current.supervisionTimeout),
        };

        const stageResponse = await sendSerialCommand(
          `SET_NEXT ${next.connectionIntervalMin} ${next.connectionIntervalMax} ${next.peripheralLatency} ${next.supervisionTimeout}`,
        );

        if (stageResponse?.ok) {
          socket.send(JSON.stringify({ type: "parameter_update_success" }));
          await refreshFromEsp32();
        } else {
          socket.send(
            JSON.stringify({
              type: "parameter_update_error",
              error: stageResponse?.error || "Failed to stage parameters",
            }),
          );
        }
        return;
      }

      if (message.type === "apply_parameters") {
        const applyResponse = await sendSerialCommand("APPLY");
        if (applyResponse?.ok) {
          await refreshFromEsp32();
          socket.send(JSON.stringify({ type: "apply_parameters_success" }));
        } else {
          socket.send(
            JSON.stringify({
              type: "apply_parameters_error",
              error: applyResponse?.error || "Failed to apply parameters",
            }),
          );
        }
        return;
      }

      if (message.type === "clear_parameters") {
        const clearResponse = await sendSerialCommand("CLEAR_NEXT");
        if (clearResponse?.ok) {
          await refreshFromEsp32();
          socket.send(JSON.stringify({ type: "clear_parameters_success" }));
        } else {
          socket.send(
            JSON.stringify({
              type: "parameter_update_error",
              error: clearResponse?.error || "Failed to clear next parameters",
            }),
          );
        }
        return;
      }

      if (message.type === "ble_command") {
        let cmd = "";
        const command = String(message.command || "");
        let timeoutMs = 3000;

        if (command === "set_scan_filter") {
          cmd = `SET_FILTER ${String(message.deviceName || "").trim()}`;
          timeoutMs = 8000;
        } else if (command === "clear_scan_filter") {
          cmd = "CLEAR_FILTER";
        } else if (command === "start_scan") {
          cmd = "SCAN_NOW";
        } else if (command === "discover_services") {
          cmd = "GET_SERVICES";
          timeoutMs = 8000;
        } else if (command === "disconnect") {
          cmd = "DISCONNECT";
        } else if (command === "connect_device") {
          cmd = `CONNECT ${String(message.address || "").trim()}`;
        }

        if (!cmd) {
          socket.send(
            JSON.stringify({
              type: "ble_command_response",
              success: false,
              command,
              error: "Unsupported BLE command",
            }),
          );
          return;
        }

        let response = await sendSerialCommand(cmd, timeoutMs);
        const responseErrorText = String(response?.error || "").toLowerCase();
        const isBusyResponse = !response?.ok && (responseErrorText.includes("busy") || responseErrorText.includes("in progress"));
        if (isBusyResponse) {
          await new Promise((resolve) => setTimeout(resolve, 250));
          response = await sendSerialCommand(cmd, timeoutMs);
        }

        const commandSucceeded = Boolean(response?.ok);
        socket.send(
          JSON.stringify({
            type: "ble_command_response",
            success: commandSucceeded,
            command,
            error: commandSucceeded ? undefined : response?.error || "BLE command failed",
          }),
        );

        if (commandSucceeded) {
          refreshFromEsp32().catch((err) => {
            const msg = err instanceof Error ? err.message : String(err);
            console.warn(`[usb-bridge] Post-command refresh failed for ${command}: ${msg}`);
          });

          if (command === "connect_device") {
            const requestedAddress = String(message.address || "").trim();
            runPostConnectDiscovery(requestedAddress).catch((err) => {
              const msg = err instanceof Error ? err.message : String(err);
              console.warn(`[usb-bridge] Post-connect discovery task failed: ${msg}`);
            });
          }
        }
        return;
      }
    } catch (error) {
      const msg = error instanceof Error ? error.message : String(error);
      socket.send(JSON.stringify({ type: "parameter_update_error", error: msg }));
    }
  });

  socket.on("close", () => {
    const hasOpenClients = Array.from(wss.clients).some((c) => c.readyState === WebSocket.OPEN);
    cache.state.status.browserConnected = hasOpenClients;
  });
});

httpServer.listen(BRIDGE_PORT, () => {
  console.log(`[usb-bridge] HTTP+WS bridge listening at http://127.0.0.1:${BRIDGE_PORT}`);
  console.log("[usb-bridge] WS endpoint: ws://127.0.0.1:" + BRIDGE_PORT + "/ws");
  broadcastBridgeStatus();
  connectSerial().catch(() => {
    // handled in connectSerial
  });
});
