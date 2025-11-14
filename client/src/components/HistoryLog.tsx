import { Card } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { ScrollArea } from "@/components/ui/scroll-area";
import { Clock, Cpu, User } from "lucide-react";
import type { ParameterHistory } from "@shared/schema";
import { formatDistanceToNow } from "date-fns";

interface HistoryLogProps {
  history: ParameterHistory[];
}

export function HistoryLog({ history }: HistoryLogProps) {
  const formatValue = (value: number, isLatency: boolean = false) => {
    return isLatency ? value.toString() : `${value.toFixed(2)} ms`;
  };

  const getSourceIcon = (source: string) => {
    switch (source) {
      case "esp32_auto":
        return <Cpu className="h-3 w-3" />;
      case "manual":
        return <User className="h-3 w-3" />;
      default:
        return <Clock className="h-3 w-3" />;
    }
  };

  const getSourceLabel = (source: string) => {
    switch (source) {
      case "esp32_auto":
        return "ESP32 Auto-Applied";
      case "manual":
        return "Manual Update";
      default:
        return source;
    }
  };

  if (history.length === 0) {
    return (
      <Card className="p-8">
        <div className="text-center text-muted-foreground">
          <Clock className="h-12 w-12 mx-auto mb-3 opacity-50" />
          <p className="text-sm">No parameter history yet</p>
          <p className="text-xs mt-1">Changes will appear here once parameters are applied</p>
        </div>
      </Card>
    );
  }

  return (
    <Card className="overflow-hidden">
      <ScrollArea className="h-[400px]">
        <div className="p-4 space-y-3">
          {history.map((entry) => (
            <div
              key={entry.id}
              className="p-4 rounded-md border bg-card hover-elevate"
              data-testid={`history-entry-${entry.id}`}
            >
              <div className="flex items-start justify-between gap-4 mb-3">
                <div className="flex items-center gap-2">
                  <Badge variant="secondary" className="gap-1">
                    {getSourceIcon(entry.source)}
                    {getSourceLabel(entry.source)}
                  </Badge>
                </div>
                <div className="text-xs text-muted-foreground flex items-center gap-1">
                  <Clock className="h-3 w-3" />
                  {formatDistanceToNow(new Date(entry.appliedAt), { addSuffix: true })}
                </div>
              </div>

              <div className="grid grid-cols-2 md:grid-cols-4 gap-3 text-sm">
                <div>
                  <div className="text-xs text-muted-foreground mb-1">Interval Min</div>
                  <div className="font-mono font-medium" data-testid={`history-${entry.id}-interval-min`}>
                    {formatValue(entry.connectionIntervalMin)}
                  </div>
                </div>
                <div>
                  <div className="text-xs text-muted-foreground mb-1">Interval Max</div>
                  <div className="font-mono font-medium" data-testid={`history-${entry.id}-interval-max`}>
                    {formatValue(entry.connectionIntervalMax)}
                  </div>
                </div>
                <div>
                  <div className="text-xs text-muted-foreground mb-1">Latency</div>
                  <div className="font-mono font-medium" data-testid={`history-${entry.id}-latency`}>
                    {formatValue(entry.peripheralLatency, true)}
                  </div>
                </div>
                <div>
                  <div className="text-xs text-muted-foreground mb-1">Timeout</div>
                  <div className="font-mono font-medium" data-testid={`history-${entry.id}-timeout`}>
                    {formatValue(entry.supervisionTimeout)}
                  </div>
                </div>
              </div>
            </div>
          ))}
        </div>
      </ScrollArea>
    </Card>
  );
}
