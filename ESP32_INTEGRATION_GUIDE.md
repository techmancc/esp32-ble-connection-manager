# ESP32 BLE Firmware Integration Guide

## Overview

This document describes how to integrate ESP32 firmware with the BLE Connection Parameter Dashboard web interface. The web interface communicates with the ESP32 via WebSocket to monitor connection status and configure BLE connection parameters in real-time.

## Architecture

```
┌─────────────┐         WebSocket          ┌──────────────┐
│             │◄──────────────────────────►│              │
│  Web        │   ws://[host]/ws           │   ESP32      │
│  Dashboard  │                            │   Firmware   │
│             │                            │              │
└─────────────┘                            └──────────────┘
                                                   │
                                                   │ BLE
                                                   ▼
                                           ┌──────────────┐
                                           │  Mobile      │
                                           │  Device      │
                                           └──────────────┘
```

## WebSocket Connection

### Endpoint
```
ws://[your-server-host]/ws
```

For local development:
```
ws://localhost:5000/ws
```

### Connection Flow

1. ESP32 connects to WiFi
2. ESP32 establishes WebSocket connection to the server
3. Server sends initial state update
4. Bidirectional communication begins

## Message Protocol

All messages are JSON-formatted strings.

### Messages from ESP32 to Server

#### 1. State Update (Send Regularly)

The ESP32 should send state updates whenever:
- Connection status changes
- Parameters are applied
- Advertising state changes
- A connected device name is discovered

```json
{
  "type": "state_update",
  "state": {
    "parameters": {
      "previous": {
        "connectionIntervalMin": 280.0,
        "connectionIntervalMax": 350.0,
        "peripheralLatency": 8,
        "supervisionTimeout": 3000.0
      },
      "current": {
        "connectionIntervalMin": 100.0,
        "connectionIntervalMax": 200.0,
        "peripheralLatency": 2,
        "supervisionTimeout": 4000.0
      },
      "next": {
        "connectionIntervalMin": 15.0,
        "connectionIntervalMax": 30.0,
        "peripheralLatency": 0,
        "supervisionTimeout": 2000.0
      }
    },
    "status": {
      "isAdvertising": true,
      "isConnected": true,
      "connectedDeviceName": "iPhone 14 Pro",
      "browserConnected": true
    }
  }
}
```

**Field Descriptions:**

| Field | Type | Description |
|-------|------|-------------|
| `parameters.previous` | Object or null | Parameters that were used before current ones |
| `parameters.current` | Object | Currently active BLE connection parameters |
| `parameters.next` | Object or null | Parameters staged for next application |
| `status.isAdvertising` | boolean | True if ESP32 is advertising BLE services |
| `status.isConnected` | boolean | True if a BLE device is connected |
| `status.connectedDeviceName` | string or null | Name of connected device (e.g., "iPhone 14 Pro") |
| `status.browserConnected` | boolean | Always true when ESP32 has WebSocket connection |

#### 2. Parameter Update Confirmation

After successfully staging parameters received from the web interface:

```json
{
  "type": "parameter_update_success"
}
```

#### 3. Parameter Update Error

If parameters cannot be applied:

```json
{
  "type": "parameter_update_error",
  "error": "Failed to update connection parameters: timeout exceeded"
}
```

### Messages from Server to ESP32

#### 1. Update Parameters Request

The web interface sends this when the user clicks "Send Next":

```json
{
  "type": "update_parameters",
  "parameters": {
    "connectionIntervalMin": 100.0,
    "connectionIntervalMax": 200.0,
    "peripheralLatency": 2,
    "supervisionTimeout": 4000.0
  }
}
```

**Note:** Not all fields may be present. Only the parameters the user wants to update are included.

**ESP32 Response Flow:**
1. Receive message
2. Validate parameter values
3. Stage parameters for next connection update
4. Send `parameter_update_success` or `parameter_update_error`
5. When BLE connection parameters are actually applied, send full `state_update` with `next: null`

## BLE Connection Parameters

### Parameter Specifications

#### Connection Interval Min/Max
- **Range:** 7.5 - 4000.0 ms
- **Type:** Float
- **BLE Units:** 1.25 ms steps (value = ms / 1.25)
- **Description:** Time between connection events. Lower = more responsive, higher = more power efficient

#### Peripheral Latency
- **Range:** 0 - 499 count
- **Type:** Integer
- **Description:** Number of connection events the peripheral can skip. 0 = no latency

#### Supervision Timeout
- **Range:** 100 - 32000 ms
- **Type:** Float
- **BLE Units:** 10 ms steps (value = ms / 10)
- **Description:** Maximum time between successful data exchanges before connection is considered lost

### Validation Rules

1. `connectionIntervalMax` should be ≥ `connectionIntervalMin`
2. `supervisionTimeout` > (1 + `peripheralLatency`) × `connectionIntervalMax` × 2
3. All values must be within specified ranges

## ESP32 Implementation Guide

### Required Arduino Libraries

```cpp
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEServer.h>
```

Recommended libraries:
- **WiFi:** Built-in ESP32
- **WebSocketsClient:** https://github.com/Links2004/arduinoWebSockets
- **ArduinoJson:** https://arduinojson.org/

### Sample Code Structure

```cpp
// WiFi credentials
const char* ssid = "your-wifi-ssid";
const char* password = "your-wifi-password";

// WebSocket server
const char* wsHost = "192.168.1.100";  // Your server IP
const uint16_t wsPort = 5000;

WebSocketsClient webSocket;

// BLE connection parameters
struct ConnectionParams {
    float intervalMin;
    float intervalMax;
    uint16_t latency;
    float timeout;
};

ConnectionParams currentParams = {280.0, 350.0, 8, 3000.0};
ConnectionParams nextParams = {0, 0, 0, 0};
bool hasNextParams = false;

void setup() {
    Serial.begin(115200);
    
    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
    
    // Setup WebSocket
    webSocket.begin(wsHost, wsPort, "/ws");
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(5000);
    
    // Initialize BLE
    initBLE();
}

void loop() {
    webSocket.loop();
    
    // Check if BLE connection parameters need updating
    if (hasNextParams && isConnected) {
        applyConnectionParameters();
    }
    
    // Send periodic state updates
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 5000) {
        sendStateUpdate();
        lastUpdate = millis();
    }
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_CONNECTED:
            Serial.println("WebSocket Connected");
            sendStateUpdate();
            break;
            
        case WStype_DISCONNECTED:
            Serial.println("WebSocket Disconnected");
            break;
            
        case WStype_TEXT:
            handleWebSocketMessage((char*)payload);
            break;
    }
}

void handleWebSocketMessage(char* payload) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        Serial.println("JSON parse error");
        return;
    }
    
    const char* type = doc["type"];
    
    if (strcmp(type, "update_parameters") == 0) {
        JsonObject params = doc["parameters"];
        
        // Store next parameters
        if (params.containsKey("connectionIntervalMin"))
            nextParams.intervalMin = params["connectionIntervalMin"];
        if (params.containsKey("connectionIntervalMax"))
            nextParams.intervalMax = params["connectionIntervalMax"];
        if (params.containsKey("peripheralLatency"))
            nextParams.latency = params["peripheralLatency"];
        if (params.containsKey("supervisionTimeout"))
            nextParams.timeout = params["supervisionTimeout"];
        
        hasNextParams = true;
        
        // Send success confirmation
        sendParameterUpdateSuccess();
    }
}

void sendStateUpdate() {
    StaticJsonDocument<2048> doc;
    doc["type"] = "state_update";
    
    JsonObject state = doc.createNestedObject("state");
    
    // Parameters
    JsonObject parameters = state.createNestedObject("parameters");
    
    // Current parameters
    JsonObject current = parameters.createNestedObject("current");
    current["connectionIntervalMin"] = currentParams.intervalMin;
    current["connectionIntervalMax"] = currentParams.intervalMax;
    current["peripheralLatency"] = currentParams.latency;
    current["supervisionTimeout"] = currentParams.timeout;
    
    // Next parameters (if staged)
    if (hasNextParams) {
        JsonObject next = parameters.createNestedObject("next");
        next["connectionIntervalMin"] = nextParams.intervalMin;
        next["connectionIntervalMax"] = nextParams.intervalMax;
        next["peripheralLatency"] = nextParams.latency;
        next["supervisionTimeout"] = nextParams.timeout;
    } else {
        parameters["next"] = nullptr;
    }
    
    // Previous parameters
    parameters["previous"] = nullptr;  // Track if needed
    
    // Status
    JsonObject status = state.createNestedObject("status");
    status["isAdvertising"] = BLEDevice::getAdvertising()->isAdvertising();
    status["isConnected"] = isConnected;
    status["connectedDeviceName"] = connectedDeviceName;
    status["browserConnected"] = true;
    
    String output;
    serializeJson(doc, output);
    webSocket.sendTXT(output);
}

void sendParameterUpdateSuccess() {
    StaticJsonDocument<128> doc;
    doc["type"] = "parameter_update_success";
    
    String output;
    serializeJson(doc, output);
    webSocket.sendTXT(output);
}

void applyConnectionParameters() {
    // Apply BLE connection parameter update
    // This is ESP32 BLE-specific implementation
    
    // After successful application:
    currentParams = nextParams;
    hasNextParams = false;
    
    // Send state update to reflect changes
    sendStateUpdate();
}
```

## BLE Connection Parameter Update (ESP32-Specific)

The ESP32 can request connection parameter updates using the BLE stack:

```cpp
#include <esp_gap_ble_api.h>

void applyConnectionParameters() {
    if (!isConnected) return;
    
    // Convert milliseconds to BLE units
    uint16_t minInterval = (uint16_t)(nextParams.intervalMin / 1.25);
    uint16_t maxInterval = (uint16_t)(nextParams.intervalMax / 1.25);
    uint16_t latency = nextParams.latency;
    uint16_t timeout = (uint16_t)(nextParams.timeout / 10);
    
    // Update connection parameters
    esp_ble_conn_update_params_t conn_params;
    memcpy(conn_params.bda, connectedDeviceAddress, sizeof(esp_bd_addr_t));
    conn_params.min_int = minInterval;
    conn_params.max_int = maxInterval;
    conn_params.latency = latency;
    conn_params.timeout = timeout;
    
    esp_ble_gap_update_conn_params(&conn_params);
    
    // Update current parameters
    currentParams = nextParams;
    hasNextParams = false;
    
    // Notify web interface
    sendStateUpdate();
}
```

## Testing

### Local Testing Setup

1. **Start the web server:**
   ```bash
   npm run dev
   ```
   Server runs on `http://localhost:5000`

2. **Find your computer's IP address:**
   - Windows: `ipconfig`
   - Mac/Linux: `ifconfig` or `ip addr`

3. **Configure ESP32:**
   ```cpp
   const char* wsHost = "192.168.1.100";  // Your computer's IP
   const uint16_t wsPort = 5000;
   ```

4. **Monitor Serial Output:**
   ```
   WebSocket Connected
   Sending state update...
   Received parameter update: 100.0, 200.0, 2, 4000.0
   Parameters applied successfully
   ```

### Debugging Tips

- **WebSocket not connecting:** Check firewall settings, ensure server is running
- **JSON parsing errors:** Verify message format matches specification
- **Parameters not applying:** Check BLE connection state, verify conversion to BLE units
- **State not updating in UI:** Ensure `browserConnected: true` in status

## Parameter Presets

The web interface includes three presets that users can quickly apply:

### Low Latency
```json
{
  "connectionIntervalMin": 7.5,
  "connectionIntervalMax": 15.0,
  "peripheralLatency": 0,
  "supervisionTimeout": 2000.0
}
```
**Use case:** Gaming, audio streaming, real-time control

### Power Saving
```json
{
  "connectionIntervalMin": 1000.0,
  "connectionIntervalMax": 2000.0,
  "peripheralLatency": 4,
  "supervisionTimeout": 6000.0
}
```
**Use case:** Sensors, periodic data collection, battery-powered devices

### Balanced
```json
{
  "connectionIntervalMin": 100.0,
  "connectionIntervalMax": 200.0,
  "peripheralLatency": 2,
  "supervisionTimeout": 4000.0
}
```
**Use case:** General-purpose applications, moderate responsiveness and power consumption

## Database Integration

The web interface automatically logs all parameter changes to a PostgreSQL database with timestamps. This allows:

- Historical trend analysis via the chart
- Export functionality (JSON/CSV)
- Parameter history review
- Troubleshooting and debugging

You don't need to implement anything special on the ESP32 side - the server handles all database operations.

## Security Considerations

For production deployments:

1. **Use WSS (WebSocket Secure):** Encrypt WebSocket traffic
2. **Authentication:** Implement token-based authentication
3. **Input Validation:** Validate all parameters server-side
4. **Rate Limiting:** Prevent abuse of parameter update endpoints
5. **WiFi Security:** Use WPA2/WPA3 for WiFi connections

## Troubleshooting

### Common Issues

**Issue:** WebSocket connects but no state updates appear
- **Solution:** Verify JSON format exactly matches specification
- Check that `browserConnected: true` is set in status

**Issue:** Parameters sent but ESP32 doesn't apply them
- **Solution:** Verify BLE connection is active before applying
- Check parameter conversion to BLE units is correct

**Issue:** Chart shows zero values
- **Solution:** Ensure timestamp format includes seconds (HH:mm:ss)
- Verify all parameter values are numeric (not strings)

**Issue:** Export contains incomplete data
- **Solution:** State updates must include all current parameters
- Ensure parameters are logged after successful application

## Additional Resources

- **BLE Specification:** [Bluetooth Core Specification](https://www.bluetooth.com/specifications/bluetooth-core-specification/)
- **ESP32 BLE Documentation:** [ESP-IDF BLE Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/index.html)
- **WebSocket Protocol:** [RFC 6455](https://tools.ietf.org/html/rfc6455)
- **ArduinoJson Assistant:** [JSON Generator Tool](https://arduinojson.org/v6/assistant/)

## Support

For questions about the web interface implementation, refer to the source code in:
- `server/routes.ts` - WebSocket message handling
- `server/storage.ts` - Parameter storage interface
- `shared/schema.ts` - Data type definitions
- `client/src/pages/Dashboard.tsx` - UI implementation

---

**Version:** 1.0  
**Last Updated:** November 14, 2025
