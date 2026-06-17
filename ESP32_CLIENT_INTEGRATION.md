# ESP32 Client Integration Guide

Canonical setup and run commands are maintained in `README.md`. Use that file first, then use this document for API and architecture compatibility details.

## Overview

The ESP32 server has been updated to provide the same API interface that the React client expects. This allows you to use the beautiful web client interface to control your ESP32's BLE connection parameters in real-time.

## ESP32 Server Changes

The ESP32 now provides:

### REST API Endpoints:
- `GET /api/state` - Current system state and parameters
- `GET /api/history` - Parameter change history
- `GET /api/presets` - Available parameter presets
- `POST /api/parameters/apply` - Apply staged parameters

### WebSocket Interface:
- **WebSocket Server**: `ws://[ESP32_IP]:81`
- **Real-time Updates**: State changes, parameter updates, history
- **Bidirectional**: Send parameter updates from web client

### Built-in Features:
- **Parameter History**: Last 50 parameter changes with timestamps
- **Preset Management**: 4 built-in presets (Low Power, Balanced, iOS Pairing, Android Pairing)
- **Auto-apply**: Parameters are automatically applied 3 seconds after staging
- **CORS Support**: Cross-origin requests enabled for development

## Client Configuration

### Option 1: Configure Client to Connect to ESP32 Directly

Update the client's WebSocket connection in `client/src/pages/Dashboard.tsx`:

```typescript
// Replace the WebSocket URL section (around line 45) with:
useEffect(() => {
  const ESP32_IP = "192.168.1.xxx"; // Replace with your ESP32's IP address
  const wsUrl = `ws://${ESP32_IP}:81`;
  const socket = new WebSocket(wsUrl);

  // ... rest of the WebSocket code stays the same
}, [toast]);
```

### Option 2: Use Development Proxy

Add to your `client/package.json`:

```json
{
  "scripts": {
    // ... existing scripts
  },
  "proxy": "http://[ESP32_IP]"
}
```

### Option 3: Configure Vite Proxy (Recommended)

Update `vite.config.ts`:

```typescript
export default defineConfig({
  // ... existing config
  server: {
    proxy: {
      '/api': {
        target: 'http://[ESP32_IP]',
        changeOrigin: true
      },
      '/ws': {
        target: 'ws://[ESP32_IP]:81',
        ws: true
      }
    }
  }
});
```

## Usage Instructions

### 1. Flash the ESP32
```bash
pio run -e esp32s3-n16r8 -t upload
```

### 2. Find ESP32 IP Address
Check the Serial Monitor for the IP address after WiFi connection.

### 3. Configure Client
Choose one of the configuration options above and update the ESP32's IP address.

### 4. Start the Client
```bash
cd client
npm install
npm run dev
```

### 5. Access the Interface
- **Development Server**: http://localhost:5000
- **Direct ESP32**: http://[ESP32_IP] (basic info page)

## API Data Format

### State Response (`/api/state`)
```json
{
  "parameters": {
    "current": {
      "connectionIntervalMin": 300.0,
      "connectionIntervalMax": 480.0,
      "peripheralLatency": 4,
      "supervisionTimeout": 3000.0
    },
    "next": null,
    "previous": null
  },
  "status": {
    "isAdvertising": true,
    "isConnected": false,
    "connectedDeviceName": null,
    "browserConnected": true
  }
}
```

### WebSocket Messages

#### Parameter Update (Client → ESP32)
```json
{
  "type": "update_parameters",
  "parameters": {
    "connectionIntervalMin": 250.0,
    "connectionIntervalMax": 400.0,
    "peripheralLatency": 3,
    "supervisionTimeout": 2000.0
  }
}
```

#### State Update (ESP32 → Client)
```json
{
  "type": "state_update",
  "state": { /* same as /api/state response */ }
}
```

## Built-in Presets

The ESP32 includes 4 pre-configured parameter sets:

1. **Low Power**: Optimized for battery life (1000-1250ms intervals)
2. **Balanced**: Good balance of power and performance (300-480ms intervals)
3. **High Performance**: Optimized for responsiveness (20-50ms intervals)
4. **Gaming**: Low latency for gaming (15-30ms intervals)

## Features Working

✅ **Real-time Updates**: WebSocket broadcasts state changes
✅ **Parameter Validation**: Client-side and server-side validation
✅ **History Tracking**: Last 50 parameter changes stored
✅ **BLE Integration**: Changes applied to actual BLE connection
✅ **Presets**: 4 built-in parameter presets
✅ **Auto-apply**: Staged parameters automatically applied
✅ **CORS Support**: Cross-origin requests enabled
✅ **Status Monitoring**: Connection state, advertising status

## Troubleshooting

### WebSocket Connection Issues
1. Check ESP32 IP address in Serial Monitor
2. Ensure ESP32 and client are on same network
3. Verify port 81 is not blocked by firewall

### API Not Responding
1. Check ESP32 WiFi connection status
2. Verify ESP32 IP address is accessible
3. Check CORS headers in browser developer tools

### Parameter Updates Not Applied
1. Check Serial Monitor for BLE connection status
2. Verify parameter values are within valid ranges
3. Ensure BLE central device supports parameter updates

## Example Usage Flow

1. **Connect BLE Device**: Pair your phone/device with ESP32
2. **Open Web Client**: Navigate to the client interface
3. **Monitor Status**: See connection state and current parameters
4. **Apply Preset**: Click a preset to stage new parameters
5. **Send Parameters**: Click "Send Next" or wait for auto-apply
6. **View Results**: Parameters automatically applied to BLE connection
7. **Check History**: View timeline of parameter changes