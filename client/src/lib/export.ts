import type { DashboardState, ParameterHistory } from "@shared/schema";
import { format } from "date-fns";

export function exportToJSON(state: DashboardState, history: ParameterHistory[]) {
  const exportData = {
    exportedAt: new Date().toISOString(),
    currentParameters: state.parameters.current,
    previousParameters: state.parameters.previous,
    nextParameters: state.parameters.next,
    status: state.status,
    history: history.map((entry) => ({
      ...entry,
      appliedAt: new Date(entry.appliedAt).toISOString(),
    })),
  };

  const blob = new Blob([JSON.stringify(exportData, null, 2)], {
    type: "application/json",
  });
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.href = url;
  link.download = `esp32-parameters-${format(new Date(), "yyyy-MM-dd-HHmmss")}.json`;
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  URL.revokeObjectURL(url);
}

export function exportToCSV(history: ParameterHistory[]) {
  const headers = [
    "Applied At",
    "Connection Interval Min (ms)",
    "Connection Interval Max (ms)",
    "Peripheral Latency",
    "Supervision Timeout (ms)",
    "Source",
  ];

  const rows = history.map((entry) => [
    format(new Date(entry.appliedAt), "yyyy-MM-dd HH:mm:ss"),
    entry.connectionIntervalMin.toString(),
    entry.connectionIntervalMax.toString(),
    entry.peripheralLatency.toString(),
    entry.supervisionTimeout.toString(),
    entry.source,
  ]);

  const csvContent = [headers, ...rows]
    .map((row) => row.map((cell) => `"${cell}"`).join(","))
    .join("\n");

  const blob = new Blob([csvContent], { type: "text/csv;charset=utf-8;" });
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.href = url;
  link.download = `esp32-history-${format(new Date(), "yyyy-MM-dd-HHmmss")}.csv`;
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  URL.revokeObjectURL(url);
}
