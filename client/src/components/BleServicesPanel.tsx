import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { ScrollArea } from "@/components/ui/scroll-area";
import { Bluetooth, Loader2 } from "lucide-react";
import type { BleServiceSummary } from "@shared/schema";

interface BleServicesPanelProps {
  connected: boolean;
  services: BleServiceSummary[];
  inProgress: boolean;
  error: string | null;
  negotiatedMtu: number | null;
}

export function BleServicesPanel({
  connected,
  services,
  inProgress,
  error,
  negotiatedMtu,
}: BleServicesPanelProps) {
  return (
    <Card>
      <CardHeader className="pb-3">
        <CardTitle className="flex items-center gap-2 text-base">
          <Bluetooth className="h-5 w-5" />
          GATT Services
        </CardTitle>
      </CardHeader>
      <CardContent className="space-y-3">
        <div className="flex items-center justify-between gap-2">
          <Badge variant={connected ? "default" : "secondary"}>
            {connected ? "Connected" : "Disconnected"}
          </Badge>
          <div className="flex items-center gap-2">
            {inProgress ? (
              <Badge variant="outline" className="flex items-center gap-1">
                <Loader2 className="h-3 w-3 animate-spin" />
                Discovering
              </Badge>
            ) : null}
            <Badge variant="outline">Services: {services.length}</Badge>
            <Badge variant="outline">MTU: {negotiatedMtu ?? "-"}</Badge>
          </div>
        </div>

        {error ? (
          <div className="rounded-md border border-destructive/40 bg-destructive/10 p-2 text-xs text-destructive">
            {error}
          </div>
        ) : null}

        {!connected ? (
          <p className="text-xs text-muted-foreground">
            Connect to a peripheral to discover and list GATT services.
          </p>
        ) : services.length === 0 && !inProgress ? (
          <p className="text-xs text-muted-foreground">
            No services discovered yet.
          </p>
        ) : (
          <ScrollArea className="h-80 pr-2">
            <div className="space-y-2">
              {services.map((service) => (
                <div key={service.uuid} className="rounded-md border p-2 space-y-2">
                  <div className="text-xs font-medium break-all">Service {service.uuid}</div>
                  <div className="text-xs text-muted-foreground">
                    Characteristics: {service.characteristicCount}
                  </div>
                  <div className="space-y-1">
                    {service.characteristics.map((characteristic) => {
                      const props = [
                        characteristic.canRead ? "read" : null,
                        characteristic.canWrite ? "write" : null,
                        characteristic.canNotify ? "notify" : null,
                        characteristic.canIndicate ? "indicate" : null,
                        characteristic.canWriteNoResponse ? "writeNoRsp" : null,
                      ].filter(Boolean);

                      return (
                        <div key={characteristic.uuid} className="rounded border bg-muted/20 p-2">
                          <div className="text-xs break-all">{characteristic.uuid}</div>
                          <div className="text-[11px] text-muted-foreground">
                            {props.length > 0 ? props.join(" | ") : "no properties"}
                          </div>
                        </div>
                      );
                    })}
                  </div>
                </div>
              ))}
            </div>
          </ScrollArea>
        )}
      </CardContent>
    </Card>
  );
}
