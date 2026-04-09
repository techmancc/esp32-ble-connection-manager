# ESP32 BLE Connection Manager

A comprehensive ESP32-based system for managing BLE connection parameters with a web-based dashboard interface. Supports both **BLE Server** and **BLE Client** architectures for maximum flexibility in BLE testing and development.

## 🎯 Project Overview

This system allows real-time configuration and monitoring of BLE connection parameters through an intuitive web dashboard. The ESP32 can operate in two distinct modes:

### 🔄 **Dual Architecture Support**

| Mode | ESP32 Role | Use Case | Branch |
|------|------------|----------|---------|
| **Server** | BLE Peripheral | Receives connections, accepts parameter changes | `main`, `feature/bluetooth-pairing` |
| **Client** | BLE Central | Initiates connections, applies parameters to peripherals | `esp32-ble-client-manager` |

## 🚀 Quick Start

### **Prerequisites**
- ESP32-S3 DevKit (or compatible ESP32 board)
- VS Code with PlatformIO extension
- Windows/Mac/Linux development environment

### **1. Clone Repository**
```bash
git clone https://github.com/techmancc/esp32-ble-connection-manager.git
cd esp32-ble-connection-manager
```

### **2. Choose Architecture**
```bash
# For BLE Server mode (original):
git checkout main

# For BLE Client mode (new):  
git checkout esp32-ble-client-manager
```

### **3. Build & Flash Firmware**
**Option A: VS Code Tasks (Recommended)**
1. Open project in VS Code: `code .`
2. Press `Ctrl+Shift+P` → `Tasks: Run Task` → `PlatformIO: Build`
3. Press `Ctrl+Shift+P` → `Tasks: Run Task` → `PlatformIO: Upload`

**Option B: PlatformIO CLI**
```bash
pio run                    # Compile
pio run --target upload    # Flash to ESP32
pio device monitor         # Serial monitor
```

### **4. Connect & Configure**
1. **Connect to ESP32 WiFi AP:**
   - Network: `ESP32-BLE-Control-ae3d98`
   - Password: `esp32ble`
2. **Open Web Dashboard:** `http://192.168.4.1`
3. **Configure BLE parameters and test**

## 📡 **BLE Server Mode** (Original Architecture)

### **How It Works**
- ESP32 acts as **BLE Peripheral/Server**
- External devices connect **TO** the ESP32
- Central devices can modify ESP32's connection parameters
- ESP32 advertises as "BLE_CFG_SECURE"

### **Use Cases**
- Testing how your ESP32 device handles different connection parameters
- Developing BLE peripheral applications
- Optimizing power consumption for battery-powered ESP32 projects

### **Connection Flow**
```
BLE Central Device → Scans & Connects → ESP32 (Peripheral)
BLE Central Device → Updates Parameters → ESP32 Connection
```

## 🎯 **BLE Client Mode** (New Architecture)

### **How It Works**
- ESP32 acts as **BLE Central/Client**
- ESP32 scans for and connects **TO** BLE peripherals
- ESP32 applies connection parameters to connected peripherals
- Ideal for testing external BLE devices

### **Use Cases**
- Testing BLE peripheral device behavior with different parameters
- Optimizing connection parameters for IoT devices, wearables, sensors
- BLE device compatibility testing
- Research and development of BLE applications

### **Connection Flow**
```
ESP32 (Central) → Scans & Connects → BLE Peripheral Device
ESP32 → Updates Parameters → Peripheral Connection  
```

## 🎛️ Dashboard Features

### **Parameter Configuration**
- **Connection Interval:** 7.5ms - 4000ms (min/max ranges)
- **Slave Latency:** 0-499 (number of skipped connection events)
- **Supervision Timeout:** 100ms - 32000ms (connection timeout)
- **Preset Configurations:** Gaming, Audio, IoT Standard, Production optimized

### **Monitoring & Analysis**
- ✅ Real-time connection status
- ✅ Parameter history tracking with timestamps  
- ✅ Export data (JSON, CSV formats)
- ✅ WebSocket-based live updates
- ✅ Security status and pairing management

### **Advanced Features**
- 🔐 BLE Security & Pairing Management
- 📊 Connection Parameter Charts & Visualization
- 📝 Parameter History & Logging
- 🎨 Dark/Light Theme Toggle
- 📦 Preset Management System

## 🔧 Installation & Setup

### **Method 1: VS Code + PlatformIO Extension**
1. Install [VS Code](https://code.visualstudio.com/)
2. Install PlatformIO IDE extension
3. Open project folder in VS Code
4. Use VS Code tasks for build/upload/monitor

### **Method 2: PlatformIO Core**
```bash
# Install PlatformIO Core
pip install platformio

# Build and upload
pio run --target upload
```

### **Troubleshooting PlatformIO CLI Issues**
If `pio` command is not found:

```powershell
# Windows: Add to PATH
[Environment]::SetEnvironmentVariable("Path", $env:Path + ";$env:USERPROFILE\.platformio\penv\Scripts", "User")

# Or use full path
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run
```

**Alternative:** Use the included VS Code tasks - no PATH configuration needed!

## 🌐 Client Dashboard Setup

### **React Development Server**
```bash
cd client
npm install
npm run dev    # Development server on http://localhost:5173
npm run build  # Production build
```

### **Dashboard Access Methods**
1. **Direct ESP32:** `http://192.168.4.1` (WiFi AP mode)
2. **Development:** `http://localhost:5173` (React dev server)
3. **Home WiFi:** Configure ESP32 for your network (see WiFi setup)

## ⚙️ Configuration Options

### **WiFi Configuration**
The ESP32 can operate in multiple WiFi modes:

1. **Access Point Mode (Default)**
   - Creates WiFi network: `ESP32-BLE-Control-ae3d98`
   - Password: `esp32ble`
   - Dashboard: `http://192.168.4.1`

2. **Station Mode (Home WiFi)**
   - Connect to dashboard and configure WiFi credentials
   - ESP32 joins your home network
   - Find new IP address on your router

### **BLE Configuration**
```cpp
// Modify in src/main.cpp for custom settings
#define DEFAULT_MIN_CONN_INTERVAL 240     // 300ms
#define DEFAULT_MAX_CONN_INTERVAL 384     // 480ms  
#define DEFAULT_SUPERVISION_TIMEOUT 5620  // 5620ms
#define DEFAULT_SLAVE_LATENCY 4
```

## 📊 Usage Examples

### **Server Mode: Testing ESP32 Power Optimization**
1. Flash server firmware
2. Connect phone/computer to ESP32
3. Adjust parameters via dashboard
4. Monitor power consumption changes
5. Find optimal parameters for your application

### **Client Mode: Testing BLE Device Compatibility**  
1. Flash client firmware
2. Connect target BLE device (watch, sensor, etc.)
3. ESP32 automatically discovers and connects
4. Test different parameter combinations
5. Analyze device response and stability

### **Parameter Optimization Workflow**
1. **Start with presets:** Choose Gaming/Audio/IoT based on use case
2. **Monitor connection stability:** Watch for disconnections
3. **Adjust intervals:** Lower for responsiveness, higher for power savings
4. **Test latency tolerance:** Increase slave latency for power savings
5. **Validate timeout:** Ensure sufficient supervision timeout
6. **Export results:** Save successful configurations

## 🔍 Architecture Details

### **Server Architecture Components**
- NimBLE Server with configurable services/characteristics
- WebSocket server for real-time dashboard communication  
- BLE Security Manager for pairing/bonding
- Parameter validation and history tracking

### **Client Architecture Components**
- NimBLE Client with scanning and connection management
- Device discovery with filtering capabilities
- Connection parameter update requests to peripherals
- Real-time monitoring of client connections

## 🛠️ Development

### **Project Structure**
```
├── src/
│   └── main.cpp              # ESP32 firmware (server or client)
├── client/                   # React dashboard application
│   ├── src/components/       # UI components
│   ├── public/              # Static assets
│   └── package.json         # Node.js dependencies
├── platformio.ini           # PlatformIO configuration
├── .vscode/tasks.json       # VS Code build tasks
└── ESP32_CLIENT_ARCHITECTURE.md  # Detailed client mode docs
```

### **Key Dependencies**
- **NimBLE-Arduino:** Bluetooth Low Energy stack
- **ArduinoJson:** JSON handling for API responses
- **WebSockets:** Real-time dashboard communication
- **React + Vite:** Modern dashboard interface
- **Tailwind CSS:** Responsive UI styling

### **Adding Custom Features**
1. **New Parameter Types:** Extend `ConnectionParams` struct
2. **Additional Presets:** Modify `parameterPresets` array
3. **Dashboard Components:** Add React components in `client/src/components/`
4. **API Endpoints:** Extend WebSocket or HTTP API handlers

## 📈 Monitoring & Debugging

### **Serial Monitor Output**
```
Status: connected=true, connHandle=1, authenticated=true, paired_devices=2
stored[min=300.00ms,max=480.00ms,sto=5620ms,lat=4]
lastRequested[min=250.00ms,max=300.00ms,sto=5990ms,lat=8]
Successfully requested connection parameter update: Min=250.0, Max=300.0, Latency=8, Timeout=599.0
```

### **Dashboard Status Indicators**
- 🟢 **Connected:** Active BLE connection established
- 🟡 **Scanning:** Discovering BLE devices (client mode)  
- 🔴 **Disconnected:** No active BLE connection
- 🔐 **Authenticated:** Secure pairing completed
- ⚡ **Parameter Applied:** Connection parameters updated successfully

### **Common Issues & Solutions**
| Issue | Cause | Solution |
|-------|-------|----------|
| `pio` command not found | PATH not configured | Use VS Code tasks or fix PATH |
| ESP32 not detected | USB driver missing | Install CH343/CP210x drivers |
| BLE connection fails | Device incompatibility | Check BLE version compatibility |
| Parameters not applied | Authentication required | Complete pairing process |
| Dashboard doesn't load | WiFi connection issue | Verify ESP32 AP network connection |

## 🤝 Contributing

1. Fork the repository
2. Create feature branch: `git checkout -b feature/your-feature`
3. Commit changes: `git commit -m "Add your feature"`
4. Push branch: `git push origin feature/your-feature`
5. Create Pull Request

## 📄 License

This project is open-source and available under the MIT License.

## 🔗 Related Resources

- [ESP32-S3 Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [NimBLE-Arduino Library](https://github.com/h2zero/NimBLE-Arduino)
- [BLE Connection Parameters Guide](https://www.bluetooth.com/blog/bluetooth-low-energy-connection-parameters/)
- [PlatformIO ESP32 Guide](https://docs.platformio.org/en/latest/platforms/espressif32.html)

---

## 🎉 **Ready to get started?**

Choose your architecture mode, flash the firmware, and start optimizing BLE connections! 

For questions or issues, check the [troubleshooting section](#-monitoring--debugging) or create a GitHub issue.