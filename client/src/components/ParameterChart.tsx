import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { useState } from "react";
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer } from "recharts";
import { Clock } from "lucide-react";
import type { ParameterHistory } from "@shared/schema";
import { format, subHours, subDays, isAfter } from "date-fns";

interface ParameterChartProps {
  history: ParameterHistory[];
}

type TimeRange = "1h" | "24h" | "7d" | "all";

export function ParameterChart({ history }: ParameterChartProps) {
  const [timeRange, setTimeRange] = useState<TimeRange>("24h");

  const filterHistoryByTimeRange = (data: ParameterHistory[], range: TimeRange) => {
    const now = new Date();
    let cutoffTime: Date;

    switch (range) {
      case "1h":
        cutoffTime = subHours(now, 1);
        break;
      case "24h":
        cutoffTime = subHours(now, 24);
        break;
      case "7d":
        cutoffTime = subDays(now, 7);
        break;
      case "all":
        return data;
    }

    return data.filter((entry) => isAfter(new Date(entry.appliedAt), cutoffTime));
  };

  const filteredHistory = filterHistoryByTimeRange(history, timeRange);

  const chartData = filteredHistory
    .slice()
    .reverse()
    .map((entry) => ({
      time: format(new Date(entry.appliedAt), "HH:mm:ss"),
      fullTime: format(new Date(entry.appliedAt), "MMM d, HH:mm:ss"),
      intervalMin: entry.connectionIntervalMin,
      intervalMax: entry.connectionIntervalMax,
      latency: entry.peripheralLatency,
      timeout: entry.supervisionTimeout,
    }));

  if (history.length === 0) {
    return (
      <Card className="p-8">
        <div className="text-center text-muted-foreground">
          <Clock className="h-12 w-12 mx-auto mb-3 opacity-50" />
          <p className="text-sm">No data to visualize yet</p>
          <p className="text-xs mt-1">Parameter changes will be shown here</p>
        </div>
      </Card>
    );
  }

  return (
    <Card className="p-6">
      <div className="flex items-center justify-between mb-6">
        <h3 className="text-base font-semibold">Parameter Trends</h3>
        <div className="flex gap-2">
          <Button
            variant={timeRange === "1h" ? "default" : "outline"}
            size="sm"
            onClick={() => setTimeRange("1h")}
            data-testid="button-range-1h"
          >
            1H
          </Button>
          <Button
            variant={timeRange === "24h" ? "default" : "outline"}
            size="sm"
            onClick={() => setTimeRange("24h")}
            data-testid="button-range-24h"
          >
            24H
          </Button>
          <Button
            variant={timeRange === "7d" ? "default" : "outline"}
            size="sm"
            onClick={() => setTimeRange("7d")}
            data-testid="button-range-7d"
          >
            7D
          </Button>
          <Button
            variant={timeRange === "all" ? "default" : "outline"}
            size="sm"
            onClick={() => setTimeRange("all")}
            data-testid="button-range-all"
          >
            All
          </Button>
        </div>
      </div>

      {chartData.length === 0 ? (
        <div className="h-[300px] flex items-center justify-center text-muted-foreground">
          <p className="text-sm">No data in this time range</p>
        </div>
      ) : (
        <ResponsiveContainer width="100%" height={300}>
          <LineChart data={chartData}>
            <CartesianGrid strokeDasharray="3 3" className="stroke-border" />
            <XAxis
              dataKey="time"
              className="text-xs"
              tick={{ fill: "hsl(var(--muted-foreground))" }}
            />
            <YAxis
              yAxisId="left"
              className="text-xs"
              tick={{ fill: "hsl(var(--muted-foreground))" }}
              domain={[0, 'auto']}
              label={{ value: 'Time (ms)', angle: -90, position: 'insideLeft', style: { fill: 'hsl(var(--muted-foreground))', fontSize: 12 } }}
            />
            <YAxis
              yAxisId="right"
              orientation="right"
              className="text-xs"
              tick={{ fill: "hsl(var(--muted-foreground))" }}
              domain={[0, 'auto']}
              label={{ value: 'Latency (count)', angle: 90, position: 'insideRight', style: { fill: 'hsl(var(--muted-foreground))', fontSize: 12 } }}
            />
            <Tooltip
              contentStyle={{
                backgroundColor: "hsl(var(--popover))",
                border: "1px solid hsl(var(--border))",
                borderRadius: "0.375rem",
              }}
              labelFormatter={(value) => {
                const entry = chartData.find((d) => d.time === value);
                return entry ? entry.fullTime : value;
              }}
              formatter={(value: number, name: string) => {
                if (name === "timeout") {
                  return [`${value.toFixed(0)} ms`, "Timeout"];
                }
                if (name === "latency") {
                  return [value.toString(), "Latency"];
                }
                return [`${value.toFixed(2)} ms`, name === "intervalMin" ? "Interval Min" : "Interval Max"];
              }}
            />
            <Legend />
            <Line
              yAxisId="left"
              type="monotone"
              dataKey="intervalMin"
              stroke="hsl(var(--chart-1))"
              strokeWidth={2}
              name="Interval Min"
              dot={{ fill: "hsl(var(--chart-1))", r: 3 }}
            />
            <Line
              yAxisId="left"
              type="monotone"
              dataKey="intervalMax"
              stroke="hsl(var(--chart-2))"
              strokeWidth={2}
              name="Interval Max"
              dot={{ fill: "hsl(var(--chart-2))", r: 3 }}
            />
            <Line
              yAxisId="right"
              type="monotone"
              dataKey="latency"
              stroke="hsl(var(--chart-3))"
              strokeWidth={2}
              name="Latency"
              dot={{ fill: "hsl(var(--chart-3))", r: 3 }}
            />
            <Line
              yAxisId="left"
              type="monotone"
              dataKey="timeout"
              stroke="hsl(var(--chart-4))"
              strokeWidth={2}
              name="Timeout"
              dot={{ fill: "hsl(var(--chart-4))", r: 3 }}
            />
          </LineChart>
        </ResponsiveContainer>
      )}
    </Card>
  );
}
