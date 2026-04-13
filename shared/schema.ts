import { z } from "zod";
import { pgTable, serial, varchar, timestamp, real, integer } from "drizzle-orm/pg-core";
import { createInsertSchema } from "drizzle-zod";
import { sql } from "drizzle-orm";

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
  scanFilterEnabled?: boolean;
  scanFilterName?: string | null;
  isScanning?: boolean;
}

export interface BleScanDevice {
  address: string;
  name: string | null;
  rssi: number;
  connectable: boolean;
  lastSeenMs: number;
}

export interface BleCharacteristicSummary {
  uuid: string;
  canRead: boolean;
  canWrite: boolean;
  canNotify: boolean;
  canIndicate: boolean;
  canWriteNoResponse: boolean;
}

export interface BleServiceSummary {
  uuid: string;
  characteristicCount: number;
  characteristics: BleCharacteristicSummary[];
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

export const parameterHistory = pgTable("parameter_history", {
  id: serial("id").primaryKey(),
  connectionIntervalMin: real("connection_interval_min").notNull(),
  connectionIntervalMax: real("connection_interval_max").notNull(),
  peripheralLatency: integer("peripheral_latency").notNull(),
  supervisionTimeout: real("supervision_timeout").notNull(),
  appliedAt: timestamp("applied_at").notNull().default(sql`now()`),
  source: varchar("source", { length: 50 }).notNull().default("manual"),
});

export const insertParameterHistorySchema = createInsertSchema(parameterHistory).omit({
  id: true,
  appliedAt: true,
});

export type ParameterHistory = typeof parameterHistory.$inferSelect;
export type InsertParameterHistory = z.infer<typeof insertParameterHistorySchema>;

export const parameterPresets = pgTable("parameter_presets", {
  id: serial("id").primaryKey(),
  name: varchar("name", { length: 100 }).notNull().unique(),
  description: varchar("description", { length: 255 }),
  connectionIntervalMin: real("connection_interval_min").notNull(),
  connectionIntervalMax: real("connection_interval_max").notNull(),
  peripheralLatency: integer("peripheral_latency").notNull(),
  supervisionTimeout: real("supervision_timeout").notNull(),
  createdAt: timestamp("created_at").notNull().default(sql`now()`),
});

export const insertParameterPresetSchema = createInsertSchema(parameterPresets).omit({
  id: true,
  createdAt: true,
});

export type ParameterPreset = typeof parameterPresets.$inferSelect;
export type InsertParameterPreset = z.infer<typeof insertParameterPresetSchema>;
