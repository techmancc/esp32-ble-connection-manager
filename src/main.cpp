#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLESecurity.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

// --- Configuration ---
// ESP32 now runs in Access Point mode - no WiFi credentials needed!

// BLE Service / Characteristic UUIDs
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define MIN_INTERVAL_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define MAX_INTERVAL_UUID "beb5483e-36e1-4688-b7f5-ea07361b26ab"
#define SUPERVISION_TO_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define SLAVE_LATENCY_UUID "beb5483e-36e1-4688-b7f5-ea07361b26aa"

// Defaults in BLE units (interval = 1.25ms units, timeout = 10ms units)
#define DEFAULT_MIN_CONN_INTERVAL 240     // 300ms
#define DEFAULT_MAX_CONN_INTERVAL 384     // 480ms
#define DEFAULT_SUPERVISION_TIMEOUT 5620  // 5620 ms
#define DEFAULT_SLAVE_LATENCY 4

struct ConnectionParams {
  float minInterval;         // ms
  float maxInterval;         // ms
  float supervisionTimeout;  // ms
  int slaveLatency;          // count
};

struct ParameterHistory {
  unsigned long timestamp;     // Unix timestamp (seconds since epoch)
  unsigned long millisOffset;  // Additional milliseconds for precision
  float connectionIntervalMin;
  float connectionIntervalMax;
  int peripheralLatency;
  float supervisionTimeout;
  String source;
};

struct ParameterPreset {
  String name;
  String description;
  float connectionIntervalMin;
  float connectionIntervalMax;
  int peripheralLatency;
  float supervisionTimeout;
};

// Security and Pairing structures
struct PairedDevice {
  String address;
  String name;
  unsigned long pairedTime;
  bool trusted;
  bool authenticated;
};

struct SecurityStatus {
  bool pairingEnabled;
  bool requireAuthentication;
  int pairingMethod; // 0=Just Works, 1=PIN, 2=Passkey
  String currentPin;
  bool isPairing;
  String pairingDeviceAddress;
  String pairingDeviceName;
  int pairedDeviceCount;
};

// Globals
Preferences preferences;
ConnectionParams storedParams;
ConnectionParams previousParams;       // Track previous values for comparison
ConnectionParams lastRequestedParams;  // last requested (what we sent in updateConnParams)
ConnectionParams nextParams = {0, 0, 0, 0}; // Parameters to be applied next
bool hasNextParams = false;
bool hasPreviousParams = false;

// BLE Client globals (changed from Server)
NimBLEClient* pClient = nullptr;
NimBLEScan* pScan = nullptr;
bool doConnect = false;
bool isConnected = false;
bool doScan = false;

// Connected peripheral info
String connectedDeviceAddress = "";
String connectedDeviceName = "None";
uint16_t currentConnHandle = 0;

WebServer webServer(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// Parameter history storage (limited to last 50 entries)
ParameterHistory paramHistory[50];
int historyCount = 0;
int historyIndex = 0;

// Built-in presets
ParameterPreset presets[] = {
  {"CGM Preferred", "Maximum interval", 300.0, 480.0, 3, 5760.0},
  {"Production Standard", "Optimized for product reliability", 270.0, 340.0, 7, 5990.0},
  {"iOS Reconn.", "Fast initial connection setup", 250.0, 300.0, 8, 5990.0},
  {"Droid Reconn.", "Fast data exchange", 200.0, 240.0, 10, 5990.0}
};
int presetCount = 4;

// Connection status (removed deviceConnected, using isConnected from client globals)

// Security and Pairing globals  
SecurityStatus securityStatus;
PairedDevice pairedDevices[10]; // Maximum 10 paired devices
int pairedDeviceCount = 0;
NimBLEAddress currentConnectedAddress;
bool isDeviceAuthenticated = false;

unsigned long lastStatusMs = 0;

// Forward declarations
void loadStoredParams();
void saveStoredParams();
void setupBLE();
void setupWebServer();
void setupWebSocket();
void setupWiFi();
void addToHistory(String source);
void broadcastState();
void broadcastHistory();
void serveClientFiles();

// Security function declarations
void setupBLESecurity();
void loadPairedDevices();
void savePairedDevices();
void addPairedDevice(NimBLEAddress address, String name);
void removePairedDevice(String address);
bool isDevicePaired(NimBLEAddress address);
void broadcastSecurityStatus();
void initializeSecurityStatus();

// Helpers: unit conversion
static inline uint16_t ms_to_conn_interval_units(float ms) {
  return (uint16_t)round(ms / 1.25f);
}
static inline float conn_interval_units_to_ms(uint16_t u) {
  return u * 1.25f;
}
static inline uint16_t ms_to_timeout_units(float ms) {
  return (uint16_t)round(ms / 10.0f);
}
static inline float timeout_units_to_ms(uint16_t u) {
  return u * 10.0f;
}

// Security Callback Class
class SecurityCallbacks : public NimBLESecurityCallbacks {
public:
  bool onConfirmPIN(uint32_t pin) override {
    Serial.printf("BLE Security: Confirm PIN: %06" PRIu32 "\n", pin);
    securityStatus.currentPin = String(pin);
    securityStatus.isPairing = true;
    broadcastSecurityStatus();

    // Auto-confirm PIN for now - in production, this should be user-confirmed
    return true;
  }

  uint32_t onPassKeyRequest() override {
    Serial.println("BLE Security: PassKey Requested");
    // Generate a random 6-digit PIN
    uint32_t passkey = esp_random() % 1000000;
    securityStatus.currentPin = String(passkey);
    securityStatus.isPairing = true;
    Serial.printf("Generated PassKey: %06" PRIu32 "\n", passkey);
    broadcastSecurityStatus();
    return passkey;
  }

  void onPassKeyNotify(uint32_t pass_key) override {
    Serial.printf("BLE Security: PassKey Notify: %06" PRIu32 "\n", pass_key);
    securityStatus.currentPin = String(pass_key);
    securityStatus.isPairing = true;
    broadcastSecurityStatus();
  }

  bool onSecurityRequest() override {
    Serial.println("BLE Security: Security Request");
    return true;
  }

  void onAuthenticationComplete(ble_gap_conn_desc* desc) override {
    if (desc->sec_state.encrypted && desc->sec_state.authenticated) {
      Serial.println("BLE Security: Authentication Successful");

      // Get device info
      NimBLEAddress peerAddr = NimBLEAddress(desc->peer_ota_addr);
      String deviceName = "BLE Device";

      // Try to get device name from connected client
      NimBLEClient* pClient = NimBLEDevice::getClientByID(desc->conn_handle);
      if (pClient && pClient->isConnected()) {
        NimBLERemoteService* pService = pClient->getService("1800"); // Generic Access Service
        if (pService) {
          NimBLERemoteCharacteristic* pChar = pService->getCharacteristic("2A00"); // Device Name
          if (pChar && pChar->canRead()) {
            deviceName = pChar->readValue().c_str();
          }
        }
      }

      // Store paired device
      addPairedDevice(peerAddr, deviceName);
      isDeviceAuthenticated = true;
      currentConnectedAddress = peerAddr;

      securityStatus.isPairing = false;
      securityStatus.currentPin = "";
      broadcastSecurityStatus();
      broadcastState();

    } else {
      Serial.println("BLE Security: Authentication Failed");
      securityStatus.isPairing = false;
      securityStatus.currentPin = "";
      broadcastSecurityStatus();
    }
  }
};

// Add to parameter history
void addToHistory(String source) {
  // Store previous values before updating
  if (historyCount > 0) {
    previousParams = storedParams;
    hasPreviousParams = true;
  }

  // Use current Unix timestamp (approximated from system boot time)
  // Note: ESP32 doesn't have RTC by default, so we'll use a base timestamp + millis
  unsigned long currentTimeSeconds = 1700000000 + (millis() / 1000); // Rough 2023 timestamp + runtime
  unsigned long currentMillisOffset = millis() % 1000;

  paramHistory[historyIndex].timestamp = currentTimeSeconds;
  paramHistory[historyIndex].millisOffset = currentMillisOffset;
  paramHistory[historyIndex].connectionIntervalMin = storedParams.minInterval;
  paramHistory[historyIndex].connectionIntervalMax = storedParams.maxInterval;
  paramHistory[historyIndex].peripheralLatency = storedParams.slaveLatency;
  paramHistory[historyIndex].supervisionTimeout = storedParams.supervisionTimeout;
  paramHistory[historyIndex].source = source;

  historyIndex = (historyIndex + 1) % 50;
  if (historyCount < 50) historyCount++;
}

// Security Management Functions
void initializeSecurityStatus() {
  securityStatus.pairingEnabled = true;
  securityStatus.requireAuthentication = true;
  securityStatus.pairingMethod = 1; // PIN method
  securityStatus.currentPin = "";
  securityStatus.isPairing = false;
  securityStatus.pairingDeviceAddress = "";
  securityStatus.pairingDeviceName = "";
  securityStatus.pairedDeviceCount = pairedDeviceCount;
}

void loadPairedDevices() {
  preferences.begin("ble_security", false);
  pairedDeviceCount = preferences.getInt("deviceCount", 0);

  for (int i = 0; i < pairedDeviceCount && i < 10; i++) {
    String prefix = "dev" + String(i) + "_";
    pairedDevices[i].address = preferences.getString((prefix + "addr").c_str(), "");
    pairedDevices[i].name = preferences.getString((prefix + "name").c_str(), "Unknown Device");
    pairedDevices[i].pairedTime = preferences.getULong((prefix + "time").c_str(), 0);
    pairedDevices[i].trusted = preferences.getBool((prefix + "trusted").c_str(), true);
    pairedDevices[i].authenticated = false; // Reset on restart
  }

  preferences.end();
  Serial.printf("Loaded %d paired devices\n", pairedDeviceCount);
}

void savePairedDevices() {
  preferences.begin("ble_security", false);
  preferences.putInt("deviceCount", pairedDeviceCount);

  for (int i = 0; i < pairedDeviceCount && i < 10; i++) {
    String prefix = "dev" + String(i) + "_";
    preferences.putString((prefix + "addr").c_str(), pairedDevices[i].address);
    preferences.putString((prefix + "name").c_str(), pairedDevices[i].name);
    preferences.putULong((prefix + "time").c_str(), pairedDevices[i].pairedTime);
    preferences.putBool((prefix + "trusted").c_str(), pairedDevices[i].trusted);
  }

  preferences.end();
  Serial.printf("Saved %d paired devices\n", pairedDeviceCount);
}

void addPairedDevice(NimBLEAddress address, String name) {
  String addrStr = address.toString().c_str();

  // Check if device already exists
  for (int i = 0; i < pairedDeviceCount; i++) {
    if (pairedDevices[i].address == addrStr) {
      // Update existing device
      pairedDevices[i].name = name;
      pairedDevices[i].pairedTime = millis() / 1000;
      pairedDevices[i].authenticated = true;
      savePairedDevices();
      securityStatus.pairedDeviceCount = pairedDeviceCount;
      Serial.printf("Updated paired device: %s (%s)\n", name.c_str(), addrStr.c_str());
      return;
    }
  }

  // Add new device if space available
  if (pairedDeviceCount < 10) {
    pairedDevices[pairedDeviceCount].address = addrStr;
    pairedDevices[pairedDeviceCount].name = name;
    pairedDevices[pairedDeviceCount].pairedTime = millis() / 1000;
    pairedDevices[pairedDeviceCount].trusted = true;
    pairedDevices[pairedDeviceCount].authenticated = true;
    pairedDeviceCount++;

    savePairedDevices();
    securityStatus.pairedDeviceCount = pairedDeviceCount;
    Serial.printf("Added new paired device: %s (%s)\n", name.c_str(), addrStr.c_str());
  } else {
    Serial.println("Maximum paired devices reached");
  }
}

void removePairedDevice(String address) {
  for (int i = 0; i < pairedDeviceCount; i++) {
    if (pairedDevices[i].address == address) {
      // Shift remaining devices
      for (int j = i; j < pairedDeviceCount - 1; j++) {
        pairedDevices[j] = pairedDevices[j + 1];
      }
      pairedDeviceCount--;
      savePairedDevices();
      securityStatus.pairedDeviceCount = pairedDeviceCount;
      Serial.printf("Removed paired device: %s\n", address.c_str());
      return;
    }
  }
}

bool isDevicePaired(NimBLEAddress address) {
  String addrStr = address.toString().c_str();
  for (int i = 0; i < pairedDeviceCount; i++) {
    if (pairedDevices[i].address == addrStr) {
      return true;
    }
  }
  return false;
}

void broadcastSecurityStatus() {
  DynamicJsonDocument doc(2048);

  doc["type"] = "security_status";
  JsonObject security = doc.createNestedObject("security");

  security["pairingEnabled"] = securityStatus.pairingEnabled;
  security["requireAuthentication"] = securityStatus.requireAuthentication;
  security["pairingMethod"] = securityStatus.pairingMethod;
  security["isPairing"] = securityStatus.isPairing;
  security["currentPin"] = securityStatus.currentPin;
  security["pairingDeviceAddress"] = securityStatus.pairingDeviceAddress;
  security["pairingDeviceName"] = securityStatus.pairingDeviceName;
  security["pairedDeviceCount"] = securityStatus.pairedDeviceCount;
  security["currentDeviceAuthenticated"] = isDeviceAuthenticated;

  // Add paired devices list
  JsonArray devices = security.createNestedArray("pairedDevices");
  for (int i = 0; i < pairedDeviceCount; i++) {
    JsonObject device = devices.createNestedObject();
    device["address"] = pairedDevices[i].address;
    device["name"] = pairedDevices[i].name;
    device["pairedTime"] = (unsigned long long)pairedDevices[i].pairedTime * 1000;
    device["trusted"] = pairedDevices[i].trusted;
    device["authenticated"] = pairedDevices[i].authenticated;
    device["isCurrentDevice"] = (pairedDevices[i].address == currentConnectedAddress.toString().c_str());
  }

  String message;
  serializeJson(doc, message);
  webSocket.broadcastTXT(message);
}

// Save / Load
void loadStoredParams() {
  preferences.begin("ble_cfg", false);
  storedParams.minInterval = preferences.getFloat("minMs", 300.0);
  storedParams.maxInterval = preferences.getFloat("maxMs", 480.0);
  storedParams.supervisionTimeout = preferences.getFloat("stMs", 5620.0);
  storedParams.slaveLatency = preferences.getInt("lat", 4);
  preferences.end();

  // copy to lastRequested as initial
  lastRequestedParams = storedParams;
}

void saveStoredParams() {
  preferences.begin("ble_cfg", false);
  preferences.putFloat("minMs", storedParams.minInterval);
  preferences.putFloat("maxMs", storedParams.maxInterval);
  preferences.putFloat("stMs", storedParams.supervisionTimeout);
  preferences.putInt("lat", storedParams.slaveLatency);
  preferences.end();
}

// BLE server callbacks
// BLE Client callbacks
class ClientCallbacks : public NimBLEClientCallbacks {
public:
  void onConnect(NimBLEClient* pclient) override {
    isConnected = true;
    currentConnHandle = pclient->getConnId();
    connectedDeviceAddress = pclient->getPeerAddress().toString().c_str();
    connectedDeviceName = "BLE Device"; // Default name, can be updated later

    Serial.printf("Connected to BLE peripheral (conn handle %u, address: %s)\n",
                  currentConnHandle, connectedDeviceAddress.c_str());

    // Apply connection parameters as client
    Serial.printf("Requesting conn params: min=%.2f max=%.2f latency=%d timeout=%.0f\n",
                  storedParams.minInterval, storedParams.maxInterval, storedParams.slaveLatency, storedParams.supervisionTimeout);
    
    uint16_t minUnits = ms_to_conn_interval_units(storedParams.minInterval);
    uint16_t maxUnits = ms_to_conn_interval_units(storedParams.maxInterval);
    uint16_t timeoutUnits = ms_to_timeout_units(storedParams.supervisionTimeout);
    
    // Update connection parameters (client requesting from peripheral)
    pClient->updateConnParams(minUnits, maxUnits, storedParams.slaveLatency, timeoutUnits);

    lastRequestedParams = storedParams;
    broadcastState();
    broadcastSecurityStatus();
  }

  void onDisconnect(NimBLEClient* pclient) override {
    (void)pclient;
    isConnected = false;
    currentConnHandle = 0xffff;
    connectedDeviceAddress = "";
    connectedDeviceName = "None";

    Serial.println("Disconnected from BLE peripheral");
    broadcastState();
    broadcastSecurityStatus();
    
    // Start scanning again after disconnect
    doScan = true;
  }
};

// Advertised Device callbacks for scanning
class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
public:
  void onResult(NimBLEAdvertisedDevice* advertisedDevice) override {
    // Auto-connect to first connectable device found
    if (advertisedDevice->isConnectable() && !isConnected && doConnect) {
      pScan->stop();
      
      Serial.printf("Found connectable device: %s\n", advertisedDevice->toString().c_str());
      
      // Create client and connect
      pClient = NimBLEDevice::createClient();
      pClient->setClientCallbacks(new ClientCallbacks(), false);
      
      if (pClient->connect(advertisedDevice)) {
        Serial.println("Connected to peripheral");
        doConnect = false;
      } else {
        Serial.println("Failed to connect to peripheral");
        doScan = true; // Resume scanning
      }
    }
  }
};

// Setup BLE Client for connecting to peripherals
void setupBLE() {
  NimBLEDevice::init("ESP32_BLE_Client_Manager");
  NimBLEDevice::setMTU(100);  // Allow larger BLE messages
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  // Setup BLE Security
  setupBLESecurity();

  // Initialize BLE Scan
  pScan = NimBLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
  pScan->setActiveScan(true); // Active scan uses more power but gets results faster
  pScan->setInterval(100);    // Scan interval in 0.625ms units
  pScan->setWindow(99);       // Scan window in 0.625ms units (must be <= interval)

  // Start scanning for peripherals
  doScan = true;
  
  Serial.println("BLE Client Manager Ready - Scanning for peripherals...");
}

void setupBLESecurity() {
  // Initialize security status
  initializeSecurityStatus();

  // Load paired devices
  loadPairedDevices();

  // Configure security parameters
  NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM | BLE_SM_PAIR_AUTHREQ_SC);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO);
  NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);

  // Set security callbacks
  NimBLEDevice::setSecurityCallbacks(new SecurityCallbacks());

  Serial.println("BLE Security configured: Bonding + MITM + Secure Connections");
}

// --- WebSocket Functions ---
void broadcastState() {
  DynamicJsonDocument doc(2048);

  doc["type"] = "state_update";
  JsonObject state = doc.createNestedObject("state");

  // Parameters
  JsonObject params = state.createNestedObject("parameters");

  // Current parameters
  JsonObject current = params.createNestedObject("current");
  current["connectionIntervalMin"] = storedParams.minInterval;
  current["connectionIntervalMax"] = storedParams.maxInterval;
  current["peripheralLatency"] = storedParams.slaveLatency;
  current["supervisionTimeout"] = storedParams.supervisionTimeout;

  // Next parameters (if any)
  if (hasNextParams) {
    JsonObject next = params.createNestedObject("next");
    next["connectionIntervalMin"] = nextParams.minInterval;
    next["connectionIntervalMax"] = nextParams.maxInterval;
    next["peripheralLatency"] = nextParams.slaveLatency;
    next["supervisionTimeout"] = nextParams.supervisionTimeout;
  } else {
    params["next"] = nullptr;
  }

  params["previous"] = nullptr; // We don't track previous for now

  // Previous parameters (if any)
  if (hasPreviousParams) {
    JsonObject previous = params.createNestedObject("previous");
    previous["connectionIntervalMin"] = previousParams.minInterval;
    previous["connectionIntervalMax"] = previousParams.maxInterval;
    previous["peripheralLatency"] = previousParams.slaveLatency;
    previous["supervisionTimeout"] = previousParams.supervisionTimeout;
  } else {
    params["previous"] = nullptr;
  }

  // Status
  JsonObject status = state.createNestedObject("status");
  status["isAdvertising"] = false; // Client mode - no advertising
  status["isConnected"] = isConnected;
  status["connectedDeviceName"] = isConnected ? connectedDeviceName.c_str() : nullptr;
  status["browserConnected"] = true; // Always true if we're broadcasting
  status["deviceAuthenticated"] = isDeviceAuthenticated;
  status["requiresAuthentication"] = securityStatus.requireAuthentication;
  if (isConnected && !connectedDeviceAddress.isEmpty()) {
    status["connectedDeviceAddress"] = currentConnectedAddress.toString().c_str();
  } else {
    status["connectedDeviceAddress"] = nullptr;
  }

  String message;
  serializeJson(doc, message);
  webSocket.broadcastTXT(message);
}

void broadcastHistory() {
  DynamicJsonDocument doc(4096);

  doc["type"] = "history_update";
  JsonArray history = doc.createNestedArray("history");

  // Send last 10 history entries
  int count = min(historyCount, 10);
  int startIdx = (historyIndex - count + 50) % 50;

  for (int i = 0; i < count; i++) {
    int idx = (startIdx + i) % 50;
    JsonObject entry = history.createNestedObject();
    entry["id"] = idx;
    entry["connectionIntervalMin"] = paramHistory[idx].connectionIntervalMin;
    entry["connectionIntervalMax"] = paramHistory[idx].connectionIntervalMax;
    entry["peripheralLatency"] = paramHistory[idx].peripheralLatency;
    entry["supervisionTimeout"] = paramHistory[idx].supervisionTimeout;
    // Convert to milliseconds timestamp for JavaScript compatibility
    entry["appliedAt"] = (unsigned long long)paramHistory[idx].timestamp * 1000 + paramHistory[idx].millisOffset;
    entry["source"] = paramHistory[idx].source;
  }

  String message;
  serializeJson(doc, message);
  webSocket.broadcastTXT(message);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;

    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

      // Send initial state
      broadcastState();
      broadcastHistory();
      broadcastSecurityStatus();
      break;
    }

    case WStype_TEXT: {
      Serial.printf("[%u] Received: %s\n", num, payload);

      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);

      if (doc["type"] == "update_parameters") {
        JsonObject params = doc["parameters"];

        // Validate and update next parameters
        bool valid = true;
        String errorMsg = "";

        if (params.containsKey("connectionIntervalMin")) {
          float val = params["connectionIntervalMin"];
          if (val < 7.5 || val > 4000) {
            valid = false;
            errorMsg = "Connection Interval Min must be between 7.5 and 4000 ms";
          } else {
            nextParams.minInterval = val;
            hasNextParams = true;
          }
        }

        if (params.containsKey("connectionIntervalMax")) {
          float val = params["connectionIntervalMax"];
          if (val < 7.5 || val > 4000) {
            valid = false;
            errorMsg = "Connection Interval Max must be between 7.5 and 4000 ms";
          } else {
            nextParams.maxInterval = val;
            hasNextParams = true;
          }
        }

        // Additional validation: Min should not be greater than Max
        if (valid && hasNextParams) {
          float checkMin = (nextParams.minInterval > 0) ? nextParams.minInterval : storedParams.minInterval;
          float checkMax = (nextParams.maxInterval > 0) ? nextParams.maxInterval : storedParams.maxInterval;

          if (checkMin > checkMax) {
            valid = false;
            errorMsg = "Connection Interval Min cannot be greater than Max";
          }
        }

        if (params.containsKey("peripheralLatency")) {
          int val = params["peripheralLatency"];
          if (val < 0 || val > 499) {
            valid = false;
            errorMsg = "Peripheral Latency must be between 0 and 499";
          } else {
            nextParams.slaveLatency = val;
            hasNextParams = true;
          }
        }

        if (params.containsKey("supervisionTimeout")) {
          float val = params["supervisionTimeout"];
          if (val < 100 || val > 32000) {
            valid = false;
            errorMsg = "Supervision Timeout must be between 100 and 32000 ms";
          } else {
            nextParams.supervisionTimeout = val;
            hasNextParams = true;
          }
        }

        if (valid) {
          DynamicJsonDocument response(256);
          response["type"] = "parameter_update_success";
          String responseStr;
          serializeJson(response, responseStr);
          webSocket.sendTXT(num, responseStr);
          broadcastState();
        } else {
          DynamicJsonDocument response(256);
          response["type"] = "parameter_update_error";
          response["error"] = errorMsg;
          String responseStr;
          serializeJson(response, responseStr);
          webSocket.sendTXT(num, responseStr);
        }
      }
      else if (doc["type"] == "security_command") {
        String command = doc["command"];

        if (command == "toggle_auth") {
          securityStatus.requireAuthentication = !securityStatus.requireAuthentication;

          DynamicJsonDocument response(256);
          response["type"] = "security_response";
          response["command"] = "toggle_auth";
          response["requireAuthentication"] = securityStatus.requireAuthentication;
          response["success"] = true;

          String responseStr;
          serializeJson(response, responseStr);
          webSocket.sendTXT(num, responseStr);
          broadcastSecurityStatus();

        } else if (command == "remove_device") {
          String address = doc["address"];
          removePairedDevice(address);

          DynamicJsonDocument response(256);
          response["type"] = "security_response";
          response["command"] = "remove_device";
          response["success"] = true;

          String responseStr;
          serializeJson(response, responseStr);
          webSocket.sendTXT(num, responseStr);
          broadcastSecurityStatus();

        } else if (command == "clear_all_devices") {
          pairedDeviceCount = 0;
          savePairedDevices();
          securityStatus.pairedDeviceCount = 0;

          DynamicJsonDocument response(256);
          response["type"] = "security_response";
          response["command"] = "clear_all_devices";
          response["success"] = true;

          String responseStr;
          serializeJson(response, responseStr);
          webSocket.sendTXT(num, responseStr);
          broadcastSecurityStatus();
        }
      }
      else if (doc["type"] == "apply_parameters") {
        Serial.println("WebSocket: Applying parameters to BLE connection");
        
        if (!hasNextParams) {
          DynamicJsonDocument response(256);
          response["type"] = "apply_parameters_error";
          response["error"] = "No parameters to apply";
          String responseStr;
          serializeJson(response, responseStr);
          webSocket.sendTXT(num, responseStr);
        } else if (!isConnected) {
          DynamicJsonDocument response(256);
          response["type"] = "apply_parameters_error";
          response["error"] = "No BLE device connected";
          String responseStr;
          serializeJson(response, responseStr);
          webSocket.sendTXT(num, responseStr);
        } else {
          // Apply parameters using existing logic
          storedParams = nextParams;
          saveStoredParams();
          hasNextParams = false;
          
          // Apply to BLE connection
          uint16_t minUnits = (uint16_t)(storedParams.minInterval / 1.25);
          uint16_t maxUnits = (uint16_t)(storedParams.maxInterval / 1.25);
          uint16_t timeoutUnits = (uint16_t)(storedParams.supervisionTimeout / 10.0);
          
          Serial.printf("Sending BLE updateConnParams: min=%u, max=%u, latency=%d, timeout=%u\n", 
                        minUnits, maxUnits, storedParams.slaveLatency, timeoutUnits);
          
          pClient->updateConnParams(minUnits, maxUnits, storedParams.slaveLatency, timeoutUnits);
          
          // Send success response
          DynamicJsonDocument response(256);
          response["type"] = "apply_parameters_success";
          response["message"] = "Parameters applied to BLE connection";
          String responseStr;
          serializeJson(response, responseStr);
          webSocket.sendTXT(num, responseStr);
          
          // Broadcast updated state
          broadcastState();
        }
      }
      else if (doc["type"] == "clear_parameters") {
        Serial.println("WebSocket: Clearing next parameters");
        hasNextParams = false;
        nextParams = {0, 0, 0, 0};
        
        DynamicJsonDocument response(256);
        response["type"] = "clear_parameters_success";
        response["message"] = "Next parameters cleared";
        String responseStr;
        serializeJson(response, responseStr);
        webSocket.sendTXT(num, responseStr);
        
        broadcastState();
      }
      break;
    }

    default:
      break;
  }
}

void setupWebSocket() {
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket server started on port 81");
}

void handleApiState() {
  // Add CORS headers
  webServer.sendHeader("Access-Control-Allow-Origin", "*");

  DynamicJsonDocument doc(1024);

  // Parameters
  JsonObject params = doc.createNestedObject("parameters");

  // Current parameters
  JsonObject current = params.createNestedObject("current");
  current["connectionIntervalMin"] = storedParams.minInterval;
  current["connectionIntervalMax"] = storedParams.maxInterval;
  current["peripheralLatency"] = storedParams.slaveLatency;
  current["supervisionTimeout"] = storedParams.supervisionTimeout;

  // Next parameters (if any)
  if (hasNextParams) {
    JsonObject next = params.createNestedObject("next");
    next["connectionIntervalMin"] = nextParams.minInterval;
    next["connectionIntervalMax"] = nextParams.maxInterval;
    next["peripheralLatency"] = nextParams.slaveLatency;
    next["supervisionTimeout"] = nextParams.supervisionTimeout;
  } else {
    params["next"] = nullptr;
  }

  params["previous"] = nullptr;

  // Status
  JsonObject status = doc.createNestedObject("status");
  status["isAdvertising"] = false; // Client mode
  status["isConnected"] = isConnected;
  status["connectedDeviceName"] = isConnected ? connectedDeviceName.c_str() : nullptr;
  status["browserConnected"] = true;

  String response;
  serializeJson(doc, response);
  webServer.send(200, "application/json", response);
}

void handleApiHistory() {
  // Add CORS headers
  webServer.sendHeader("Access-Control-Allow-Origin", "*");

  DynamicJsonDocument doc(4096);
  JsonArray history = doc.to<JsonArray>();

  // Send last 50 history entries
  int count = historyCount;
  int startIdx = (historyIndex - count + 50) % 50;

  for (int i = 0; i < count; i++) {
    int idx = (startIdx + i) % 50;
    JsonObject entry = history.createNestedObject();
    entry["id"] = idx;
    entry["connectionIntervalMin"] = paramHistory[idx].connectionIntervalMin;
    entry["connectionIntervalMax"] = paramHistory[idx].connectionIntervalMax;
    entry["peripheralLatency"] = paramHistory[idx].peripheralLatency;
    entry["supervisionTimeout"] = paramHistory[idx].supervisionTimeout;
    entry["appliedAt"] = (unsigned long long)paramHistory[idx].timestamp * 1000 + paramHistory[idx].millisOffset;
    entry["source"] = paramHistory[idx].source;
  }

  String response;
  serializeJson(doc, response);
  webServer.send(200, "application/json", response);
}

void handleApiPresets() {
  // Add CORS headers
  webServer.sendHeader("Access-Control-Allow-Origin", "*");

  DynamicJsonDocument doc(2048);
  JsonArray presetsArray = doc.to<JsonArray>();

  for (int i = 0; i < presetCount; i++) {
    JsonObject preset = presetsArray.createNestedObject();
    preset["id"] = i;
    preset["name"] = presets[i].name;
    preset["description"] = presets[i].description;
    preset["connectionIntervalMin"] = presets[i].connectionIntervalMin;
    preset["connectionIntervalMax"] = presets[i].connectionIntervalMax;
    preset["peripheralLatency"] = presets[i].peripheralLatency;
    preset["supervisionTimeout"] = presets[i].supervisionTimeout;
  }

  String response;
  serializeJson(doc, response);
  webServer.send(200, "application/json", response);
}

void handleApiWifiStatus() {
  // Add CORS headers
  webServer.sendHeader("Access-Control-Allow-Origin", "*");

  DynamicJsonDocument doc(1024);
  
  // Current WiFi status
  doc["mode"] = (WiFi.getMode() == WIFI_STA) ? "STA" : 
                (WiFi.getMode() == WIFI_AP) ? "AP" : 
                (WiFi.getMode() == WIFI_AP_STA) ? "AP_STA" : "OFF";
  
  if (WiFi.getMode() == WIFI_STA || WiFi.getMode() == WIFI_AP_STA) {
    doc["sta"]["connected"] = (WiFi.status() == WL_CONNECTED);
    if (WiFi.status() == WL_CONNECTED) {
      doc["sta"]["ssid"] = WiFi.SSID();
      doc["sta"]["ip"] = WiFi.localIP().toString();
      doc["sta"]["rssi"] = WiFi.RSSI();
      doc["sta"]["mac"] = WiFi.macAddress();
    }
    doc["sta"]["status"] = WiFi.status();
  }
  
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    doc["ap"]["active"] = true;
    doc["ap"]["ssid"] = WiFi.softAPSSID();
    doc["ap"]["ip"] = WiFi.softAPIP().toString();
    doc["ap"]["clients"] = WiFi.softAPgetStationNum();
    doc["ap"]["mac"] = WiFi.softAPmacAddress();
  }
  
  // Stored credentials info (without passwords)
  preferences.begin("ble_cfg", true);
  String stored_ssid = preferences.getString("ssid", "");
  String last_ip = preferences.getString("last_ip", "");
  String last_error = preferences.getString("last_error", "");
  unsigned long long last_connect = preferences.getULong64("last_connect", 0);
  preferences.end();
  
  doc["stored"]["hasCredentials"] = (stored_ssid.length() > 0);
  if (stored_ssid.length() > 0) {
    doc["stored"]["ssid"] = stored_ssid;
    doc["stored"]["lastSuccessfulIP"] = last_ip;
    doc["stored"]["lastConnectTime"] = last_connect;
    doc["stored"]["lastError"] = last_error;
  }
  
  doc["uptime"] = millis();
  
  String response;
  serializeJson(doc, response);
  webServer.send(200, "application/json", response);
}

void handleApiDashboard() {
  // Add CORS headers
  webServer.sendHeader("Access-Control-Allow-Origin", "*");
  
  DynamicJsonDocument doc(1024);
  
  // Determine network mode and generate appropriate dashboard URLs
  bool isAPMode = (WiFi.getMode() == WIFI_MODE_AP || WiFi.getMode() == WIFI_MODE_APSTA);
  bool isConnected = (WiFi.status() == WL_CONNECTED);
  
  doc["esp32IP"] = isAPMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  doc["networkMode"] = isAPMode ? "AP" : "STA";
  doc["wsEndpoint"] = "ws://" + doc["esp32IP"].as<String>() + ":81";
  
  if (isAPMode) {
    doc["networkName"] = WiFi.softAPSSID();
    doc["networkType"] = "Access Point";
    // AP mode dashboard URLs
    JsonArray dashboardURLs = doc.createNestedArray("dashboardURLs");
    dashboardURLs.add("http://192.168.4.2:5173/");
    dashboardURLs.add("http://192.168.4.3:5173/");
    dashboardURLs.add("http://192.168.4.100:5173/");
  } else if (isConnected) {
    doc["networkName"] = WiFi.SSID();
    doc["networkType"] = "Home WiFi";
    doc["signal"] = WiFi.RSSI();
    
    // Generate smart dashboard URLs based on current IP
    String esp32IP = WiFi.localIP().toString();
    String ipBase = esp32IP;
    int lastDot = ipBase.lastIndexOf('.');
    if (lastDot > 0) {
      ipBase = ipBase.substring(0, lastDot);
      
      JsonArray dashboardURLs = doc.createNestedArray("dashboardURLs");
      dashboardURLs.add("http://" + ipBase + ".5:5173/");
      dashboardURLs.add("http://" + ipBase + ".1:5173/");
      dashboardURLs.add("http://" + ipBase + ".100:5173/");
      dashboardURLs.add("http://" + ipBase + ".101:5173/");
    }
  }
  
  doc["instructions"] = "Run 'npm run dev -- --host' then try the dashboard URLs";
  doc["timestamp"] = millis();
  
  String response;
  serializeJson(doc, response);
  webServer.send(200, "application/json", response);
}

// Helper function to validate connection parameter combination
bool validateParameterCombination(const ConnectionParams& params, String& errorMsg) {
  // Check Min <= Max
  if (params.minInterval > params.maxInterval) {
    errorMsg = "Min interval cannot be greater than Max interval";
    return false;
  }

  // Check supervision timeout vs connection interval (BLE spec requirement)
  // Supervision timeout must be larger than connection interval * (1 + latency) * 2
  float minSupervisionTime = params.maxInterval * (1 + params.slaveLatency) * 2;
  if (params.supervisionTimeout <= minSupervisionTime) {
    errorMsg = "Supervision timeout too small for connection interval and latency combination";
    return false;
  }

  // Additional BLE spec validation
  if (params.slaveLatency > 499) {
    errorMsg = "Slave latency cannot exceed 499";
    return false;
  }

  if (params.supervisionTimeout > 32000) {
    errorMsg = "Supervision timeout cannot exceed 32000ms";
    return false;
  }

  return true;
}

void handleApiApplyParameters() {
  // Add CORS headers
  webServer.sendHeader("Access-Control-Allow-Origin", "*");

  if (hasNextParams) {
    // Store original parameters in case we need to rollback
    ConnectionParams originalParams = storedParams;

    // Apply next parameters with validation
    if (nextParams.minInterval > 0) storedParams.minInterval = nextParams.minInterval;
    if (nextParams.maxInterval > 0) storedParams.maxInterval = nextParams.maxInterval;
    if (nextParams.slaveLatency >= 0) storedParams.slaveLatency = nextParams.slaveLatency;
    if (nextParams.supervisionTimeout > 0) storedParams.supervisionTimeout = nextParams.supervisionTimeout;

    // Validate final parameter combination
    String validationError = "";
    if (!validateParameterCombination(storedParams, validationError)) {
      // Rollback to original parameters
      storedParams = originalParams;

      DynamicJsonDocument errorDoc(256);
      errorDoc["error"] = validationError;
      String errorResponse;
      serializeJson(errorDoc, errorResponse);
      webServer.send(400, "application/json", errorResponse);

      Serial.println("Parameter validation failed: " + validationError);
      return;
    }    saveStoredParams();
    addToHistory("Web API");

    // Update BLE characteristics
    uint16_t minUnits = ms_to_conn_interval_units(storedParams.minInterval);
    uint16_t maxUnits = ms_to_conn_interval_units(storedParams.maxInterval);
    uint16_t timeoutUnits = ms_to_timeout_units(storedParams.supervisionTimeout);
    uint16_t latency = storedParams.slaveLatency;

    Serial.printf("Preparing connection parameters: Min=%d, Max=%d, Latency=%d, Timeout=%d\n",
                  minUnits, maxUnits, latency, timeoutUnits);

    // If connected, update connection parameters with enhanced error handling
    if (isConnected && pClient && currentConnHandle != 0xffff) {
      Serial.println("Applying new params to connected BLE peripheral");

      // Check if device is authenticated (if required)
      if (securityStatus.requireAuthentication && !isDeviceAuthenticated) {
        Serial.println("WARNING: Authentication required but device not authenticated. Parameters applied to characteristics only.");

        DynamicJsonDocument warningDoc(256);
        warningDoc["warning"] = "Parameters updated in characteristics but not applied to connection - authentication required";
        String warningResponse;
        serializeJson(warningDoc, warningResponse);
        webServer.send(200, "application/json", warningResponse);
      } else {
        // Apply to active connection
        pClient->updateConnParams(minUnits, maxUnits, storedParams.slaveLatency, timeoutUnits);
        lastRequestedParams = storedParams;
        Serial.printf("Successfully requested connection parameter update: Min=%.1f, Max=%.1f, Latency=%d, Timeout=%.1f\n",
                      storedParams.minInterval, storedParams.maxInterval, storedParams.slaveLatency, storedParams.supervisionTimeout);
      }
    } else {
      Serial.println("No active BLE connection - parameters saved to characteristics for next connection");
    }

    // Clear next parameters
    hasNextParams = false;
    nextParams = {0, 0, 0, 0};

    broadcastState();
    broadcastHistory();

    handleApiState(); // Return updated state
  } else {
    webServer.send(400, "application/json", "{\"error\":\"No parameters to apply\"}");
  }
}

void handleApiSecurity() {
  // Add CORS headers
  webServer.sendHeader("Access-Control-Allow-Origin", "*");

  broadcastSecurityStatus(); // This will send via WebSocket

  DynamicJsonDocument doc(2048);
  doc["pairingEnabled"] = securityStatus.pairingEnabled;
  doc["requireAuthentication"] = securityStatus.requireAuthentication;
  doc["pairingMethod"] = securityStatus.pairingMethod;
  doc["isPairing"] = securityStatus.isPairing;
  doc["currentPin"] = securityStatus.currentPin;
  doc["pairedDeviceCount"] = securityStatus.pairedDeviceCount;
  doc["currentDeviceAuthenticated"] = isDeviceAuthenticated;

  // Add paired devices list
  JsonArray devices = doc.createNestedArray("pairedDevices");
  for (int i = 0; i < pairedDeviceCount; i++) {
    JsonObject device = devices.createNestedObject();
    device["address"] = pairedDevices[i].address;
    device["name"] = pairedDevices[i].name;
    device["pairedTime"] = (unsigned long long)pairedDevices[i].pairedTime * 1000;
    device["trusted"] = pairedDevices[i].trusted;
    device["authenticated"] = pairedDevices[i].authenticated;
    device["isCurrentDevice"] = (pairedDevices[i].address == currentConnectedAddress.toString().c_str());
  }

  String response;
  serializeJson(doc, response);
  webServer.send(200, "application/json", response);
}

void handleApiRemoveDevice() {
  // Add CORS headers
  webServer.sendHeader("Access-Control-Allow-Origin", "*");

  if (!webServer.hasArg("address")) {
    webServer.send(400, "application/json", "{\"error\":\"Missing device address\"}");
    return;
  }

  String address = webServer.arg("address");
  removePairedDevice(address);

  webServer.send(200, "application/json", "{\"success\":true}");
  broadcastSecurityStatus();
}

void handleApiToggleAuth() {
  // Add CORS headers
  webServer.sendHeader("Access-Control-Allow-Origin", "*");

  securityStatus.requireAuthentication = !securityStatus.requireAuthentication;

  DynamicJsonDocument response(256);
  response["requireAuthentication"] = securityStatus.requireAuthentication;
  response["success"] = true;

  String responseStr;
  serializeJson(response, responseStr);
  webServer.send(200, "application/json", responseStr);

  broadcastSecurityStatus();
  Serial.printf("Authentication requirement %s\n",
                securityStatus.requireAuthentication ? "enabled" : "disabled");
}// WiFi configuration UI (served in AP fallback)
String wifiPageHtml() {
  String s = "<html><head><meta name=viewport content=width=device-width, initial-scale=1><title>WiFi Config</title></head><body>";
  s += "<h3>WiFi Configuration</h3>";
  s += "<form action=\"/savewifi\" method=\"get\">SSID: <input name=\"ssid\" type=text><br>Password: <input name=\"pass\" type=password><br><input type=submit value=\"Save & Connect\"></form>";
  s += "<p>Available networks (scan results):</p><ul>";
  int n = WiFi.scanNetworks();
  if (n == 0) s += "<li>(no networks found)</li>";
  else {
    for (int i = 0; i < n; ++i) {
      s += "<li>" + String(WiFi.SSID(i)) + " (RSSI " + String(WiFi.RSSI(i)) + ")" + ((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " [OPEN]" : "") + "</li>";
    }
  }
  s += "</ul></body></html>";
  return s;
}

void handleWifiPage() {
  webServer.send(200, "text/html", wifiPageHtml());
}

void handleSaveWifi() {
  if (webServer.hasArg("ssid")) {
    String ss = webServer.arg("ssid");
    String pw = webServer.arg("pass");
    Serial.printf("💾 Saving new WiFi credentials: SSID='%s'\n", ss.c_str());
    
    // Store in preferences
    preferences.begin("ble_cfg", false);
    preferences.putString("ssid", ss);
    preferences.putString("pass", pw);
    preferences.end();

    // Send response with status page
    String html = "<html><head><title>WiFi Configuration</title>"
                 "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                 "<style>body{font-family:Arial,sans-serif;max-width:500px;margin:50px auto;padding:20px;text-align:center}"
                 ".status{padding:20px;margin:20px 0;border-radius:8px;background:#fff3cd;border:1px solid #ffeaa7}"
                 ".success{background:#d4edda;border:1px solid #c3e6cb;color:#155724}"
                 "</style>"
                 "<script>"
                 "let countdown = 10;"
                 "function updateCounter() {"
                 "  document.getElementById('counter').innerText = countdown;"
                 "  if (countdown <= 0) {"
                 "    window.location.href = '/';"
                 "  } else {"
                 "    countdown--;"
                 "    setTimeout(updateCounter, 1000);"
                 "  }"
                 "}"
                 "window.onload = updateCounter;"
                 "</script></head><body>"
                 "<h1>🔄 WiFi Configuration Saved</h1>"
                 "<div class='status'>"
                 "<p>📡 Network: <strong>" + ss + "</strong></p>"
                 "<p>⏳ ESP32 will restart and attempt to connect...</p>"
                 "<p>🔄 Redirecting in <span id='counter'>10</span> seconds...</p>"
                 "</div>"
                 "<h3>📋 Next Steps:</h3>"
                 "<p>1. ESP32 will restart and try to connect to your home WiFi</p>"
                 "<p>2. If successful, find the new IP address on your router</p>"
                 "<p>3. If failed, ESP32 will return to Access Point mode</p>"
                 "<p>4. Check serial monitor for connection status</p>"
                 "</body></html>";
    
    webServer.send(200, "text/html", html);
    
    Serial.println("📡 WiFi credentials saved. Restarting in 3 seconds...");
    Serial.println("🔄 ESP32 will attempt to connect to home WiFi after restart");
    Serial.println("⚠️  If connection fails, ESP32 will return to Access Point mode");
    
    // Schedule restart to apply new WiFi settings
    delay(3000);
    ESP.restart();
  } else {
    webServer.send(400, "text/html", "<h1>Error</h1><p>Missing SSID parameter</p>");
  }
}

// Serve static files for the client
void serveClientFiles() {
  // Serve index.html for all routes that don't start with /api
  webServer.onNotFound([]() {
    if (webServer.uri().startsWith("/api")) {
      webServer.send(404, "application/json", "{\"error\":\"API endpoint not found\"}");
    } else {
      // Generate dynamic dashboard page based on current network mode
      String networkInfo;
      String dashboardLinks;
      String esp32IP;
      String wsEndpoint;
      
      if (WiFi.getMode() == WIFI_MODE_AP || WiFi.getMode() == WIFI_MODE_APSTA) {
        // Access Point mode
        networkInfo = "📡 Network: ESP32-BLE-Control Access Point";
        esp32IP = "192.168.4.1";
        wsEndpoint = "ws://192.168.4.1:81";
        
        dashboardLinks = 
          "<p>Try these React Dashboard links (depending on your PC's IP):</p>"
          "<a href='http://192.168.4.2:5173/' class='button'>Dashboard (IP .2)</a>"
          "<a href='http://192.168.4.3:5173/' class='button'>Dashboard (IP .3)</a>"
          "<a href='http://192.168.4.100:5173/' class='button'>Dashboard (IP .100)</a><br>";
      } else {
        // Station mode (connected to home WiFi)
        String ssid = WiFi.SSID();
        IPAddress localIP = WiFi.localIP();
        esp32IP = localIP.toString();
        wsEndpoint = "ws://" + esp32IP + ":81";
        
        networkInfo = "🏠 Network: Connected to " + ssid;
        
        // Extract network base (e.g., "10.0.0" from "10.0.0.67")
        String ipBase = esp32IP;
        int lastDot = ipBase.lastIndexOf('.');
        if (lastDot > 0) {
          ipBase = ipBase.substring(0, lastDot);
        }
        
        dashboardLinks = 
          "<p>🎯 <strong>Smart Dashboard Links for your network (" + ipBase + ".x):</strong></p>"
          "<a href='http://" + ipBase + ".5:5173/' class='button' target='_blank'>📱 Dashboard (.5)</a>"
          "<a href='http://" + ipBase + ".1:5173/' class='button' target='_blank'>💻 Dashboard (.1)</a>"
          "<a href='http://" + ipBase + ".100:5173/' class='button' target='_blank'>🖥️ Dashboard (.100)</a>"
          "<a href='http://" + ipBase + ".101:5173/' class='button' target='_blank'>📟 Dashboard (.101)</a><br>"
          "<div style='margin:15px 0;padding:10px;background:#fff3cd;border:1px solid #ffeaa7;border-radius:5px;'>"
          "💡 <strong>Quick Start:</strong> Run <code>npm run dev -- --host</code> in your project folder, then click a dashboard link above!"
          "</div>";
      }

      webServer.send(200, "text/html",
        "<html><head><title>ESP32 BLE Dashboard</title>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<style>body{font-family:Arial,sans-serif;max-width:600px;margin:50px auto;padding:20px;background:#f5f5f5}"
        ".card{background:white;padding:20px;border-radius:8px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}"
        ".button{display:inline-block;padding:10px 20px;background:#007bff;color:white;text-decoration:none;border-radius:5px;margin:5px}"
        ".button:hover{background:#0056b3}"
        ".status{padding:10px;margin:10px 0;border-radius:5px}"
        ".success{background:#d4edda;color:#155724;border:1px solid #c3e6cb}"
        ".info{background:#d1ecf1;color:#0c5460;border:1px solid #bee5eb}"
        "</style></head><body>"
        "<div class='card'>"
        "<h1>🚀 ESP32 BLE Configuration Dashboard</h1>"
        "<div class='status success'>✅ ESP32 API Server Running</div>"
        "<div class='status info'>" + networkInfo + "</div>"
        "<h3>Quick Access:</h3>"
        + dashboardLinks +
        "<h3>Manual Access:</h3>"
        "<p>If the above links don't work:</p>"
        "<ol>"
        "<li>Find your PC's IP with: <code>ipconfig</code> (Windows) or <code>ifconfig</code> (Mac/Linux)</li>"
        "<li>Start React server: <code>npm run dev -- --host</code></li>"
        "<li>Open: <code>http://[YOUR-PC-IP]:5173/</code></li>"
        "</ol>"
        "<h3>API Endpoints (Working):</h3>"
        "<ul>"
        "<li><a href='/api/state'>GET /api/state</a> - Current system state</li>"
        "<li><a href='/api/history'>GET /api/history</a> - Parameter history</li>"
        "<li><a href='/api/presets'>GET /api/presets</a> - Available presets ✅</li>"
        "<li><a href='/api/dashboard'>GET /api/dashboard</a> - Smart dashboard URLs 🎯</li>"
        "<li>POST /api/parameters/apply - Apply staged parameters</li>"
        "<li>WebSocket: <code>" + wsEndpoint + "</code> for real-time updates</li>"
        "</ul>"
        "<p><small>ESP32 IP: " + esp32IP + " | Time: " + String(millis()/1000) + "s</small></p>"
        "</div></body></html>");
    }
  });
}

void setupWebServer() {
  // API routes
  webServer.on("/api/state", HTTP_GET, handleApiState);
  webServer.on("/api/history", HTTP_GET, handleApiHistory);
  webServer.on("/api/presets", HTTP_GET, handleApiPresets);
  webServer.on("/api/wifi", HTTP_GET, handleApiWifiStatus);
  webServer.on("/api/dashboard", HTTP_GET, handleApiDashboard);
  webServer.on("/api/parameters/apply", HTTP_POST, handleApiApplyParameters);

  // Security API routes
  webServer.on("/api/security", HTTP_GET, handleApiSecurity);
  webServer.on("/api/security/remove-device", HTTP_POST, handleApiRemoveDevice);
  webServer.on("/api/security/toggle-auth", HTTP_POST, handleApiToggleAuth);

  // WiFi configuration routes
  webServer.on("/wificonfig", HTTP_GET, handleWifiPage);
  webServer.on("/savewifi", HTTP_GET, handleSaveWifi);

  // CORS preflight handlers
  webServer.on("/api/state", HTTP_OPTIONS, []() {
    webServer.sendHeader("Access-Control-Allow-Origin", "*");
    webServer.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    webServer.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    webServer.send(200);
  });

  webServer.on("/api/history", HTTP_OPTIONS, []() {
    webServer.sendHeader("Access-Control-Allow-Origin", "*");
    webServer.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    webServer.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    webServer.send(200);
  });

  webServer.on("/api/presets", HTTP_OPTIONS, []() {
    webServer.sendHeader("Access-Control-Allow-Origin", "*");
    webServer.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    webServer.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    webServer.send(200);
  });

  webServer.on("/api/wifi", HTTP_OPTIONS, []() {
    webServer.sendHeader("Access-Control-Allow-Origin", "*");
    webServer.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    webServer.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    webServer.send(200);
  });

  webServer.on("/api/parameters/apply", HTTP_OPTIONS, []() {
    webServer.sendHeader("Access-Control-Allow-Origin", "*");
    webServer.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    webServer.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    webServer.send(200);
  });

  // Security CORS preflight handlers
  webServer.on("/api/security", HTTP_OPTIONS, []() {
    webServer.sendHeader("Access-Control-Allow-Origin", "*");
    webServer.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    webServer.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    webServer.send(200);
  });

  webServer.on("/api/security/remove-device", HTTP_OPTIONS, []() {
    webServer.sendHeader("Access-Control-Allow-Origin", "*");
    webServer.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    webServer.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    webServer.send(200);
  });

  webServer.on("/api/security/toggle-auth", HTTP_OPTIONS, []() {
    webServer.sendHeader("Access-Control-Allow-Origin", "*");
    webServer.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    webServer.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    webServer.send(200);
  });

  serveClientFiles();
  webServer.begin();
  Serial.println("Web server started on port 80");
}

void setupWiFi() {
  // Load stored WiFi credentials
  preferences.begin("ble_cfg", true);
  String stored_ssid = preferences.getString("ssid", "");
  String stored_pass = preferences.getString("pass", "");
  preferences.end();

  // Try to connect to stored WiFi first
  if (stored_ssid.length() > 0) {
    Serial.printf("🔍 Found stored WiFi credentials for: '%s'\n", stored_ssid.c_str());
    Serial.println("📶 Attempting to connect to home WiFi...");
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(stored_ssid.c_str(), stored_pass.c_str());
    
    // Wait up to 15 seconds for connection with better feedback
    int attempts = 0;
    int maxAttempts = 30; // 15 seconds
    
    while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
      delay(500);
      if (attempts % 4 == 0) {
        Serial.printf("🔄 Connection attempt %d/%d...\n", (attempts/4) + 1, maxAttempts/4);
      } else {
        Serial.print(".");
      }
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      IPAddress localIP = WiFi.localIP();
      Serial.println("\n✅ Connected to home WiFi successfully!");
      Serial.printf("📡 Network: %s\n", stored_ssid.c_str());
      Serial.printf("🌐 IP Address: %s\n", localIP.toString().c_str());
      Serial.printf("💻 Web Dashboard: http://%s/\n", localIP.toString().c_str());
      Serial.printf("📊 WebSocket: ws://%s:81\n", localIP.toString().c_str());
      Serial.printf("🔧 WiFi Config: http://%s/wificonfig\n", localIP.toString().c_str());
      Serial.printf("🏠 Both ESP32 and your PC are now on the same network!\n");
      Serial.printf("🚀 Start React dev server with: npm run dev -- --host\n");
      Serial.printf("📱 Then open: http://localhost:5173 or http://%s:5173\n", localIP.toString().c_str());
      
      // Store successful connection info for troubleshooting
      preferences.begin("ble_cfg", false);
      preferences.putString("last_ip", localIP.toString());
      preferences.putULong64("last_connect", millis());
      preferences.end();
      
      return;
    } else {
      Serial.printf("\n❌ Failed to connect to '%s' (Reason: %d)\n", stored_ssid.c_str(), WiFi.status());
      Serial.println("💡 Common issues:");
      Serial.println("   - Incorrect password");
      Serial.println("   - Network out of range");
      Serial.println("   - Router temporarily unavailable");
      Serial.println("🔄 Falling back to Access Point mode...");
      
      // Clear failed credentials to prevent boot loops
      preferences.begin("ble_cfg", false);
      preferences.putString("last_error", "Connection failed - code " + String(WiFi.status()));
      preferences.end();
    }
  } else {
    Serial.println("🔍 No stored WiFi credentials found");
    Serial.println("🔄 Starting in Access Point mode...");
  }

  // Fallback to Access Point mode
  WiFi.mode(WIFI_AP);

  // Create a unique AP name based on MAC address
  uint64_t mac = ESP.getEfuseMac();
  String apName = "ESP32-BLE-Control-" + String((uint32_t)(mac & 0xFFFFFF), HEX);

  // Start Access Point with a simple password
  const char* apPassword = "esp32ble";

  Serial.printf("Starting Access Point: '%s'\n", apName.c_str());
  Serial.printf("Password: '%s'\n", apPassword);

  bool result = WiFi.softAP(apName.c_str(), apPassword);
  if (result) {
    IPAddress apIP = WiFi.softAPIP();
    Serial.println("✅ Access Point started successfully!");
    Serial.printf("📡 Network Name: %s\n", apName.c_str());
    Serial.printf("🔐 Password: %s\n", apPassword);
    Serial.printf("🌐 IP Address: %s\n", apIP.toString().c_str());
    Serial.printf("💻 Web Dashboard: http://%s/\n", apIP.toString().c_str());
    Serial.printf("📊 WebSocket: ws://%s:81\n", apIP.toString().c_str());
    Serial.printf("🔧 WiFi Setup: http://%s/wificonfig\n", apIP.toString().c_str());
    Serial.println("📱 Connect your device to this network, then configure WiFi settings");
  } else {
    Serial.println("❌ Failed to start Access Point!");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nStarting BLE Peripheral + WebServer (ESP32-S3)");

  // Load previously saved parameters
  loadStoredParams();

  // Setup BLE peripheral
  setupBLE();

  // Setup WiFi, webserver, and websocket
  setupWiFi();
  setupWebServer();
  setupWebSocket();

  // Add initial history entry
  addToHistory("startup");

  // print stored params
  Serial.printf("Stored params: min=%.2f max=%.2f latency=%d timeout=%.0f\n",
                storedParams.minInterval, storedParams.maxInterval, storedParams.slaveLatency, storedParams.supervisionTimeout);
}

void loop() {
  webServer.handleClient();
  webSocket.loop();

  // BLE Client scanning and connection management
  if (doScan && !isConnected) {
    Serial.println("Starting BLE scan for peripherals...");
    pScan->start(5, false); // Scan for 5 seconds, don't continue previous scan
    doScan = false;
    doConnect = true; // Ready to connect when device found
  }

  // Check if we need to apply next parameters with retry limit
  static unsigned long lastParamCheck = 0;
  static int paramRetryCount = 0;
  const int MAX_PARAM_RETRIES = 3;
  
  if (hasNextParams && millis() - lastParamCheck > 3000) {
    lastParamCheck = millis();
    Serial.println("Auto-applying next parameters after 3 seconds...");
    
    // Store current hasNextParams state to detect if apply failed
    bool hadNextParams = hasNextParams;
    handleApiApplyParameters();
    
    // If parameters still pending after apply attempt, increment retry count
    if (hasNextParams && hadNextParams) {
      paramRetryCount++;
      Serial.printf("Parameter apply attempt %d/%d\n", paramRetryCount, MAX_PARAM_RETRIES);
      
      // If max retries reached, clear the pending parameters to prevent infinite loop
      if (paramRetryCount >= MAX_PARAM_RETRIES) {
        Serial.println("WARNING: Max parameter apply retries reached, clearing pending parameters");
        hasNextParams = false;
        paramRetryCount = 0;
        // Clear the invalid nextParams
        nextParams = {};
      }
    } else {
      // Success or cleared, reset retry count
      paramRetryCount = 0;
    }
  }

  // Periodic status logging: report last requested params and connection state
  unsigned long now = millis();
  if (now - lastStatusMs > 5000) {
    lastStatusMs = now;
    Serial.print("Status: ");
    Serial.printf("connected=%s, connHandle=%u, authenticated=%s, paired_devices=%d, ",
                  isConnected ? "true" : "false", currentConnHandle,
                  isDeviceAuthenticated ? "true" : "false", pairedDeviceCount);
    Serial.printf("stored[min=%.2fms,max=%.2fms,sto=%.0fms,lat=%d], lastRequested[min=%.2fms,max=%.2fms,sto=%.0fms,lat=%d]\n",
                  storedParams.minInterval, storedParams.maxInterval, storedParams.supervisionTimeout, storedParams.slaveLatency,
                  lastRequestedParams.minInterval, lastRequestedParams.maxInterval, lastRequestedParams.supervisionTimeout, lastRequestedParams.slaveLatency);

    // Broadcast current state periodically
    broadcastState();
  }

  // Restart scanning if disconnected and not already scanning
  if (!isConnected && !doScan && !doConnect && millis() - lastStatusMs > 10000) {
    Serial.println("No connection - restarting scan...");
    doScan = true;
  }

  delay(10);
}