import { Card } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { Cpu, Radio, Smartphone, Wifi } from "lucide-react";
import type { ESP32Status } from "@shared/schema";

interface StatusPanelProps {
  status: ESP32Status;
}

export function StatusPanel({ status }: StatusPanelProps) {
  return (
    <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
      <Card className="p-6">
        <div className="flex items-start gap-4">
          <div className="p-3 rounded-md bg-primary/10">
            <Radio className="h-5 w-5 text-primary" data-testid="icon-ble-advertising" />
          </div>
          <div className="flex-1 min-w-0">
            <p className="text-sm font-medium text-muted-foreground mb-1">
              BLE Advertising
            </p>
            <div className="flex items-center gap-2">
              <Badge
                variant={status.isAdvertising ? "default" : "secondary"}
                className={status.isAdvertising ? "pulse-subtle" : ""}
                data-testid="badge-advertising-status"
              >
                {status.isAdvertising ? "Active" : "Inactive"}
              </Badge>
            </div>
          </div>
        </div>
      </Card>

      <Card className="p-6">
        <div className="flex items-start gap-4">
          <div className="p-3 rounded-md bg-primary/10">
            <Smartphone className="h-5 w-5 text-primary" data-testid="icon-ble-connected" />
          </div>
          <div className="flex-1 min-w-0">
            <p className="text-sm font-medium text-muted-foreground mb-1">
              BLE Connection
            </p>
            <div className="flex items-center gap-2">
              <Badge
                variant={status.isConnected ? "default" : "secondary"}
                className={status.isConnected ? "pulse-subtle" : ""}
                data-testid="badge-connection-status"
              >
                {status.isConnected ? "Connected" : "Disconnected"}
              </Badge>
            </div>
          </div>
        </div>
      </Card>

      <Card className="p-6">
        <div className="flex items-start gap-4">
          <div className="p-3 rounded-md bg-primary/10">
            <Cpu className="h-5 w-5 text-primary" data-testid="icon-device" />
          </div>
          <div className="flex-1 min-w-0">
            <p className="text-sm font-medium text-muted-foreground mb-1">
              Connected Device
            </p>
            <p className="text-base font-mono truncate" data-testid="text-device-name">
              {status.connectedDeviceName || "None"}
            </p>
          </div>
        </div>
      </Card>

      <Card className="p-6">
        <div className="flex items-start gap-4">
          <div className="p-3 rounded-md bg-primary/10">
            <Wifi className="h-5 w-5 text-primary" data-testid="icon-browser-connection" />
          </div>
          <div className="flex-1 min-w-0">
            <p className="text-sm font-medium text-muted-foreground mb-1">
              Browser Connection
            </p>
            <div className="flex items-center gap-2">
              <Badge
                variant={status.browserConnected ? "default" : "destructive"}
                className={status.browserConnected ? "pulse-subtle" : ""}
                data-testid="badge-browser-status"
              >
                {status.browserConnected ? "Connected" : "Disconnected"}
              </Badge>
            </div>
          </div>
        </div>
      </Card>
    </div>
  );
}
