import { clsx, type ClassValue } from "clsx"
import { twMerge } from "tailwind-merge"

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs))
}

// ESP32 Discovery utility
let discoveredEsp32Url: string | null = null;

export async function discoverEsp32(): Promise<string> {
  // Return cached result if available
  if (discoveredEsp32Url) {
    return discoveredEsp32Url;
  }

  // If explicit URL is set in environment, use it
  const envUrl = import.meta.env.VITE_API_BASE_URL;
  if (envUrl && envUrl.trim()) {
    discoveredEsp32Url = envUrl;
    return envUrl;
  }

  // Get fallback URLs
  const fallbackUrl = import.meta.env.VITE_FALLBACK_API_URL || "http://192.168.4.1";

  // Auto-discovery: Try common ESP32 IP patterns
  const candidateIPs = [
    // Access Point mode
    "192.168.4.1",
    // Common home router ranges for DHCP
    "192.168.1.100", "192.168.1.101", "192.168.1.102", "192.168.1.103", "192.168.1.104",
    "192.168.0.100", "192.168.0.101", "192.168.0.102", "192.168.0.103", "192.168.0.104",
    "10.0.0.100", "10.0.0.101", "10.0.0.102", "10.0.0.103", "10.0.0.104",
  ];

  console.log("🔍 Discovering ESP32...");

  // Test each candidate IP
  for (const ip of candidateIPs) {
    try {
      const testUrl = `http://${ip}`;
      const controller = new AbortController();
      const timeout = setTimeout(() => controller.abort(), 2000); // 2 second timeout

      console.log(`  Testing ${testUrl}...`);
      const response = await fetch(`${testUrl}/api/state`, {
        signal: controller.signal,
        mode: 'cors',
        headers: {
          'Content-Type': 'application/json'
        }
      });

      clearTimeout(timeout);

      if (response.ok) {
        const data = await response.json();
        // Verify this is actually our ESP32 by checking response structure
        if (data && (data.stagedParams || data.appliedParams)) {
          console.log(`✅ Found ESP32 at ${testUrl}`);
          discoveredEsp32Url = testUrl;
          return testUrl;
        }
      }
    } catch (error) {
      // Failed to connect to this IP, try next
      continue;
    }
  }

  console.log(`⚠️  Could not auto-discover ESP32, using fallback: ${fallbackUrl}`);
  discoveredEsp32Url = fallbackUrl;
  return fallbackUrl;
}

export function getWebSocketUrl(httpUrl: string): string {
  const url = new URL(httpUrl);
  const wsUrl = `ws://${url.hostname}:81`;
  
  // Check if there's a custom WebSocket URL in environment
  const envWsUrl = import.meta.env.VITE_WS_URL;
  if (envWsUrl && envWsUrl.trim()) {
    return envWsUrl;
  }
  
  return wsUrl;
}

export function resetEsp32Discovery() {
  discoveredEsp32Url = null;
}
