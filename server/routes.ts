import type { Express } from "express";
import { createServer, type Server } from "http";
import { WebSocketServer, WebSocket } from "ws";
import { storage } from "./storage";
import { updateParametersSchema, type UpdateParameters } from "@shared/schema";

export async function registerRoutes(app: Express): Promise<Server> {
  app.get("/api/state", async (req, res) => {
    try {
      const state = await storage.getState();
      res.json(state);
    } catch (error) {
      res.status(500).json({ error: "Failed to get state" });
    }
  });

  app.post("/api/parameters/next", async (req, res) => {
    try {
      const validated = updateParametersSchema.partial().parse(req.body);
      const state = await storage.updateNextParameters(validated);
      res.json(state);
    } catch (error) {
      res.status(400).json({ error: "Invalid parameters" });
    }
  });

  app.post("/api/parameters/apply", async (req, res) => {
    try {
      const state = await storage.applyNextParameters();
      res.json(state);
    } catch (error) {
      res.status(500).json({ error: "Failed to apply parameters" });
    }
  });

  app.get("/api/history", async (req, res) => {
    try {
      const limit = req.query.limit ? parseInt(req.query.limit as string) : 50;
      const history = await storage.getHistory(limit);
      res.json(history);
    } catch (error) {
      res.status(500).json({ error: "Failed to get history" });
    }
  });

  app.get("/api/presets", async (req, res) => {
    try {
      const presets = await storage.getPresets();
      res.json(presets);
    } catch (error) {
      res.status(500).json({ error: "Failed to get presets" });
    }
  });

  app.post("/api/presets", async (req, res) => {
    try {
      const preset = await storage.createPreset(req.body);
      res.json(preset);
    } catch (error) {
      res.status(400).json({ error: "Failed to create preset" });
    }
  });

  const httpServer = createServer(app);

  const wss = new WebSocketServer({ server: httpServer, path: '/ws' });

  const clients = new Set<WebSocket>();

  const broadcastState = async () => {
    const state = await storage.getState();
    const message = JSON.stringify({
      type: "state_update",
      state,
    });

    clients.forEach((client) => {
      if (client.readyState === WebSocket.OPEN) {
        client.send(message);
      }
    });
  };

  const broadcastHistoryUpdate = async () => {
    const history = await storage.getHistory(10);
    const message = JSON.stringify({
      type: "history_update",
      history,
    });

    clients.forEach((client) => {
      if (client.readyState === WebSocket.OPEN) {
        client.send(message);
      }
    });
  };

  const simulateESP32Connection = async () => {
    await new Promise(resolve => setTimeout(resolve, 2000));
    
    await storage.updateStatus({
      isConnected: true,
      connectedDeviceName: "iPhone 14 Pro",
    });
    await broadcastState();
  };

  let esp32SimulationInterval: NodeJS.Timeout | null = null;

  const startESP32Simulation = () => {
    if (esp32SimulationInterval) return;
    
    esp32SimulationInterval = setInterval(async () => {
      const state = await storage.getState();
      if (state.parameters.next && state.status.isConnected) {
        await new Promise(resolve => setTimeout(resolve, 1000));
        await storage.applyNextParameters();
        await broadcastState();
        await broadcastHistoryUpdate();
      }
    }, 5000);
  };

  const stopESP32Simulation = () => {
    if (esp32SimulationInterval) {
      clearInterval(esp32SimulationInterval);
      esp32SimulationInterval = null;
    }
  };

  wss.on('connection', async (ws) => {
    console.log('WebSocket client connected');
    clients.add(ws);

    await storage.updateStatus({ browserConnected: true });

    const initialState = await storage.getState();
    ws.send(JSON.stringify({
      type: "state_update",
      state: initialState,
    }));

    const initialHistory = await storage.getHistory(10);
    ws.send(JSON.stringify({
      type: "history_update",
      history: initialHistory,
    }));

    if (!initialState.status.isConnected) {
      simulateESP32Connection();
    }

    startESP32Simulation();

    ws.on('message', async (data) => {
      try {
        const message = JSON.parse(data.toString());

        if (message.type === 'update_parameters') {
          const params = message.parameters;

          try {
            const validated = updateParametersSchema.partial().parse(params);

            await storage.updateNextParameters(validated);

            ws.send(JSON.stringify({
              type: "parameter_update_success",
            }));

            await broadcastState();
          } catch (error) {
            ws.send(JSON.stringify({
              type: "parameter_update_error",
              error: "Parameters out of valid range",
            }));
          }
        }
      } catch (error) {
        console.error('Error processing message:', error);
        ws.send(JSON.stringify({
          type: "parameter_update_error",
          error: "Invalid message format",
        }));
      }
    });

    ws.on('close', async () => {
      console.log('WebSocket client disconnected');
      clients.delete(ws);
      
      if (clients.size === 0) {
        await storage.updateStatus({ browserConnected: false });
        stopESP32Simulation();
      }
    });

    ws.on('error', (error) => {
      console.error('WebSocket error:', error);
    });
  });

  return httpServer;
}
