import { db } from "../db";
import { parameterPresets } from "@shared/schema";

async function seed() {
  console.log("Seeding database with default presets...");

  const defaultPresets = [
    {
      name: "Low Latency",
      description: "Optimized for minimal latency and fast response times",
      connectionIntervalMin: 7.5,
      connectionIntervalMax: 15,
      peripheralLatency: 0,
      supervisionTimeout: 2000,
    },
    {
      name: "Power Saving",
      description: "Maximizes battery life with longer intervals",
      connectionIntervalMin: 1000,
      connectionIntervalMax: 2000,
      peripheralLatency: 4,
      supervisionTimeout: 6000,
    },
    {
      name: "Balanced",
      description: "Balanced performance and power consumption",
      connectionIntervalMin: 100,
      connectionIntervalMax: 200,
      peripheralLatency: 2,
      supervisionTimeout: 4000,
    },
  ];

  try {
    for (const preset of defaultPresets) {
      await db
        .insert(parameterPresets)
        .values(preset)
        .onConflictDoNothing();
    }
    
    console.log("✓ Seeding completed successfully");
  } catch (error) {
    console.error("Error seeding database:", error);
    throw error;
  }
}

seed()
  .then(() => process.exit(0))
  .catch((error) => {
    console.error(error);
    process.exit(1);
  });
