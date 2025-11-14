import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Badge } from "@/components/ui/badge";
import { Zap, Battery, Scale, ArrowRight } from "lucide-react";
import type { ParameterPreset } from "@shared/schema";

interface PresetSelectorProps {
  presets: ParameterPreset[];
  onApplyPreset: (preset: ParameterPreset) => void;
  disabled?: boolean;
}

export function PresetSelector({ presets, onApplyPreset, disabled = false }: PresetSelectorProps) {
  const getPresetIcon = (name: string) => {
    const lowerName = name.toLowerCase();
    if (lowerName.includes("low latency")) return Zap;
    if (lowerName.includes("power") || lowerName.includes("saving")) return Battery;
    if (lowerName.includes("balanced")) return Scale;
    return ArrowRight;
  };

  const getPresetColor = (name: string) => {
    const lowerName = name.toLowerCase();
    if (lowerName.includes("low latency")) return "text-status-online";
    if (lowerName.includes("power") || lowerName.includes("saving")) return "text-status-away";
    if (lowerName.includes("balanced")) return "text-primary";
    return "text-foreground";
  };

  if (presets.length === 0) {
    return (
      <Card className="p-8">
        <div className="text-center text-muted-foreground">
          <Scale className="h-12 w-12 mx-auto mb-3 opacity-50" />
          <p className="text-sm">No presets available</p>
        </div>
      </Card>
    );
  }

  return (
    <div className="grid grid-cols-1 gap-3">
      {presets.map((preset) => {
        const Icon = getPresetIcon(preset.name);
        const iconColor = getPresetColor(preset.name);

        return (
          <Card
            key={preset.id}
            className="p-4 hover-elevate cursor-pointer transition-all"
            data-testid={`preset-card-${preset.id}`}
          >
            <div className="flex items-start gap-4">
              <div className={`p-3 rounded-md bg-primary/10 ${iconColor}`}>
                <Icon className="h-5 w-5" />
              </div>
              <div className="flex-1 min-w-0">
                <div className="flex items-start justify-between gap-2 mb-2">
                  <div>
                    <h3 className="font-semibold text-sm" data-testid={`preset-${preset.id}-name`}>
                      {preset.name}
                    </h3>
                    {preset.description && (
                      <p className="text-xs text-muted-foreground mt-1">
                        {preset.description}
                      </p>
                    )}
                  </div>
                </div>

                <div className="grid grid-cols-2 gap-2 text-xs mb-3">
                  <div>
                    <span className="text-muted-foreground">Interval:</span>
                    <span className="ml-1 font-mono">
                      {preset.connectionIntervalMin.toFixed(1)}-{preset.connectionIntervalMax.toFixed(1)}ms
                    </span>
                  </div>
                  <div>
                    <span className="text-muted-foreground">Latency:</span>
                    <span className="ml-1 font-mono">{preset.peripheralLatency}</span>
                  </div>
                  <div className="col-span-2">
                    <span className="text-muted-foreground">Timeout:</span>
                    <span className="ml-1 font-mono">{preset.supervisionTimeout.toFixed(0)}ms</span>
                  </div>
                </div>

                <Button
                  size="sm"
                  variant="secondary"
                  className="w-full"
                  onClick={() => onApplyPreset(preset)}
                  disabled={disabled}
                  data-testid={`button-apply-preset-${preset.id}`}
                >
                  <ArrowRight className="h-3 w-3 mr-1" />
                  Apply Preset
                </Button>
              </div>
            </div>
          </Card>
        );
      })}
    </div>
  );
}
