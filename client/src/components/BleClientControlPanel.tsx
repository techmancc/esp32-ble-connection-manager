import { useEffect, useState } from "react";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Badge } from "@/components/ui/badge";
import { ScrollArea } from "@/components/ui/scroll-area";
import { useToast } from "@/hooks/use-toast";
import type { BleScanDevice, ESP32Status } from "@shared/schema";
import { BluetoothSearching, Link2Off, Play, ScanLine, Wifi, XCircle } from "lucide-react";

interface BleClientControlPanelProps {
  ws: WebSocket | null;
  status: ESP32Status;
  devices: BleScanDevice[];
}

export function BleClientControlPanel({ ws, status, devices }: BleClientControlPanelProps) {
  const { toast } = useToast();
  const [scanFilterName, setScanFilterName] = useState("");
  const [expandedScanList, setExpandedScanList] = useState(false);

  useEffect(() => {
    setScanFilterName(status.scanFilterName || "");
  }, [status.scanFilterName]);

  const sendBleCommand = (payload: Record<string, unknown>) => {
    if (!ws || ws.readyState !== WebSocket.OPEN) {
      toast({
        title: "Connection Error",
        description: "WebSocket is not connected to ESP32.",
        variant: "destructive",
      });
      return false;
    }

    ws.send(JSON.stringify({ type: "ble_command", ...payload }));
    return true;
  };

  const handleApplyFilterAndScan = () => {
    if (!scanFilterName.trim()) {
      toast({
        title: "Filter Required",
        description: "Enter BLE device-name text to include before scanning.",
        variant: "destructive",
      });
      return;
    }

    if (
      sendBleCommand({
        command: "set_scan_filter",
        deviceName: scanFilterName.trim(),
        restartScan: true,
      })
    ) {
      toast({
        title: "Include Filter Applied",
        description: `Scanning only for peripherals whose name contains \"${scanFilterName.trim()}\"...`,
      });
    }
  };

  const handleStartScan = () => {
    if (sendBleCommand({ command: "start_scan" })) {
      toast({
        title: "Scan Requested",
        description: "BLE scan command sent to ESP32.",
      });
    }
  };

  const handleClearFilter = () => {
    if (sendBleCommand({ command: "clear_scan_filter" })) {
      setScanFilterName("");
      toast({
        title: "Filter Cleared",
        description: "Peripheral device-name filter was removed.",
      });
    }
  };

  const handleDisconnect = () => {
    if (sendBleCommand({ command: "disconnect" })) {
      toast({
        title: "Disconnect Requested",
        description: "Sent BLE disconnect command to ESP32.",
      });
    }
  };

  const handleConnect = (device: BleScanDevice) => {
    if (
      sendBleCommand({
        command: "connect_device",
        address: device.address,
      })
    ) {
      toast({
        title: "Connect Requested",
        description: `ESP32 is attempting to connect to ${device.name || device.address}.`,
      });
    }
  };

  return (
    <Card>
      <CardHeader className="pb-3">
        <CardTitle className="flex items-center gap-2 text-base">
          <ScanLine className="h-5 w-5" />
          BLE Client Control
        </CardTitle>
      </CardHeader>
      <CardContent className="space-y-4">
        <div className="space-y-2">
          <label className="text-sm font-medium">Peripheral Name Include Filter</label>
          <Input
            placeholder="e.g. CGM_Transmitter"
            value={scanFilterName}
            onChange={(e) => setScanFilterName(e.target.value)}
            data-testid="input-ble-filter-name"
          />
          <p className="text-xs text-muted-foreground">
            Only devices whose advertised name contains this text will be eligible for connection.
          </p>
        </div>

        <div className="grid grid-cols-1 gap-2">
          <Button onClick={handleApplyFilterAndScan} data-testid="button-ble-apply-filter-scan">
            <BluetoothSearching className="h-4 w-4 mr-2" />
            Apply Filter + Scan
          </Button>
          <Button variant="secondary" onClick={handleStartScan} data-testid="button-ble-start-scan">
            <Play className="h-4 w-4 mr-2" />
            Start Scan
          </Button>
          <Button variant="outline" onClick={handleClearFilter} data-testid="button-ble-clear-filter">
            <XCircle className="h-4 w-4 mr-2" />
            Clear Filter
          </Button>
          <Button
            variant="destructive"
            onClick={handleDisconnect}
            disabled={!status.isConnected}
            data-testid="button-ble-disconnect"
          >
            <Link2Off className="h-4 w-4 mr-2" />
            Disconnect
          </Button>
        </div>

        <div className="space-y-2 rounded-md border p-3">
          <div className="flex items-center justify-between">
            <span className="text-sm">Scan State</span>
            <Badge variant={status.isScanning ? "default" : "secondary"}>
              {status.isScanning ? "Scanning" : "Idle"}
            </Badge>
          </div>
          <div className="flex items-center justify-between">
            <span className="text-sm">Filter</span>
            <Badge variant={status.scanFilterEnabled ? "default" : "outline"}>
              {status.scanFilterEnabled ? "Enabled" : "Disabled"}
            </Badge>
          </div>
          <div className="text-xs text-muted-foreground break-all">
            Active filter: {status.scanFilterName || "(none)"}
          </div>
        </div>

        <div className="space-y-3 rounded-md border p-3">
          <div className="flex items-center justify-between">
            <span className="text-sm font-medium">Discovered Peripherals</span>
            <div className="flex items-center gap-2">
              <Badge variant="outline">{devices.length}</Badge>
              <Button
                type="button"
                variant="ghost"
                size="sm"
                className="h-7 px-2"
                onClick={() => setExpandedScanList((prev) => !prev)}
                data-testid="button-ble-toggle-scan-list-height"
              >
                {expandedScanList ? "Compact" : "Expand"}
              </Button>
            </div>
          </div>

          {devices.length === 0 ? (
            <p className="text-xs text-muted-foreground">
              No scan results yet. Start a scan to populate the list.
            </p>
          ) : (
            <ScrollArea className={`${expandedScanList ? "h-[34rem]" : "h-72"} pr-2`}>
              <div className="space-y-2">
                {devices.map((device) => (
                  <div key={device.address} className="rounded-md border p-3 space-y-2">
                    <div className="flex flex-col gap-2">
                      <div className="min-w-0">
                        <div className="text-sm font-medium truncate">{device.name || "(unnamed)"}</div>
                        <div className="text-xs text-muted-foreground break-all">{device.address}</div>
                      </div>
                      <div className="flex items-center justify-between gap-2 flex-wrap">
                        <div className="flex flex-wrap items-center gap-2">
                          <Badge variant={device.connectable ? "default" : "secondary"}>
                            {device.connectable ? "Connectable" : "Observer"}
                          </Badge>
                          <Badge variant="outline">{device.rssi} dBm</Badge>
                        </div>
                        <Button
                          size="sm"
                          className="h-7 px-2 text-xs"
                          variant={device.connectable ? "secondary" : "outline"}
                          disabled={ws?.readyState !== WebSocket.OPEN}
                          onClick={() => handleConnect(device)}
                          data-testid={`button-ble-connect-${device.address}`}
                        >
                          <Wifi className="h-3.5 w-3.5 mr-1" />
                          {device.connectable ? "Connect" : "Try Connect"}
                        </Button>
                      </div>
                    </div>
                  </div>
                ))}
              </div>
            </ScrollArea>
          )}
        </div>
      </CardContent>
    </Card>
  );
}
