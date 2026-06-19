import { useState, useEffect, useCallback, useRef } from "react";
import { Button } from "@/components/ui/button";
import { StatusPanel } from "@/components/StatusPanel";
import { SecurityPanel } from "@/components/SecurityPanel";
import { ConnectionParametersTable } from "@/components/ConnectionParametersTable";
import { HistoryLog } from "@/components/HistoryLog";
import { PresetSelector } from "@/components/PresetSelector";
import { ParameterChart } from "@/components/ParameterChart";
import { ExportButtons } from "@/components/ExportButtons";
import { ThemeToggle } from "@/components/ThemeToggle";
import { BleClientControlPanel } from "@/components/BleClientControlPanel";
import { BleServicesPanel } from "@/components/BleServicesPanel";
import { useToast } from "@/hooks/use-toast";
import { Cpu, Send, X, Loader2, RefreshCw, Usb } from "lucide-react";
import { useQuery } from "@tanstack/react-query";
import { discoverEsp32, getWebSocketUrl } from "@/lib/utils";
import type { BleScanDevice, BleServiceSummary, DashboardState, UpdateParameters, ParameterHistory, ParameterPreset } from "@shared/schema";

type BridgeStatus = {
  serialConnected: boolean;
  selectedPort: string | null;
  autoSelectEnabled: boolean;
  lastError: string;
  baudRate: number;
  pollIntervalMs: number;
};

type BridgePort = {
  path: string;
  manufacturer: string;
  friendlyName: string;
  selected: boolean;
  suggested: boolean;
};

export default function Dashboard() {
  const { toast } = useToast();
  const [state, setState] = useState<DashboardState>({
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
    },
  });

  const [history, setHistory] = useState<ParameterHistory[]>([]);

  const [nextValues, setNextValues] = useState({
    connectionIntervalMin: "",
    connectionIntervalMax: "",
    peripheralLatency: "",
    supervisionTimeout: "",
  });

  const [isSending, setIsSending] = useState(false);
  const [ws, setWs] = useState<WebSocket | null>(null);
  const [discoveredDevices, setDiscoveredDevices] = useState<BleScanDevice[]>([]);
  const [discoveredServices, setDiscoveredServices] = useState<BleServiceSummary[]>([]);
  const [servicesInProgress, setServicesInProgress] = useState(false);
  const [servicesError, setServicesError] = useState<string | null>(null);
  const [negotiatedMtu, setNegotiatedMtu] = useState<number | null>(null);
  const [bridgeStatus, setBridgeStatus] = useState<BridgeStatus | null>(null);
  const [bridgePorts, setBridgePorts] = useState<BridgePort[]>([]);
  const [selectedBridgePort, setSelectedBridgePort] = useState("");
  const [isSwitchingPort, setIsSwitchingPort] = useState(false);
  const [startupWaitSeconds, setStartupWaitSeconds] = useState(0);
  const previousNextRef = useRef<typeof state.parameters.next>(null);
  const wasConnectedRef = useRef(false);

  const { data: presets = [] } = useQuery<ParameterPreset[]>({
    queryKey: ["presets"],
    queryFn: async () => {
      const baseUrl = await discoverEsp32();
      const response = await fetch(`${baseUrl}/api/presets`);
      if (!response.ok) {
        throw new Error('Failed to fetch presets');
      }
      return response.json();
    },
  });

  useEffect(() => {
    if (state.parameters.next) {
      setNextValues({
        connectionIntervalMin: state.parameters.next.connectionIntervalMin.toString(),
        connectionIntervalMax: state.parameters.next.connectionIntervalMax.toString(),
        peripheralLatency: state.parameters.next.peripheralLatency.toString(),
        supervisionTimeout: state.parameters.next.supervisionTimeout.toString(),
      });
      previousNextRef.current = state.parameters.next;
    } else if (previousNextRef.current !== null && state.parameters.next === null) {
      setNextValues({
        connectionIntervalMin: "",
        connectionIntervalMax: "",
        peripheralLatency: "",
        supervisionTimeout: "",
      });
      
      toast({
        title: "Parameters Applied",
        description: "ESP32 has successfully applied the new connection parameters.",
      });
      
      previousNextRef.current = null;
    }
  }, [state.parameters.next, toast]);

  const loadBridgeMeta = useCallback(async () => {
    try {
      const baseUrl = await discoverEsp32();

      const [statusResponse, portsResponse] = await Promise.all([
        fetch(`${baseUrl}/api/bridge/status`),
        fetch(`${baseUrl}/api/bridge/ports`),
      ]);

      if (statusResponse.ok) {
        const statusData = (await statusResponse.json()) as BridgeStatus;
        setBridgeStatus(statusData);
        setSelectedBridgePort(statusData.selectedPort || "");
      }

      if (portsResponse.ok) {
        const portsData = (await portsResponse.json()) as { ports: BridgePort[] };
        setBridgePorts(Array.isArray(portsData.ports) ? portsData.ports : []);
      }
    } catch (error) {
      console.error("Failed to load USB bridge metadata", error);
    }
  }, []);

  useEffect(() => {
    loadBridgeMeta();
    const intervalId = setInterval(loadBridgeMeta, 3000);

    return () => {
      clearInterval(intervalId);
    };
  }, [loadBridgeMeta]);

  const handlePortModeSwitch = useCallback(async (autoSelect: boolean) => {
    try {
      setIsSwitchingPort(true);
      const baseUrl = await discoverEsp32();

      const payload = autoSelect
        ? { autoSelect: true }
        : { autoSelect: false, path: selectedBridgePort };

      const response = await fetch(`${baseUrl}/api/bridge/select-port`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
      });

      const data = await response.json();
      if (!response.ok || !data.success) {
        throw new Error(data.error || "Failed to switch bridge port mode");
      }

      await loadBridgeMeta();
      toast({
        title: autoSelect ? "Auto Port Selection Enabled" : "Port Selected",
        description: autoSelect
          ? "Bridge will automatically choose an ESP32 serial port."
          : `Bridge switched to ${selectedBridgePort}.`,
      });
    } catch (error) {
      toast({
        title: "Port Switch Failed",
        description: error instanceof Error ? error.message : "Could not update bridge port mode.",
        variant: "destructive",
      });
    } finally {
      setIsSwitchingPort(false);
    }
  }, [loadBridgeMeta, selectedBridgePort, toast]);

  useEffect(() => {
    const connectWebSocket = () => {
      // Use direct WebSocket URL for testing
      const wsUrl = import.meta.env.VITE_WS_URL || 'ws://127.0.0.1:8787/ws';
      console.log('Attempting WebSocket connection to:', wsUrl);
      const socket = new WebSocket(wsUrl);

      socket.onopen = () => {
        console.log("WebSocket connected to:", wsUrl);
        setState((prev) => ({
          ...prev,
          status: {
            ...prev.status,
            browserConnected: true,
          },
        }));
      };

      socket.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          if (data.type === "state_update") {
            const nextConnected = Boolean(data.state?.status?.isConnected);
            setState({
              ...data.state,
              status: {
                ...data.state.status,
                browserConnected: true,
              },
            });

            // Kick off GATT discovery exactly when BLE link transitions to connected.
            if (nextConnected && !wasConnectedRef.current && socket.readyState === WebSocket.OPEN) {
              socket.send(JSON.stringify({ type: "ble_command", command: "discover_services" }));
            }

            wasConnectedRef.current = nextConnected;
          } else if (data.type === "bridge_status") {
            setBridgeStatus(data.status as BridgeStatus);
          } else if (data.type === "ble_scan_results") {
            setDiscoveredDevices(Array.isArray(data.devices) ? data.devices : []);
          } else if (data.type === "ble_services_update") {
            setDiscoveredServices(Array.isArray(data.services) ? data.services : []);
            setServicesInProgress(Boolean(data.inProgress));
            setServicesError(typeof data.error === "string" && data.error.length > 0 ? data.error : null);
            setNegotiatedMtu(typeof data.mtu === "number" && data.mtu > 0 ? data.mtu : null);
          } else if (data.type === "history_update") {
            setHistory(data.history);
          } else if (data.type === "parameter_update_success") {
            // Parameters staged - now apply them to BLE connection
            if (socket.readyState === WebSocket.OPEN) {
              socket.send(JSON.stringify({ type: "apply_parameters" }));
            }
            toast({
              title: "Parameters Staged", 
              description: "Parameters staged, applying to BLE connection...",
            });
          } else if (data.type === "apply_parameters_success") {
            toast({
              title: "Success!",
              description: "Connection parameters applied to BLE device successfully.",
            });
            setIsSending(false);
            setNextValues({
              connectionIntervalMin: "",
              connectionIntervalMax: "",
              peripheralLatency: "",
              supervisionTimeout: "",
            });
          } else if (data.type === "apply_parameters_error") {
            toast({
              title: "Apply Error",
              description: data.error || "Failed to apply parameters to BLE connection.",
              variant: "destructive",
            });
            setIsSending(false);
          } else if (data.type === "clear_parameters_success") {
            toast({
              title: "Cleared",
              description: "Next parameters cleared successfully.",
            });
          } else if (data.type === "ble_command_response") {
            if (data.success) {
              toast({
                title: "BLE Command Sent",
                description: `Command '${data.command}' completed successfully.`,
              });
            } else {
              toast({
                title: "BLE Command Failed",
                description: data.error || `Command '${data.command}' failed.`,
                variant: "destructive",
              });
            }
          } else if (data.type === "parameter_update_error") {
            toast({
              title: "Error",
              description: data.error || "Failed to send parameters to ESP32.",
              variant: "destructive",
            });
            setIsSending(false);
          }
        } catch (error) {
          console.error("Error parsing WebSocket message:", error);
        }
      };

      socket.onerror = (error) => {
        console.error("WebSocket error:", error);
        setState((prev) => ({
          ...prev,
          status: {
            ...prev.status,
            browserConnected: false,
          },
        }));
        toast({
          title: "Connection Error",
          description: "Failed to connect to ESP32 server.",
          variant: "destructive",
        });
      };

      socket.onclose = () => {
        console.log("WebSocket disconnected, attempting to reconnect...");
        wasConnectedRef.current = false;
        setState((prev) => ({
          ...prev,
          status: {
            ...prev.status,
            browserConnected: false,
          },
        }));
        setDiscoveredDevices([]);
        setDiscoveredServices([]);
        setServicesInProgress(false);
        setServicesError(null);
        setNegotiatedMtu(null);
        setTimeout(connectWebSocket, 3000);
      };

      setWs(socket);
    };

    connectWebSocket();

    return () => {
      ws?.close();
    };
  }, [toast]);

  const handleNextValueChange = useCallback((field: string, value: string) => {
    setNextValues((prev) => ({ ...prev, [field]: value }));
  }, []);

  const handleClearNext = useCallback(() => {
    setNextValues({
      connectionIntervalMin: "",
      connectionIntervalMax: "",
      peripheralLatency: "",
      supervisionTimeout: "",
    });
    
    // Send clear command to ESP32
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ type: "clear_parameters" }));
    }
    
    toast({
      title: "Cleared",
      description: "Next parameter values have been cleared.",
    });
  }, [ws, toast]);

  const handleApplyPreset = useCallback((preset: ParameterPreset) => {
    setNextValues({
      connectionIntervalMin: preset.connectionIntervalMin.toString(),
      connectionIntervalMax: preset.connectionIntervalMax.toString(),
      peripheralLatency: preset.peripheralLatency.toString(),
      supervisionTimeout: preset.supervisionTimeout.toString(),
    });
    
    toast({
      title: "Preset Applied",
      description: `${preset.name} parameters loaded into Next column. Click Send to apply.`,
    });
  }, [toast]);

  const handleSendNext = useCallback(() => {
    const hasAnyValue = Object.values(nextValues).some((v) => v !== "");
    if (!hasAnyValue) {
      toast({
        title: "No Values",
        description: "Please enter at least one parameter value to send.",
        variant: "destructive",
      });
      return;
    }

    const params: Partial<UpdateParameters> = {};
    if (nextValues.connectionIntervalMin !== "") {
      const val = parseFloat(nextValues.connectionIntervalMin);
      if (isNaN(val) || val < 7.5 || val > 4000) {
        toast({
          title: "Invalid Value",
          description: "Connection Interval Min must be between 7.5 and 4000 ms.",
          variant: "destructive",
        });
        return;
      }
      params.connectionIntervalMin = val;
    }
    if (nextValues.connectionIntervalMax !== "") {
      const val = parseFloat(nextValues.connectionIntervalMax);
      if (isNaN(val) || val < 7.5 || val > 4000) {
        toast({
          title: "Invalid Value",
          description: "Connection Interval Max must be between 7.5 and 4000 ms.",
          variant: "destructive",
        });
        return;
      }
      params.connectionIntervalMax = val;
    }
    if (nextValues.peripheralLatency !== "") {
      const val = parseInt(nextValues.peripheralLatency, 10);
      if (isNaN(val) || val < 0 || val > 499) {
        toast({
          title: "Invalid Value",
          description: "Peripheral Latency must be between 0 and 499.",
          variant: "destructive",
        });
        return;
      }
      params.peripheralLatency = val;
    }
    if (nextValues.supervisionTimeout !== "") {
      const val = parseFloat(nextValues.supervisionTimeout);
      if (isNaN(val) || val < 100 || val > 32000) {
        toast({
          title: "Invalid Value",
          description: "Supervision Timeout must be between 100 and 32000 ms.",
          variant: "destructive",
        });
        return;
      }
      params.supervisionTimeout = val;
    }

    if (ws && ws.readyState === WebSocket.OPEN) {
      setIsSending(true);
      ws.send(
        JSON.stringify({
          type: "update_parameters",
          parameters: params,
        })
      );
    } else {
      toast({
        title: "Connection Error",
        description: "WebSocket is not connected. Please refresh the page.",
        variant: "destructive",
      });
    }
  }, [nextValues, ws, toast]);

  const hasNextValues = Object.values(nextValues).some((v) => v !== "");
  const isBridgeMetaLoading = bridgeStatus === null;
  const showBridgeWaitBanner = isBridgeMetaLoading || !state.status.browserConnected || !bridgeStatus?.serialConnected;

  const waitBannerTitle = isBridgeMetaLoading
    ? "Dashboard is initializing"
    : !state.status.browserConnected
      ? "Connecting to local bridge"
      : "Waiting for ESP32 serial link";

  const waitBannerMessage = isBridgeMetaLoading
    ? "Checking bridge health and loading startup metadata. Controls will unlock when initialization finishes."
    : !state.status.browserConnected
      ? "The browser is waiting for a WebSocket session with the local USB bridge. This can take a few seconds after launch."
      : "USB bridge is online but serial is not connected yet. Ensure ESP32 is attached and selected, then controls will become fully usable.";

  useEffect(() => {
    if (!showBridgeWaitBanner) {
      setStartupWaitSeconds(0);
      return;
    }

    const startedAt = Date.now();
    const timerId = setInterval(() => {
      setStartupWaitSeconds(Math.floor((Date.now() - startedAt) / 1000));
    }, 1000);

    return () => clearInterval(timerId);
  }, [showBridgeWaitBanner]);

  return (
    <div className="min-h-screen bg-background">
      <header className="sticky top-0 z-50 border-b bg-background/95 backdrop-blur supports-[backdrop-filter]:bg-background/60">
        <div className="container mx-auto px-4 sm:px-6 lg:px-8 max-w-[96rem]">
          <div className="flex h-16 items-center justify-between gap-4">
            <div className="flex items-center gap-3">
              <div className="p-2 rounded-md bg-primary/10">
                <Cpu className="h-6 w-6 text-primary" data-testid="icon-header-cpu" />
              </div>
              <div>
                <h1 className="text-xl sm:text-2xl font-semibold tracking-tight" data-testid="text-header-title">
                  ESP32 BLE Manager
                </h1>
                <p className="text-xs text-muted-foreground hidden sm:block">
                  Connection Parameter Configuration
                </p>
              </div>
            </div>
            <div className="flex items-center gap-2">
              <ExportButtons state={state} history={history} />
              <ThemeToggle />
            </div>
          </div>
        </div>
      </header>

      <main className="container mx-auto px-4 sm:px-6 lg:px-8 max-w-[96rem] py-8">
        {showBridgeWaitBanner ? (
          <section
            className="mb-4 rounded-lg border border-amber-300 bg-amber-50 p-4 text-amber-900"
            data-testid="startup-wait-banner"
          >
            <div className="flex items-start gap-3">
              <Loader2 className="mt-0.5 h-4 w-4 animate-spin" />
              <div className="space-y-1">
                <h2 className="text-sm font-semibold">{waitBannerTitle}</h2>
                <p className="text-xs leading-relaxed">{waitBannerMessage}</p>
                <p className="text-xs font-medium">Startup wait: {startupWaitSeconds}s</p>
              </div>
            </div>
          </section>
        ) : null}

        <section className="mb-6 rounded-lg border bg-muted/20 p-4" data-testid="usb-bridge-banner">
          <div className="flex flex-col gap-4 lg:flex-row lg:items-end lg:justify-between">
            <div className="space-y-2">
              <div className="flex items-center gap-2">
                <Usb className="h-4 w-4 text-primary" />
                <h2 className="text-sm font-semibold">USB Bridge Status</h2>
                <span
                  className={`inline-flex items-center rounded-full px-2 py-0.5 text-xs font-medium ${bridgeStatus?.serialConnected ? "bg-green-100 text-green-700" : "bg-red-100 text-red-700"}`}
                >
                  {bridgeStatus?.serialConnected ? "Connected" : "Disconnected"}
                </span>
              </div>
              <p className="text-xs text-muted-foreground">
                Port: {bridgeStatus?.selectedPort || "(none)"} | Mode: {bridgeStatus?.autoSelectEnabled ? "Auto Select" : "Manual"} | Baud: {bridgeStatus?.baudRate || 115200}
              </p>
              {bridgeStatus?.lastError ? (
                <p className="text-xs text-red-600">Last error: {bridgeStatus.lastError}</p>
              ) : null}
            </div>

            <div className="flex flex-col gap-2 sm:flex-row sm:items-center">
              <select
                className="h-9 min-w-[220px] rounded-md border bg-background px-2 text-sm"
                value={selectedBridgePort}
                onChange={(e) => setSelectedBridgePort(e.target.value)}
                disabled={Boolean(bridgeStatus?.autoSelectEnabled) || isSwitchingPort}
                data-testid="select-usb-bridge-port"
              >
                <option value="">Select COM Port</option>
                {bridgePorts.map((port) => (
                  <option key={port.path} value={port.path}>
                    {port.path} - {port.manufacturer}{port.suggested ? " (suggested)" : ""}
                  </option>
                ))}
              </select>

              <Button
                variant="outline"
                onClick={() => handlePortModeSwitch(true)}
                disabled={isSwitchingPort || bridgeStatus?.autoSelectEnabled === true}
                data-testid="button-bridge-auto-select"
              >
                Auto Select
              </Button>

              <Button
                variant="outline"
                onClick={() => handlePortModeSwitch(false)}
                disabled={isSwitchingPort || !selectedBridgePort}
                data-testid="button-bridge-manual-select"
              >
                Use Selected Port
              </Button>

              <Button variant="ghost" onClick={loadBridgeMeta} disabled={isSwitchingPort} data-testid="button-bridge-refresh">
                <RefreshCw className="mr-2 h-4 w-4" />
                Refresh
              </Button>
            </div>
          </div>
        </section>

        <div className="grid grid-cols-1 lg:grid-cols-12 gap-8">
          <div className="lg:col-span-8 space-y-8">
            <section>
              <h2 className="text-lg font-semibold mb-4">System Status</h2>
              <StatusPanel status={state.status} />
            </section>

            <section>
              <h2 className="text-lg font-semibold mb-4">Connection Parameters</h2>
              <ConnectionParametersTable
                parameters={state.parameters}
                nextValues={nextValues}
                onNextValueChange={handleNextValueChange}
              />
            </section>

            <section>
              <div className="flex flex-col sm:flex-row gap-4 justify-end">
                <Button
                  variant="outline"
                  onClick={handleClearNext}
                  disabled={!hasNextValues || isSending}
                  data-testid="button-clear-next"
                  className="w-full sm:w-auto"
                >
                  <X className="h-4 w-4 mr-2" />
                  Clear Next
                </Button>
                <Button
                  onClick={handleSendNext}
                  disabled={!hasNextValues || isSending || !state.status.browserConnected}
                  data-testid="button-send-next"
                  className="w-full sm:w-auto"
                >
                  {isSending ? (
                    <Loader2 className="h-4 w-4 mr-2 animate-spin" />
                  ) : (
                    <Send className="h-4 w-4 mr-2" />
                  )}
                  {isSending ? "Sending..." : "Send Next"}
                </Button>
              </div>
            </section>

            <section>
              <h2 className="text-lg font-semibold mb-4">Parameter Trends</h2>
              <ParameterChart history={history} />
            </section>
          </div>

          <div className="lg:col-span-4 space-y-8 min-w-0 lg:sticky lg:top-20 lg:max-h-[calc(100vh-6rem)] lg:overflow-y-auto lg:pr-1">
            <section>
              <BleClientControlPanel ws={ws} status={state.status} devices={discoveredDevices} />
            </section>

            <section>
              <BleServicesPanel
                connected={state.status.isConnected}
                services={discoveredServices}
                inProgress={servicesInProgress}
                error={servicesError}
                negotiatedMtu={negotiatedMtu}
              />
            </section>

            <section>
              <h2 className="text-lg font-semibold mb-4">Bluetooth Security</h2>
              <SecurityPanel ws={ws} />
            </section>

            <section>
              <h2 className="text-lg font-semibold mb-4">Quick Presets</h2>
              <PresetSelector
                presets={presets}
                onApplyPreset={handleApplyPreset}
                disabled={isSending}
              />
            </section>

            <section>
              <h2 className="text-lg font-semibold mb-4">Parameter History</h2>
              <HistoryLog history={history} />
            </section>
          </div>
        </div>
      </main>
    </div>
  );
}
