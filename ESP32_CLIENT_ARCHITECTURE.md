# ESP32 BLE Client Architecture

Canonical setup and run commands are maintained in `README.md`. This file focuses on architecture decisions and client-mode behavior.

## Overview
This branch (`esp32-ble-client-manager`) implements the **inverse architecture** where the ESP32 acts as a **BLE Central/Client** device that scans for and connects to BLE peripherals, then applies connection parameters to those connected devices.

## Architecture Comparison

### Original Architecture (main/server branches)
- ESP32 = **BLE Peripheral/Server**
- External devices = **BLE Central/Client** (connect TO the ESP32)
- ESP32 advertises and accepts connections
- Central devices apply connection parameters TO the ESP32

### New Client Architecture (this branch)
- ESP32 = **BLE Central/Client** 
- External devices = **BLE Peripheral/Server** (ESP32 connects TO them)
- ESP32 scans for and connects to peripherals
- ESP32 applies connection parameters TO connected peripherals

## Key Changes Made

### 1. Global Variables
```cpp
// OLD (Server):
NimBLEServer* pServer = nullptr;

// NEW (Client):
NimBLEClient* pClient = nullptr;
NimBLEScan* pScan = nullptr;
```

### 2. Callback Classes
```cpp
// OLD (Server):
class ServerCallbacks : public NimBLEServerCallbacks { ... }

// NEW (Client):
class ClientCallbacks : public NimBLEClientCallbacks { ... }
class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks { ... }
```

### 3. BLE Setup Function
```cpp
// OLD (Server): Create service, advertise, wait for connections
pServer = NimBLEDevice::createServer();
pServer->setCallbacks(new ServerCallbacks());
pService = pServer->createService(SERVICE_UUID);
// ... create characteristics
pServer->startAdvertising();

// NEW (Client): Initialize scanning for peripherals
pClient = NimBLEDevice::createClient();
pClient->setClientCallbacks(new ClientCallbacks());
pScan = NimBLEDevice::getScan();
pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
pScan->setActiveScan(true);
```

### 4. Connection Management
```cpp
// OLD (Server): Handle incoming connections
void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
    currentConnHandle = connInfo.getConnHandle();
    isConnected = true;
}

// NEW (Client): Handle outgoing connections  
void onConnect(NimBLEClient* pClient) {
    isConnected = true;
    currentConnHandle = pClient->getConnId();
}
```

### 5. Parameter Application
```cpp
// OLD (Server): Receive parameters from central
// Parameters applied TO the ESP32 BY external central

// NEW (Client): Apply parameters to peripheral
pClient->updateConnParams(minUnits, maxUnits, latency, timeoutUnits);
// Parameters applied BY the ESP32 TO connected peripheral
```

## How It Works

### 1. Startup Sequence
1. ESP32 initializes BLE client
2. Starts scanning for BLE peripherals
3. Web dashboard remains accessible via WiFi AP mode
4. Users can configure parameters via web interface

### 2. Device Discovery
1. ESP32 continuously scans for BLE devices
2. When device found, attempts connection
3. Connection info displayed on dashboard and serial monitor

### 3. Parameter Application
1. User sets parameters via web dashboard
2. Parameters stored in ESP32 memory
3. If connected to peripheral, parameters applied immediately
4. If not connected, parameters applied on next connection

### 4. Connection Lifecycle
1. **Scan** → Find peripheral devices
2. **Connect** → Establish BLE connection to selected device  
3. **Apply** → Send connection parameters to peripheral
4. **Monitor** → Track connection status and parameter effectiveness
5. **Disconnect/Reconnect** → Handle connection management

## Dashboard Interface

The React dashboard interface remains the same - users can still:
- ✅ Set connection intervals (min/max)
- ✅ Configure slave latency
- ✅ Set supervision timeout
- ✅ View connection status
- ✅ Monitor parameter history
- ✅ Use preset configurations

The difference is that now these parameters are applied **TO** connected BLE peripherals instead of being applied **BY** BLE centrals.

## Testing the Client Architecture

### 1. Flash the Firmware
```bash
pio run --target upload
```

### 2. Monitor Serial Output
```bash  
pio device monitor
```

### 3. Connect to WiFi AP
- Network: `ESP32-BLE-Control-ae3d98`
- Password: `esp32ble`
- Dashboard: `http://192.168.4.1`

### 4. Test BLE Scanning
- Check serial monitor for peripheral scan results
- Dashboard should show "Scanning for peripherals..." status
- When peripheral found, should show connection attempt

### 5. Test Parameter Application  
- Connect a BLE peripheral device (phone, wearable, etc.)
- Set parameters via dashboard
- Verify parameters applied via protocol analyzer or peripheral logs

## Use Cases

This client architecture is useful for:

1. **BLE Device Testing** - Test how various BLE peripherals respond to different connection parameters
2. **Power Optimization** - Find optimal parameters for battery-powered BLE devices
3. **Connection Quality Testing** - Evaluate connection stability with different parameter sets  
4. **BLE Development** - Prototype and test BLE peripheral device behavior
5. **Research Applications** - Study BLE connection parameter impact on performance

## Next Steps

- [ ] Test with actual BLE peripheral devices
- [ ] Add device filtering/selection in dashboard  
- [ ] Implement connection parameter negotiation feedback
- [ ] Add support for multiple concurrent connections
- [ ] Create automated parameter optimization routines

## Branch Information

- **Branch**: `esp32-ble-client-manager`
- **Base**: Original BLE server implementation
- **Status**: ✅ Compiles successfully, ready for testing
- **Commit**: Complete architectural conversion from server to client