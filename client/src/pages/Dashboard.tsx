import { useState, useEffect, useCallback, useRef } from "react";
import { Button } from "@/components/ui/button";
import { StatusPanel } from "@/components/StatusPanel";
import { ConnectionParametersTable } from "@/components/ConnectionParametersTable";
import { HistoryLog } from "@/components/HistoryLog";
import { PresetSelector } from "@/components/PresetSelector";
import { ParameterChart } from "@/components/ParameterChart";
import { ExportButtons } from "@/components/ExportButtons";
import { ThemeToggle } from "@/components/ThemeToggle";
import { useToast } from "@/hooks/use-toast";
import { Cpu, Send, X, Loader2 } from "lucide-react";
import { useQuery } from "@tanstack/react-query";
import type { DashboardState, UpdateParameters, ParameterHistory, ParameterPreset } from "@shared/schema";

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
  const previousNextRef = useRef<typeof state.parameters.next>(null);

  const { data: presets = [] } = useQuery<ParameterPreset[]>({
    queryKey: ["/api/presets"],
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

  useEffect(() => {
    const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
    const wsUrl = `${protocol}//${window.location.host}/ws`;
    const socket = new WebSocket(wsUrl);

    socket.onopen = () => {
      console.log("WebSocket connected");
    };

    socket.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        if (data.type === "state_update") {
          setState(data.state);
        } else if (data.type === "history_update") {
          setHistory(data.history);
        } else if (data.type === "parameter_update_success") {
          toast({
            title: "Parameters Staged",
            description: "Connection parameters have been staged and will be applied by ESP32 in a few seconds.",
          });
          setIsSending(false);
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
      toast({
        title: "Connection Error",
        description: "Failed to connect to ESP32 server.",
        variant: "destructive",
      });
    };

    socket.onclose = () => {
      console.log("WebSocket disconnected");
    };

    setWs(socket);

    return () => {
      socket.close();
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
    toast({
      title: "Cleared",
      description: "Next parameter values have been cleared.",
    });
  }, [toast]);

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

  return (
    <div className="min-h-screen bg-background">
      <header className="sticky top-0 z-50 border-b bg-background/95 backdrop-blur supports-[backdrop-filter]:bg-background/60">
        <div className="container mx-auto px-4 sm:px-6 lg:px-8 max-w-7xl">
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

      <main className="container mx-auto px-4 sm:px-6 lg:px-8 max-w-7xl py-8">
        <div className="grid grid-cols-1 lg:grid-cols-4 gap-8">
          <div className="lg:col-span-3 space-y-8">
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

          <div className="lg:col-span-1 space-y-8">
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
