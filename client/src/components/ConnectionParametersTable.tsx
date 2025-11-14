import { Card } from "@/components/ui/card";
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from "@/components/ui/table";
import { ParameterInput } from "./ParameterInput";
import type { ConnectionParameterSet } from "@shared/schema";

interface ConnectionParametersTableProps {
  parameters: ConnectionParameterSet;
  nextValues: {
    connectionIntervalMin: string;
    connectionIntervalMax: string;
    peripheralLatency: string;
    supervisionTimeout: string;
  };
  onNextValueChange: (field: string, value: string) => void;
}

export function ConnectionParametersTable({
  parameters,
  nextValues,
  onNextValueChange,
}: ConnectionParametersTableProps) {
  const formatValue = (value: number | null | undefined, isLatency: boolean = false) => {
    if (value === null || value === undefined) return "—";
    return isLatency ? value.toString() : `${value.toFixed(2)}`;
  };

  return (
    <Card className="overflow-hidden">
      <div className="overflow-x-auto">
        <Table>
          <TableHeader>
            <TableRow>
              <TableHead className="font-semibold w-[200px] md:w-[250px]">Parameter</TableHead>
              <TableHead className="font-semibold text-center">Previous</TableHead>
              <TableHead className="font-semibold text-center">Current</TableHead>
              <TableHead className="font-semibold text-center min-w-[200px]">Next</TableHead>
            </TableRow>
          </TableHeader>
          <TableBody>
            <TableRow>
              <TableCell className="font-medium">
                <div>
                  <div className="text-sm">Connection Interval Min</div>
                  <div className="text-xs text-muted-foreground">milliseconds</div>
                </div>
              </TableCell>
              <TableCell className="text-center">
                <span className="font-mono text-sm text-muted-foreground" data-testid="text-previous-interval-min">
                  {formatValue(parameters.previous?.connectionIntervalMin)}
                </span>
              </TableCell>
              <TableCell className="text-center">
                <span className="font-mono text-sm font-medium" data-testid="text-current-interval-min">
                  {formatValue(parameters.current.connectionIntervalMin)}
                </span>
              </TableCell>
              <TableCell>
                <ParameterInput
                  label=""
                  value={nextValues.connectionIntervalMin}
                  onChange={(v) => onNextValueChange("connectionIntervalMin", v)}
                  min={7.5}
                  max={4000}
                  step={1.25}
                  unit="ms"
                  hint="Range: 7.5 - 4000 ms"
                  testId="input-next-interval-min"
                />
              </TableCell>
            </TableRow>

            <TableRow>
              <TableCell className="font-medium">
                <div>
                  <div className="text-sm">Connection Interval Max</div>
                  <div className="text-xs text-muted-foreground">milliseconds</div>
                </div>
              </TableCell>
              <TableCell className="text-center">
                <span className="font-mono text-sm text-muted-foreground" data-testid="text-previous-interval-max">
                  {formatValue(parameters.previous?.connectionIntervalMax)}
                </span>
              </TableCell>
              <TableCell className="text-center">
                <span className="font-mono text-sm font-medium" data-testid="text-current-interval-max">
                  {formatValue(parameters.current.connectionIntervalMax)}
                </span>
              </TableCell>
              <TableCell>
                <ParameterInput
                  label=""
                  value={nextValues.connectionIntervalMax}
                  onChange={(v) => onNextValueChange("connectionIntervalMax", v)}
                  min={7.5}
                  max={4000}
                  step={1.25}
                  unit="ms"
                  hint="Range: 7.5 - 4000 ms"
                  testId="input-next-interval-max"
                />
              </TableCell>
            </TableRow>

            <TableRow>
              <TableCell className="font-medium">
                <div>
                  <div className="text-sm">Peripheral Latency</div>
                  <div className="text-xs text-muted-foreground">count</div>
                </div>
              </TableCell>
              <TableCell className="text-center">
                <span className="font-mono text-sm text-muted-foreground" data-testid="text-previous-latency">
                  {formatValue(parameters.previous?.peripheralLatency, true)}
                </span>
              </TableCell>
              <TableCell className="text-center">
                <span className="font-mono text-sm font-medium" data-testid="text-current-latency">
                  {formatValue(parameters.current.peripheralLatency, true)}
                </span>
              </TableCell>
              <TableCell>
                <ParameterInput
                  label=""
                  value={nextValues.peripheralLatency}
                  onChange={(v) => onNextValueChange("peripheralLatency", v)}
                  min={0}
                  max={499}
                  step={1}
                  unit="count"
                  hint="Range: 0 - 499"
                  testId="input-next-latency"
                />
              </TableCell>
            </TableRow>

            <TableRow>
              <TableCell className="font-medium">
                <div>
                  <div className="text-sm">Supervision Timeout</div>
                  <div className="text-xs text-muted-foreground">milliseconds</div>
                </div>
              </TableCell>
              <TableCell className="text-center">
                <span className="font-mono text-sm text-muted-foreground" data-testid="text-previous-timeout">
                  {formatValue(parameters.previous?.supervisionTimeout)}
                </span>
              </TableCell>
              <TableCell className="text-center">
                <span className="font-mono text-sm font-medium" data-testid="text-current-timeout">
                  {formatValue(parameters.current.supervisionTimeout)}
                </span>
              </TableCell>
              <TableCell>
                <ParameterInput
                  label=""
                  value={nextValues.supervisionTimeout}
                  onChange={(v) => onNextValueChange("supervisionTimeout", v)}
                  min={100}
                  max={32000}
                  step={10}
                  unit="ms"
                  hint="Range: 100 - 32000 ms"
                  testId="input-next-timeout"
                />
              </TableCell>
            </TableRow>
          </TableBody>
        </Table>
      </div>
    </Card>
  );
}
