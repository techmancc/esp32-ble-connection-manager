import { z } from "zod";

export const connectionParametersSchema = z.object({
  connectionIntervalMin: z.number().min(7.5).max(4000),
  connectionIntervalMax: z.number().min(7.5).max(4000),
  peripheralLatency: z.number().min(0).max(499).int(),
  supervisionTimeout: z.number().min(100).max(32000),
});

export type ConnectionParameters = z.infer<typeof connectionParametersSchema>;

export interface ConnectionParameterSet {
  previous: ConnectionParameters | null;
  current: ConnectionParameters;
  next: ConnectionParameters | null;
}

export interface ESP32Status {
  isAdvertising: boolean;
  isConnected: boolean;
  connectedDeviceName: string | null;
  browserConnected: boolean;
}

export interface DashboardState {
  parameters: ConnectionParameterSet;
  status: ESP32Status;
}

export const updateParametersSchema = z.object({
  connectionIntervalMin: z.number().min(7.5).max(4000),
  connectionIntervalMax: z.number().min(7.5).max(4000),
  peripheralLatency: z.number().min(0).max(499).int(),
  supervisionTimeout: z.number().min(100).max(32000),
});

export type UpdateParameters = z.infer<typeof updateParametersSchema>;
