# ESP32 Client Integration Guide

Canonical setup and run commands are maintained in `README.md`. Use that file first, then use this document for integration behavior and API expectations.

## Overview
Your ESP32 server is now fully configured to work with the React client dashboard. The ESP32 provides the same REST API and WebSocket interface as the original Node.js server, allowing the sophisticated React client to control BLE connection parameters.

## ESP32 Server Features
- **REST API**: Full compatibility with client's expected endpoints
- **WebSocket**: Real-time parameter updates on port 81
- **CORS Support**: Cross-origin requests from web browsers
- **Built-in Presets**: Common BLE parameter configurations
- **Parameter History**: Automatic logging of parameter changes
- **Web Server**: Static file serving on port 80

## API Endpoints
The ESP32 provides these endpoints that match your React client's expectations:

### REST API (Port 80)
- `GET /api/state` - Current BLE connection parameters
- `GET /api/history` - Parameter change history 
- `GET /api/presets` - Available parameter presets
- `POST /api/parameters/apply` - Apply new connection parameters

### WebSocket (Port 81)
- Real-time updates when parameters change
- Broadcasts `parameterUpdate` events with current state

## Client Configuration Steps

### 1. Find your ESP32's IP Address
After uploading the firmware to your ESP32-S3:
1. Open the Arduino Serial Monitor (115200 baud)
2. Reset the ESP32
3. Look for output like: `WiFi connected! IP address: 192.168.1.100`
4. Note this IP address

### 2. Update Client Configuration
Navigate to your client folder and update the connection settings:

```bash
cd client
```

Create or update a `.env` file in the client folder:
```env
# ESP32 Server Configuration
VITE_API_BASE_URL=http://192.168.1.100
VITE_WS_URL=ws://192.168.1.100:81
```

Replace `192.168.1.100` with your actual ESP32 IP address.

### 3. Update Client Code (if needed)
If your client doesn't use environment variables, update the connection URLs directly in your code:

**In `src/lib/queryClient.ts` or similar:**
```typescript
const API_BASE_URL = 'http://192.168.1.100';  // Your ESP32 IP
const WS_URL = 'ws://192.168.1.100:81';       // WebSocket port
```

### 4. Install Dependencies and Run Client
```bash
npm install
npm run dev
```

Your React client should now connect to the ESP32 server!

## Expected Behavior

### On Successful Connection:
1. **Dashboard loads** with current BLE parameters
2. **Real-time updates** show parameter changes
3. **Charts display** connection parameter history
4. **Presets work** for quick parameter changes
5. **Export functions** provide data in CSV/JSON formats

### Status Panel Shows:
- BLE Status: "Connected" or "Advertising" 
- Connected Device: MAC address when connected
- Last Update: Real-time timestamp
- Parameter History: Live chart updates

## API Response Examples

### GET /api/state
```json
{
  "connectionInterval": 50,
  "latency": 0,
  "supervisionTimeout": 400,
  "timestamp": "2024-01-10T10:30:00Z",
  "status": "connected",
  "connectedDevice": "AA:BB:CC:DD:EE:FF"
}
```

### GET /api/presets
```json
[
  {
    "id": "low_power",
    "name": "Low Power",
    "description": "Optimized for battery life",
    "connectionInterval": 100,
    "latency": 4,
    "supervisionTimeout": 600
  },
  {
    "id": "high_performance", 
    "name": "High Performance",
    "description": "Low latency for real-time applications",
    "connectionInterval": 20,
    "latency": 0,
    "supervisionTimeout": 200
  }
]
```

### WebSocket Events
```json
{
  "type": "parameterUpdate",
  "data": {
    "connectionInterval": 50,
    "latency": 0,
    "supervisionTimeout": 400,
    "timestamp": "2024-01-10T10:30:00Z"
  }
}
```

## Troubleshooting

### Client Can't Connect
1. **Check ESP32 Serial Output**: Verify WiFi connection and IP address
2. **Network Access**: Ensure ESP32 and client are on same network
3. **Firewall**: Some networks may block WebSocket connections
4. **CORS Issues**: Check browser console for CORS errors

### WebSocket Connection Fails
1. **Port 81**: Ensure WebSocket connects to port 81, not 80
2. **Protocol**: Use `ws://` not `wss://` for local connections
3. **Browser Security**: Some browsers block WS connections from HTTPS pages

### API Requests Fail
1. **CORS Headers**: ESP32 includes proper CORS headers
2. **Content-Type**: Ensure POST requests include `Content-Type: application/json`
3. **Network Timeout**: ESP32 may take a moment to respond during BLE operations

## Development vs Production
- **Development**: Use ESP32 IP address directly
- **Production**: Consider using mDNS (`.local` domains) or static IP configuration

## Next Steps
1. Upload firmware to ESP32-S3
2. Note the assigned IP address
3. Update client configuration
4. Test the web dashboard
5. Monitor real-time parameter updates

Your React client's beautiful dashboard will now control your ESP32 BLE parameters with real-time updates and rich data visualization!