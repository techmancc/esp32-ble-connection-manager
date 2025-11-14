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

export function exportToCSV(state: DashboardState, history: ParameterHistory[]) {
  const timestamp = format(new Date(), "yyyy-MM-dd HH:mm:ss");
  
  const metadataRows = [
    ["ESP32 BLE Connection Parameters Export"],
    ["Exported At", timestamp],
    [""],
    ["Current System Status"],
    ["Advertising", state.status.isAdvertising ? "Yes" : "No"],
    ["Connected", state.status.isConnected ? "Yes" : "No"],
    ["Connected Device", state.status.connectedDeviceName || "None"],
    [""],
    ["Current Parameters"],
    [
      "Connection Interval Min (ms)",
      "Connection Interval Max (ms)",
      "Peripheral Latency",
      "Supervision Timeout (ms)",
    ],
    [
      state.parameters.current.connectionIntervalMin.toString(),
      state.parameters.current.connectionIntervalMax.toString(),
      state.parameters.current.peripheralLatency.toString(),
      state.parameters.current.supervisionTimeout.toString(),
    ],
  ];

  if (state.parameters.previous) {
    metadataRows.push(
      [""],
      ["Previous Parameters"],
      [
        "Connection Interval Min (ms)",
        "Connection Interval Max (ms)",
        "Peripheral Latency",
        "Supervision Timeout (ms)",
      ],
      [
        state.parameters.previous.connectionIntervalMin.toString(),
        state.parameters.previous.connectionIntervalMax.toString(),
        state.parameters.previous.peripheralLatency.toString(),
        state.parameters.previous.supervisionTimeout.toString(),
      ]
    );
  }

  if (state.parameters.next) {
    metadataRows.push(
      [""],
      ["Next Parameters (Staged)"],
      [
        "Connection Interval Min (ms)",
        "Connection Interval Max (ms)",
        "Peripheral Latency",
        "Supervision Timeout (ms)",
      ],
      [
        state.parameters.next.connectionIntervalMin.toString(),
        state.parameters.next.connectionIntervalMax.toString(),
        state.parameters.next.peripheralLatency.toString(),
        state.parameters.next.supervisionTimeout.toString(),
      ]
    );
  }

  const historyHeaders = [
    "Applied At",
    "Connection Interval Min (ms)",
    "Connection Interval Max (ms)",
    "Peripheral Latency",
    "Supervision Timeout (ms)",
    "Source",
  ];

  const historyRows = history.map((entry) => [
    format(new Date(entry.appliedAt), "yyyy-MM-dd HH:mm:ss"),
    entry.connectionIntervalMin.toString(),
    entry.connectionIntervalMax.toString(),
    entry.peripheralLatency.toString(),
    entry.supervisionTimeout.toString(),
    entry.source,
  ]);

  const allRows = [
    ...metadataRows,
    [""],
    ["Parameter Change History"],
    historyHeaders,
    ...historyRows,
  ];

  const csvContent = allRows
    .map((row) => row.map((cell) => `"${cell}"`).join(","))
    .join("\n");

  const blob = new Blob([csvContent], { type: "text/csv;charset=utf-8;" });
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.href = url;
  link.download = `esp32-parameters-${format(new Date(), "yyyy-MM-dd-HHmmss")}.csv`;
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  URL.revokeObjectURL(url);
}
