#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLESecurity.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <esp_mac.h>
#include <vector>

// --- Configuration ---
#ifndef BLE_USB_ONLY_MODE
#define BLE_USB_ONLY_MODE 1
#endif

// BLE_USB_ONLY_MODE=1 disables WiFi/WebServer/WebSocket and uses USB Serial commands.

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

struct DiscoveredBleDevice {
  String address;
  String name;
  int rssi;
  bool connectable;
  unsigned long lastSeenMs;
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
String scanNameFilter = "";
bool scanFilterEnabled = false;
DiscoveredBleDevice discoveredDevices[25];
int discoveredDeviceCount = 0;
String pendingConnectAddress = "";
String discoveredServicesJson = "[]";
bool servicesDiscoveryInProgress = false;
String servicesDiscoveryError = "";
uint16_t negotiatedMtu = 0;
bool servicesDiscoveryPending = false;
unsigned long servicesDiscoveryNextAttemptMs = 0;
int servicesDiscoveryRetryCount = 0;
unsigned long lastScanRestartMs = 0;

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
String usbCommandBuffer = "";

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
void broadcastScanResults();
void broadcastServices();
void discoverConnectedServices(NimBLEClient* client);
void clearDiscoveredServices();
void serveClientFiles();
void handleApiBleFilter();
void handleApiBleScan();
void logMacDiagnostics();

// Security function declarations
void setupBLESecurity();
void loadPairedDevices();
void savePairedDevices();
void addPairedDevice(NimBLEAddress address, String name);
void removePairedDevice(String address);
bool isDevicePaired(NimBLEAddress address);
void broadcastSecurityStatus();
void initializeSecurityStatus();
void clearDiscoveredDevices();
void upsertDiscoveredDevice(NimBLEAdvertisedDevice* advertisedDevice);
bool applyNextParametersInternal(const String& source, String* errorMsg = nullptr, bool* authWarning = nullptr);
void handleUsbSerialInput();
void processUsbCommand(const String& commandLine);
void printUsbCommandHelp();
void sendUsbState();
void sendUsbHistory();
void sendUsbScanResults();
void sendUsbServices();
void sendUsbSecurity();

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

static const char* wifiStatusToString(wl_status_t status) {
  switch (status) {
    case WL_NO_SHIELD:
      return "NO_SHIELD";
    case WL_IDLE_STATUS:
      return "IDLE";
    case WL_NO_SSID_AVAIL:
      return "NO_SSID_AVAILABLE";
    case WL_SCAN_COMPLETED:
      return "SCAN_COMPLETED";
    case WL_CONNECTED:
      return "CONNECTED";
    case WL_CONNECT_FAILED:
      return "CONNECT_FAILED";
    case WL_CONNECTION_LOST:
      return "CONNECTION_LOST";
    case WL_DISCONNECTED:
      return "DISCONNECTED";
    default:
      return "UNKNOWN";
  }
}

static const char* wifiAuthModeToString(wifi_auth_mode_t authMode) {
  switch (authMode) {
    case WIFI_AUTH_OPEN:
      return "OPEN";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA_PSK";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2_PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA_WPA2_PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "WPA2_ENTERPRISE";
    case WIFI_AUTH_WPA3_PSK:
      return "WPA3_PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "WPA2_WPA3_PSK";
    case WIFI_AUTH_WAPI_PSK:
      return "WAPI_PSK";
    default:
      return "UNKNOWN";
  }
}

struct WiFiScanDiagnostic {
  bool found;
  int rssi;
  int channel;
  wifi_auth_mode_t authMode;
  int matches;
};

static WiFiScanDiagnostic scanForTargetNetwork(const String& targetSsid) {
  WiFiScanDiagnostic diagnostic = {false, -127, 0, WIFI_AUTH_OPEN, 0};

  int networkCount = WiFi.scanNetworks();
  for (int i = 0; i < networkCount; ++i) {
    if (WiFi.SSID(i) == targetSsid) {
      diagnostic.matches++;
      if (!diagnostic.found || WiFi.RSSI(i) > diagnostic.rssi) {
        diagnostic.found = true;
        diagnostic.rssi = WiFi.RSSI(i);
        diagnostic.channel = WiFi.channel(i);
        diagnostic.authMode = WiFi.encryptionType(i);
      }
    }
  }

  WiFi.scanDelete();
  return diagnostic;
}

static bool matchesScanFilter(NimBLEAdvertisedDevice* advertisedDevice) {
  if (!scanFilterEnabled || scanNameFilter.length() == 0) {
    return true;
  }

  String advertisedName = advertisedDevice->getName().c_str();
  if (advertisedName.length() == 0) {
    return false;
  }

  String loweredName = advertisedName;
  loweredName.toLowerCase();
  String loweredFilter = scanNameFilter;
  loweredFilter.toLowerCase();
  return loweredName.indexOf(loweredFilter) >= 0;
}

static bool addressesMatch(const String& left, const String& right) {
  String loweredLeft = left;
  String loweredRight = right;
  loweredLeft.toLowerCase();
  loweredRight.toLowerCase();
  return loweredLeft == loweredRight;
}

static bool isAdvertisementConnectable(NimBLEAdvertisedDevice* advertisedDevice) {
  if (advertisedDevice == nullptr) {
    return false;
  }

  // Rely on NimBLE's connectable classification.
  return advertisedDevice->isConnectable();
}

void clearDiscoveredDevices() {
  discoveredDeviceCount = 0;
  broadcastScanResults();
}

void clearDiscoveredServices() {
  discoveredServicesJson = "[]";
  servicesDiscoveryInProgress = false;
  servicesDiscoveryError = "";
  negotiatedMtu = 0;
  broadcastServices();
}

void broadcastServices() {
  if (BLE_USB_ONLY_MODE) {
    return;
  }

  DynamicJsonDocument doc(16384);
  doc["type"] = "ble_services_update";
  doc["connected"] = isConnected;
  doc["inProgress"] = servicesDiscoveryInProgress;
  doc["mtu"] = negotiatedMtu;
  if (servicesDiscoveryError.length() > 0) {
    doc["error"] = servicesDiscoveryError;
  } else {
    doc["error"] = nullptr;
  }
  doc["services"] = serialized(discoveredServicesJson);

  String message;
  serializeJson(doc, message);
  webSocket.broadcastTXT(message);
}

void discoverConnectedServices(NimBLEClient* client) {
  if (client == nullptr || !client->isConnected()) {
    servicesDiscoveryError = "No connected BLE client available for service discovery";
    servicesDiscoveryInProgress = false;
    broadcastServices();
    return;
  }

  servicesDiscoveryInProgress = true;
  servicesDiscoveryError = "";
  negotiatedMtu = client->getMTU();
  broadcastServices();

  DynamicJsonDocument servicesDoc(16384);
  JsonArray services = servicesDoc.to<JsonArray>();

  std::vector<NimBLERemoteService*>* remoteServices = client->getServices(true);
  if (remoteServices == nullptr) {
    servicesDiscoveryError = "Service discovery returned no services";
    discoveredServicesJson = "[]";
    servicesDiscoveryInProgress = false;
    broadcastServices();
    return;
  }

  for (NimBLERemoteService* service : *remoteServices) {
    if (service == nullptr) {
      continue;
    }

    JsonObject serviceObj = services.createNestedObject();
    serviceObj["uuid"] = service->getUUID().toString().c_str();

    JsonArray characteristics = serviceObj.createNestedArray("characteristics");
    std::vector<NimBLERemoteCharacteristic*>* remoteCharacteristics = service->getCharacteristics(true);

    int characteristicCount = 0;
    if (remoteCharacteristics != nullptr) {
      for (NimBLERemoteCharacteristic* characteristic : *remoteCharacteristics) {
        if (characteristic == nullptr) {
          continue;
        }

        JsonObject characteristicObj = characteristics.createNestedObject();
        characteristicObj["uuid"] = characteristic->getUUID().toString().c_str();
        characteristicObj["canRead"] = characteristic->canRead();
        characteristicObj["canWrite"] = characteristic->canWrite();
        characteristicObj["canNotify"] = characteristic->canNotify();
        characteristicObj["canIndicate"] = characteristic->canIndicate();
        characteristicObj["canWriteNoResponse"] = characteristic->canWriteNoResponse();
        characteristicCount++;
      }
    }

    serviceObj["characteristicCount"] = characteristicCount;
  }

  if (servicesDoc.overflowed()) {
    servicesDiscoveryError = "Service discovery data exceeded buffer size";
    discoveredServicesJson = "[]";
  } else {
    serializeJson(servicesDoc, discoveredServicesJson);
  }

  negotiatedMtu = client->getMTU();
  servicesDiscoveryInProgress = false;
  broadcastServices();
}

void upsertDiscoveredDevice(NimBLEAdvertisedDevice* advertisedDevice) {
  if (advertisedDevice == nullptr) {
    return;
  }

  DiscoveredBleDevice device;
  device.address = advertisedDevice->getAddress().toString().c_str();
  device.name = advertisedDevice->getName().c_str();
  if (device.name.length() == 0) {
    device.name = "(unnamed)";
  }
  device.rssi = advertisedDevice->getRSSI();
  device.connectable = isAdvertisementConnectable(advertisedDevice);
  device.lastSeenMs = millis();

  int existingIndex = -1;
  for (int i = 0; i < discoveredDeviceCount; ++i) {
    if (addressesMatch(discoveredDevices[i].address, device.address)) {
      existingIndex = i;
      break;
    }
  }

  if (existingIndex >= 0) {
    if (device.name == "(unnamed)" && discoveredDevices[existingIndex].name.length() > 0) {
      device.name = discoveredDevices[existingIndex].name;
    }

    // Preserve connectable=true once observed for an address so later packets
    // (for example scan responses) do not downgrade it to observer-only.
    device.connectable = discoveredDevices[existingIndex].connectable || device.connectable;

    for (int i = existingIndex; i > 0; --i) {
      discoveredDevices[i] = discoveredDevices[i - 1];
    }
    discoveredDevices[0] = device;
    return;
  }

  int insertLimit = min(discoveredDeviceCount, 24);
  for (int i = insertLimit; i > 0; --i) {
    discoveredDevices[i] = discoveredDevices[i - 1];
  }
  discoveredDevices[0] = device;

  if (discoveredDeviceCount < 25) {
    discoveredDeviceCount++;
  }
}

void broadcastScanResults() {
  if (BLE_USB_ONLY_MODE) {
    return;
  }

  DynamicJsonDocument doc(8192);
  doc["type"] = "ble_scan_results";

  JsonArray devices = doc.createNestedArray("devices");
  for (int i = 0; i < discoveredDeviceCount; ++i) {
    JsonObject device = devices.createNestedObject();
    device["address"] = discoveredDevices[i].address;
    device["name"] = discoveredDevices[i].name;
    device["rssi"] = discoveredDevices[i].rssi;
    device["connectable"] = discoveredDevices[i].connectable;
    device["lastSeenMs"] = discoveredDevices[i].lastSeenMs;
  }

  String message;
  serializeJson(doc, message);
  webSocket.broadcastTXT(message);
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
  if (BLE_USB_ONLY_MODE) {
    return;
  }

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
  
  // Initialize defaults if this is first boot (no params set yet)
  if (!preferences.isKey("minMs")) {
    preferences.putFloat("minMs", storedParams.minInterval);
    preferences.putFloat("maxMs", storedParams.maxInterval);
    preferences.putFloat("stMs", storedParams.supervisionTimeout);
    preferences.putInt("lat", storedParams.slaveLatency);
    Serial.println("[INIT] First boot detected - initialized default BLE parameters");
  }
  
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
    isDeviceAuthenticated = false;
    currentConnHandle = pclient->getConnId();
    connectedDeviceAddress = pclient->getPeerAddress().toString().c_str();
    connectedDeviceName = "BLE Device";

    for (int i = 0; i < discoveredDeviceCount; ++i) {
      if (addressesMatch(discoveredDevices[i].address, connectedDeviceAddress) &&
          discoveredDevices[i].name.length() > 0) {
        connectedDeviceName = discoveredDevices[i].name;
        break;
      }
    }

    negotiatedMtu = pclient->getMTU();
    Serial.printf("Connected to BLE peripheral (conn handle %u, address: %s, mtu=%u)\n",
                  currentConnHandle, connectedDeviceAddress.c_str(), negotiatedMtu);

    // Do not force conn params immediately on connect.
    // Some peripherals disconnect if first request is too aggressive before ATT/GATT settles.
    Serial.println("Skipping immediate conn param update; keeping peripheral defaults for discovery");
    broadcastState();
    broadcastSecurityStatus();

    // Defer GATT discovery to loop context to avoid running heavy ATT/GATT
    // operations from within NimBLE callback context.
    servicesDiscoveryPending = true;
    servicesDiscoveryRetryCount = 0;
    servicesDiscoveryNextAttemptMs = millis() + 300;
    servicesDiscoveryError = "";
    servicesDiscoveryInProgress = false;
    discoveredServicesJson = "[]";
    broadcastServices();

    Serial.println("Queued GATT service discovery after connection stabilization");
  }

  void onDisconnect(NimBLEClient* pclient) override {
    (void)pclient;
    isConnected = false;
    currentConnHandle = 0xffff;
    connectedDeviceAddress = "";
    connectedDeviceName = "None";

    Serial.println("Disconnected from BLE peripheral");
    servicesDiscoveryPending = false;
    servicesDiscoveryRetryCount = 0;
    servicesDiscoveryNextAttemptMs = 0;
    clearDiscoveredServices();
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
    String advertisedAddress = advertisedDevice->getAddress().toString().c_str();
    String advertisedName = advertisedDevice->getName().c_str();
    bool isAdvertisedConnectable = isAdvertisementConnectable(advertisedDevice);
    bool isPendingTarget = !isConnected && doConnect && pendingConnectAddress.length() > 0 &&
                           addressesMatch(advertisedAddress, pendingConnectAddress);
    bool hasAdvertisedName = advertisedName.length() > 0;
    if (advertisedName.length() == 0) {
      advertisedName = "(unnamed)";
    }

    if (!isPendingTarget && !matchesScanFilter(advertisedDevice)) {
      if (hasAdvertisedName) {
        Serial.printf("Ignoring BLE device '%s' because it does not match include filter '%s'\n",
                      advertisedName.c_str(), scanNameFilter.c_str());
      }
      return;
    }

    upsertDiscoveredDevice(advertisedDevice);
    broadcastScanResults();

    if (scanFilterEnabled) {
      Serial.printf("BLE device '%s' matched include filter '%s'\n",
                    advertisedName.c_str(), scanNameFilter.c_str());
    }

    if (!isConnected && doConnect && pendingConnectAddress.length() > 0 &&
        addressesMatch(advertisedAddress, pendingConnectAddress)) {
      pScan->stop();

      Serial.printf("Found requested device: %s (connectable=%s)\n",
                    advertisedDevice->toString().c_str(),
                    isAdvertisedConnectable ? "true" : "false");

      if (!isAdvertisedConnectable) {
        Serial.println("Matched packet is non-connectable (likely scan response); attempting direct address connection");
      }

      // Create client and connect
      pClient = NimBLEDevice::createClient();
      pClient->setClientCallbacks(new ClientCallbacks(), false);

      NimBLEAddress targetAddress(advertisedAddress.c_str());
      bool connectStarted = pClient->connect(targetAddress);

      if (connectStarted) {
        Serial.println("Connected to peripheral");
        doConnect = false;
        pendingConnectAddress = "";
      } else {
        Serial.println("Failed to connect to peripheral; keeping target pending for retry");
        doScan = true; // Resume scanning
      }
    }
  }
};

// Setup BLE Client for connecting to peripherals
void setupBLE() {
  uint8_t btMac[6] = {0};
  String bleClientName = "ESP32_BLE_Client_Manager";
  if (esp_read_mac(btMac, ESP_MAC_BT) == ESP_OK) {
    char nameBuffer[48];
    snprintf(nameBuffer, sizeof(nameBuffer), "ESP32_BLE_Client_%02X%02X", btMac[4], btMac[5]);
    bleClientName = String(nameBuffer);
  }

  NimBLEDevice::init(bleClientName.c_str());
  NimBLEDevice::setMTU(100);  // Allow larger BLE messages
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  logMacDiagnostics();

  // Hard-disable GAP advertising for this client-only firmware.
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  if (pAdvertising != nullptr && pAdvertising->isAdvertising()) {
    pAdvertising->stop();
  }

  NimBLEAddress localAddress = NimBLEDevice::getAddress();
  Serial.printf("BLE client identity: name='%s', address=%s\n",
                bleClientName.c_str(), localAddress.toString().c_str());
  Serial.println("BLE advertising explicitly disabled (client-only mode)");

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

void logMacDiagnostics() {
  uint8_t baseMac[6] = {0};
  uint8_t btMac[6] = {0};

  if (esp_base_mac_addr_get(baseMac) == ESP_OK) {
    Serial.printf("EFUSE base MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
                  baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
  } else {
    Serial.println("EFUSE base MAC: <unavailable>");
  }

  if (esp_read_mac(btMac, ESP_MAC_BT) == ESP_OK) {
    Serial.printf("ESP BT MAC    : %02x:%02x:%02x:%02x:%02x:%02x\n",
                  btMac[0], btMac[1], btMac[2], btMac[3], btMac[4], btMac[5]);
  } else {
    Serial.println("ESP BT MAC    : <unavailable>");
  }
}

void setupBLESecurity() {
  // Initialize security status
  initializeSecurityStatus();

  // Load paired devices
  loadPairedDevices();

  // Configure security parameters.
  // Keep this permissive for compatibility with meters that do not support
  // MITM/SC and may disconnect during strict pairing negotiation.
  NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);

  // Set security callbacks
  NimBLEDevice::setSecurityCallbacks(new SecurityCallbacks());

  Serial.println("BLE Security configured: Bonding (compatibility mode)");
}

// --- WebSocket Functions ---
void broadcastState() {
  if (BLE_USB_ONLY_MODE) {
    return;
  }

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
  status["scanFilterEnabled"] = scanFilterEnabled;
  if (scanFilterEnabled) {
    status["scanFilterName"] = scanNameFilter;
  } else {
    status["scanFilterName"] = nullptr;
  }
  status["isScanning"] = (pScan != nullptr && pScan->isScanning());
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
  if (BLE_USB_ONLY_MODE) {
    return;
  }

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
      broadcastScanResults();
      broadcastServices();
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
      else if (doc["type"] == "ble_command") {
        String command = doc["command"] | "";

        DynamicJsonDocument response(384);
        response["type"] = "ble_command_response";
        response["command"] = command;

        if (command == "set_scan_filter") {
          String deviceName = doc["deviceName"] | "";
          deviceName.trim();

          scanNameFilter = deviceName;
          scanFilterEnabled = scanNameFilter.length() > 0;

          response["success"] = true;
          response["scanFilterEnabled"] = scanFilterEnabled;
          response["scanFilterName"] = scanNameFilter;

          if ((bool)(doc["restartScan"] | true)) {
            if (pScan != nullptr && pScan->isScanning()) {
              pScan->stop();
            }
            clearDiscoveredDevices();
            pendingConnectAddress = "";
            doConnect = false;
            doScan = true;
          }

          Serial.printf("BLE scan filter set to: '%s'\n", scanNameFilter.c_str());
          broadcastState();
        }
        else if (command == "clear_scan_filter") {
          scanNameFilter = "";
          scanFilterEnabled = false;

          response["success"] = true;
          response["scanFilterEnabled"] = false;
          response["scanFilterName"] = "";

          Serial.println("BLE scan filter cleared");
          broadcastState();
        }
        else if (command == "start_scan") {
          if (pScan != nullptr && pScan->isScanning()) {
            pScan->stop();
          }
          clearDiscoveredDevices();
          pendingConnectAddress = "";
          doConnect = false;
          doScan = true;

          response["success"] = true;
          response["scanFilterEnabled"] = scanFilterEnabled;
          response["scanFilterName"] = scanNameFilter;

          Serial.println("BLE scan requested from WebSocket command");
          broadcastState();
        }
        else if (command == "connect_device") {
          String deviceAddress = doc["address"] | "";
          deviceAddress.trim();

          if (deviceAddress.length() == 0) {
            response["success"] = false;
            response["error"] = "Device address is required";
          } else {
            bool foundDevice = false;
            bool deviceIsConnectable = false;
            for (int i = 0; i < discoveredDeviceCount; ++i) {
              if (addressesMatch(discoveredDevices[i].address, deviceAddress)) {
                foundDevice = true;
                deviceIsConnectable = discoveredDevices[i].connectable;
                break;
              }
            }

            if (!foundDevice) {
              response["success"] = false;
              response["error"] = "Device is no longer in scan results. Scan again and retry.";
            } else {
            if (!deviceIsConnectable) {
              response["warning"] = "Selected device is marked observer; attempting connection anyway.";
            }
            pendingConnectAddress = deviceAddress;
            doConnect = true;
            if (pScan != nullptr && !pScan->isScanning()) {
              doScan = true;
            }

            response["success"] = true;
            response["address"] = pendingConnectAddress;
            Serial.printf("BLE connection requested for %s\n", pendingConnectAddress.c_str());
            broadcastState();
            }
          }
        }
        else if (command == "disconnect") {
          if (isConnected && pClient != nullptr) {
            pClient->disconnect();
            pendingConnectAddress = "";
            doConnect = false;
            response["success"] = true;
          } else {
            response["success"] = false;
            response["error"] = "No active BLE connection";
          }
          broadcastState();
        }
        else {
          response["success"] = false;
          response["error"] = "Unknown BLE command";
        }

        String responseStr;
        serializeJson(response, responseStr);
        webSocket.sendTXT(num, responseStr);
      }
      break;
    }

    default:
      break;
  }
}

void setupWebSocket() {
  if (BLE_USB_ONLY_MODE) {
    return;
  }

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
  status["scanFilterEnabled"] = scanFilterEnabled;
  if (scanFilterEnabled) {
    status["scanFilterName"] = scanNameFilter;
  } else {
    status["scanFilterName"] = nullptr;
  }
  status["isScanning"] = (pScan != nullptr && pScan->isScanning());

  String response;
  serializeJson(doc, response);
  webServer.send(200, "application/json", response);
}

void handleApiBleFilter() {
  webServer.sendHeader("Access-Control-Allow-Origin", "*");

  if (webServer.hasArg("name")) {
    scanNameFilter = webServer.arg("name");
    scanNameFilter.trim();
    scanFilterEnabled = scanNameFilter.length() > 0;
  } else if (webServer.hasArg("clear")) {
    scanNameFilter = "";
    scanFilterEnabled = false;
  }

  DynamicJsonDocument doc(256);
  doc["success"] = true;
  doc["scanFilterEnabled"] = scanFilterEnabled;
  doc["scanFilterName"] = scanFilterEnabled ? scanNameFilter : "";

  String response;
  serializeJson(doc, response);
  webServer.send(200, "application/json", response);
  broadcastState();
}

void handleApiBleScan() {
  webServer.sendHeader("Access-Control-Allow-Origin", "*");

  if (pScan != nullptr && pScan->isScanning()) {
    pScan->stop();
  }
  clearDiscoveredDevices();
  pendingConnectAddress = "";
  doConnect = false;
  doScan = true;

  DynamicJsonDocument doc(256);
  doc["success"] = true;
  doc["message"] = "BLE scan started";
  doc["scanFilterEnabled"] = scanFilterEnabled;
  doc["scanFilterName"] = scanFilterEnabled ? scanNameFilter : "";

  String response;
  serializeJson(doc, response);
  webServer.send(200, "application/json", response);
  broadcastState();
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
    doc["sta"]["statusText"] = wifiStatusToString((wl_status_t)WiFi.status());
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
  String last_status_text = preferences.getString("last_status_text", "");
  String last_scan_result = preferences.getString("last_scan_result", "");
  int last_error_code = preferences.getInt("last_error_code", -1);
  unsigned long long last_connect = preferences.getULong64("last_connect", 0);
  preferences.end();
  
  doc["stored"]["hasCredentials"] = (stored_ssid.length() > 0);
  if (stored_ssid.length() > 0) {
    doc["stored"]["ssid"] = stored_ssid;
    doc["stored"]["lastSuccessfulIP"] = last_ip;
    doc["stored"]["lastConnectTime"] = last_connect;
    doc["stored"]["lastError"] = last_error;
    doc["stored"]["lastStatusText"] = last_status_text;
    doc["stored"]["lastScanResult"] = last_scan_result;
    doc["stored"]["lastErrorCode"] = last_error_code;
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

bool applyNextParametersInternal(const String& source, String* errorMsg, bool* authWarning) {
  if (authWarning != nullptr) {
    *authWarning = false;
  }

  if (!hasNextParams) {
    if (errorMsg != nullptr) {
      *errorMsg = "No parameters to apply";
    }
    return false;
  }

  ConnectionParams originalParams = storedParams;

  if (nextParams.minInterval > 0) storedParams.minInterval = nextParams.minInterval;
  if (nextParams.maxInterval > 0) storedParams.maxInterval = nextParams.maxInterval;
  if (nextParams.slaveLatency >= 0) storedParams.slaveLatency = nextParams.slaveLatency;
  if (nextParams.supervisionTimeout > 0) storedParams.supervisionTimeout = nextParams.supervisionTimeout;

  String validationError = "";
  if (!validateParameterCombination(storedParams, validationError)) {
    storedParams = originalParams;
    if (errorMsg != nullptr) {
      *errorMsg = validationError;
    }
    return false;
  }

  saveStoredParams();
  addToHistory(source);

  uint16_t minUnits = ms_to_conn_interval_units(storedParams.minInterval);
  uint16_t maxUnits = ms_to_conn_interval_units(storedParams.maxInterval);
  uint16_t timeoutUnits = ms_to_timeout_units(storedParams.supervisionTimeout);

  if (isConnected && pClient && currentConnHandle != 0xffff) {
    if (securityStatus.requireAuthentication && !isDeviceAuthenticated) {
      if (authWarning != nullptr) {
        *authWarning = true;
      }
    } else {
      pClient->updateConnParams(minUnits, maxUnits, storedParams.slaveLatency, timeoutUnits);
      lastRequestedParams = storedParams;
    }
  }

  hasNextParams = false;
  nextParams = {0, 0, 0, 0};

  broadcastState();
  broadcastHistory();
  return true;
}

void handleApiApplyParameters() {
  // Add CORS headers
  webServer.sendHeader("Access-Control-Allow-Origin", "*");

  String errorMsg = "";
  bool authWarning = false;
  bool success = applyNextParametersInternal("Web API", &errorMsg, &authWarning);

  if (!success) {
    DynamicJsonDocument errorDoc(256);
    errorDoc["error"] = errorMsg;
    String errorResponse;
    serializeJson(errorDoc, errorResponse);
    webServer.send(400, "application/json", errorResponse);
    return;
  }

  if (authWarning) {
    DynamicJsonDocument warningDoc(256);
    warningDoc["warning"] = "Parameters updated but not applied to active connection - authentication required";
    String warningResponse;
    serializeJson(warningDoc, warningResponse);
    webServer.send(200, "application/json", warningResponse);
    return;
  }

  handleApiState();
}

void sendUsbState() {
  DynamicJsonDocument doc(1024);
  doc["type"] = "state";

  JsonObject params = doc.createNestedObject("parameters");
  JsonObject current = params.createNestedObject("current");
  current["connectionIntervalMin"] = storedParams.minInterval;
  current["connectionIntervalMax"] = storedParams.maxInterval;
  current["peripheralLatency"] = storedParams.slaveLatency;
  current["supervisionTimeout"] = storedParams.supervisionTimeout;

  if (hasNextParams) {
    JsonObject next = params.createNestedObject("next");
    next["connectionIntervalMin"] = nextParams.minInterval;
    next["connectionIntervalMax"] = nextParams.maxInterval;
    next["peripheralLatency"] = nextParams.slaveLatency;
    next["supervisionTimeout"] = nextParams.supervisionTimeout;
  } else {
    params["next"] = nullptr;
  }

  JsonObject status = doc.createNestedObject("status");
  status["isConnected"] = isConnected;
  status["connectedDeviceName"] = isConnected ? connectedDeviceName : "";
  status["connectedDeviceAddress"] = isConnected ? connectedDeviceAddress : "";
  status["isScanning"] = (pScan != nullptr && pScan->isScanning());
  status["scanFilterEnabled"] = scanFilterEnabled;
  status["scanFilterName"] = scanFilterEnabled ? scanNameFilter : "";

  String response;
  serializeJson(doc, response);
  Serial.println(response);
}

void sendUsbHistory() {
  DynamicJsonDocument doc(4096);
  doc["type"] = "history_update";
  JsonArray history = doc.createNestedArray("history");

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
    entry["appliedAt"] = (unsigned long long)paramHistory[idx].timestamp * 1000 + paramHistory[idx].millisOffset;
    entry["source"] = paramHistory[idx].source;
  }

  String response;
  serializeJson(doc, response);
  Serial.println(response);
}

void sendUsbScanResults() {
  DynamicJsonDocument doc(8192);
  doc["type"] = "ble_scan_results";
  JsonArray devices = doc.createNestedArray("devices");

  Serial.printf("D sendUsbScanResults: discoveredDeviceCount=%d, filter=%s\n", 
                discoveredDeviceCount, scanFilterEnabled ? scanNameFilter.c_str() : "disabled");
  
  for (int i = 0; i < discoveredDeviceCount; ++i) {
    JsonObject device = devices.createNestedObject();
    device["address"] = discoveredDevices[i].address;
    device["name"] = discoveredDevices[i].name;
    device["rssi"] = discoveredDevices[i].rssi;
    device["connectable"] = discoveredDevices[i].connectable;
    device["lastSeenMs"] = discoveredDevices[i].lastSeenMs;
    Serial.printf("D   [%d] %s : %s (rssi=%d, conn=%d)\n", 
                  i, discoveredDevices[i].address.c_str(), 
                  discoveredDevices[i].name.c_str(), 
                  discoveredDevices[i].rssi, 
                  discoveredDevices[i].connectable);
  }

  String response;
  serializeJson(doc, response);
  Serial.println(response);
}

void sendUsbServices() {
  DynamicJsonDocument doc(16384);
  doc["type"] = "ble_services_update";
  doc["connected"] = isConnected;
  doc["inProgress"] = servicesDiscoveryInProgress;
  doc["mtu"] = negotiatedMtu;
  if (servicesDiscoveryError.length() > 0) {
    doc["error"] = servicesDiscoveryError;
  } else {
    doc["error"] = "";
  }

  if (discoveredServicesJson.length() > 0) {
    DynamicJsonDocument serviceDoc(15360);
    if (deserializeJson(serviceDoc, discoveredServicesJson) == DeserializationError::Ok) {
      JsonArray source = serviceDoc.as<JsonArray>();
      JsonArray target = doc.createNestedArray("services");
      for (JsonVariant value : source) {
        target.add(value);
      }
    } else {
      doc.createNestedArray("services");
    }
  } else {
    doc.createNestedArray("services");
  }

  String response;
  serializeJson(doc, response);
  Serial.println(response);
}

void sendUsbSecurity() {
  DynamicJsonDocument doc(3072);
  doc["type"] = "security_update";

  JsonObject status = doc.createNestedObject("status");
  status["isConnected"] = isConnected;
  status["isAuthenticated"] = isDeviceAuthenticated;
  status["currentDevice"] = isConnected ? connectedDeviceAddress : "";
  status["pairedDeviceCount"] = pairedDeviceCount;
  status["authRequired"] = securityStatus.requireAuthentication;
  status["pairingInProgress"] = securityStatus.isPairing;
  status["currentPin"] = securityStatus.currentPin;

  JsonArray devices = doc.createNestedArray("pairedDevices");
  for (int i = 0; i < pairedDeviceCount; i++) {
    JsonObject device = devices.createNestedObject();
    device["address"] = pairedDevices[i].address;
    device["name"] = pairedDevices[i].name;
    device["pairedAt"] = (unsigned long long)pairedDevices[i].pairedTime * 1000;
  }

  String response;
  serializeJson(doc, response);
  Serial.println(response);
}

void printUsbCommandHelp() {
  Serial.println("USB commands:");
  Serial.println("  HELP");
  Serial.println("  PING");
  Serial.println("  GET_STATE");
  Serial.println("  SCAN_NOW");
  Serial.println("  CONNECT <ble-address>");
  Serial.println("  DISCONNECT");
  Serial.println("  SET_FILTER <name-substring>");
  Serial.println("  CLEAR_FILTER");
  Serial.println("  GET_SCAN_RESULTS");
  Serial.println("  GET_SERVICES");
  Serial.println("  GET_HISTORY");
  Serial.println("  GET_SECURITY");
  Serial.println("  SET_AUTH_REQUIRED <0|1>");
  Serial.println("  REMOVE_PAIRED <ble-address>");
  Serial.println("  SET_NEXT <minMs> <maxMs> <latency> <timeoutMs>");
  Serial.println("  CLEAR_NEXT");
  Serial.println("  APPLY");
}

void processUsbCommand(const String& commandLine) {
  String cmd = commandLine;
  cmd.trim();
  cmd.replace("\r", "");
  cmd.replace("\n", "");

  if (cmd.length() >= 2) {
    if ((cmd.startsWith("\"") && cmd.endsWith("\"")) ||
        (cmd.startsWith("'") && cmd.endsWith("'"))) {
      cmd = cmd.substring(1, cmd.length() - 1);
      cmd.trim();
    }
  }

  if (cmd.length() == 0) {
    return;
  }

  if (cmd.equalsIgnoreCase("HELP")) {
    printUsbCommandHelp();
    return;
  }

  if (cmd.equalsIgnoreCase("PING")) {
    Serial.println("{\"ok\":true,\"reply\":\"PONG\"}");
    return;
  }

  if (cmd.equalsIgnoreCase("GET_STATE") || cmd.startsWith("GET_STATE")) {
    sendUsbState();
    return;
  }

  if (cmd.equalsIgnoreCase("GET_SCAN_RESULTS")) {
    sendUsbScanResults();
    return;
  }

  if (cmd.equalsIgnoreCase("GET_SERVICES")) {
    sendUsbServices();
    return;
  }

  if (cmd.equalsIgnoreCase("GET_HISTORY")) {
    sendUsbHistory();
    return;
  }

  if (cmd.equalsIgnoreCase("GET_SECURITY")) {
    sendUsbSecurity();
    return;
  }

  if (cmd.equalsIgnoreCase("SCAN_NOW")) {
    doScan = true;
    Serial.println("{\"ok\":true,\"message\":\"scan_requested\"}");
    return;
  }

  if (cmd.startsWith("CONNECT ")) {
    String address = cmd.substring(8);
    address.trim();
    if (address.length() == 0) {
      Serial.println("{\"ok\":false,\"error\":\"Missing BLE address\"}");
      return;
    }
    pendingConnectAddress = address;
    doConnect = false;
    doScan = true;
    Serial.println("{\"ok\":true,\"message\":\"connect_requested\"}");
    return;
  }

  if (cmd.equalsIgnoreCase("DISCONNECT")) {
    if (isConnected && pClient != nullptr) {
      pClient->disconnect();
      pendingConnectAddress = "";
      doConnect = false;
      Serial.println("{\"ok\":true,\"message\":\"disconnected\"}");
    } else {
      Serial.println("{\"ok\":false,\"error\":\"No active connection\"}");
    }
    return;
  }

  if (cmd.startsWith("SET_FILTER ")) {
    scanNameFilter = cmd.substring(11);
    scanNameFilter.trim();
    scanFilterEnabled = scanNameFilter.length() > 0;
    clearDiscoveredDevices();
    doScan = true;
    Serial.println("{\"ok\":true,\"message\":\"filter_updated\"}");
    return;
  }

  if (cmd.equalsIgnoreCase("CLEAR_FILTER")) {
    scanNameFilter = "";
    scanFilterEnabled = false;
    clearDiscoveredDevices();
    doScan = true;
    Serial.println("{\"ok\":true,\"message\":\"filter_cleared\"}");
    return;
  }

  if (cmd.startsWith("SET_AUTH_REQUIRED ")) {
    String value = cmd.substring(18);
    value.trim();
    if (value != "0" && value != "1") {
      Serial.println("{\"ok\":false,\"error\":\"Usage: SET_AUTH_REQUIRED <0|1>\"}");
      return;
    }
    securityStatus.requireAuthentication = (value == "1");
    Serial.println("{\"ok\":true,\"message\":\"auth_requirement_updated\"}");
    return;
  }

  if (cmd.startsWith("REMOVE_PAIRED ")) {
    String address = cmd.substring(14);
    address.trim();
    if (address.length() == 0) {
      Serial.println("{\"ok\":false,\"error\":\"Usage: REMOVE_PAIRED <ble-address>\"}");
      return;
    }
    removePairedDevice(address);
    Serial.println("{\"ok\":true,\"message\":\"paired_device_removed\"}");
    return;
  }

  if (cmd.startsWith("SET_NEXT ")) {
    float minMs = 0;
    float maxMs = 0;
    int latency = 0;
    float timeoutMs = 0;
    String args = cmd.substring(9);
    int parsed = sscanf(args.c_str(), "%f %f %d %f", &minMs, &maxMs, &latency, &timeoutMs);
    if (parsed != 4) {
      Serial.println("{\"ok\":false,\"error\":\"Usage: SET_NEXT <minMs> <maxMs> <latency> <timeoutMs>\"}");
      return;
    }
    nextParams.minInterval = minMs;
    nextParams.maxInterval = maxMs;
    nextParams.slaveLatency = latency;
    nextParams.supervisionTimeout = timeoutMs;
    hasNextParams = true;
    Serial.println("{\"ok\":true,\"message\":\"next_parameters_staged\"}");
    return;
  }

  if (cmd.equalsIgnoreCase("CLEAR_NEXT")) {
    hasNextParams = false;
    nextParams = {0, 0, 0, 0};
    Serial.println("{\"ok\":true,\"message\":\"next_parameters_cleared\"}");
    return;
  }

  if (cmd.equalsIgnoreCase("APPLY")) {
    String errorMsg = "";
    bool authWarning = false;
    bool success = applyNextParametersInternal("USB Serial", &errorMsg, &authWarning);
    if (!success) {
      DynamicJsonDocument errorDoc(256);
      errorDoc["ok"] = false;
      errorDoc["error"] = errorMsg;
      String response;
      serializeJson(errorDoc, response);
      Serial.println(response);
      return;
    }
    if (authWarning) {
      Serial.println("{\"ok\":true,\"warning\":\"Parameters stored, authentication required before active apply\"}");
      return;
    }
    Serial.println("{\"ok\":true,\"message\":\"parameters_applied\"}");
    return;
  }

  Serial.println("{\"ok\":false,\"error\":\"Unknown command\"}");
}

void handleUsbSerialInput() {
  while (Serial.available() > 0) {
    char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (usbCommandBuffer.length() > 0) {
        processUsbCommand(usbCommandBuffer);
        usbCommandBuffer = "";
      }
    } else {
      usbCommandBuffer += ch;
      if (usbCommandBuffer.length() > 256) {
        usbCommandBuffer = "";
        Serial.println("{\"ok\":false,\"error\":\"Command too long\"}");
      }
    }
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
  webServer.on("/api/ble/filter", HTTP_POST, handleApiBleFilter);
  webServer.on("/api/ble/scan", HTTP_POST, handleApiBleScan);

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

  webServer.on("/api/ble/filter", HTTP_OPTIONS, []() {
    webServer.sendHeader("Access-Control-Allow-Origin", "*");
    webServer.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    webServer.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    webServer.send(200);
  });

  webServer.on("/api/ble/scan", HTTP_OPTIONS, []() {
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
  Serial.println("[SETUP] setupWiFi() starting...");
  Serial.flush();
  delay(10);
  
  // Load stored WiFi credentials
  preferences.begin("ble_cfg", true);
  String stored_ssid = preferences.getString("ssid", "");
  String stored_pass = preferences.getString("pass", "");
  preferences.end();
  
  Serial.println("[SETUP] Preferences loaded");
  Serial.flush();

  // Try to connect to stored WiFi first
  if (stored_ssid.length() > 0) {
    Serial.printf("🔍 Found stored WiFi credentials for: '%s'\n", stored_ssid.c_str());
    Serial.println("📶 Attempting to connect to home WiFi...");

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true, true);
    delay(250);

    WiFiScanDiagnostic scanDiagnostic = scanForTargetNetwork(stored_ssid);
    String scanSummary;
    if (scanDiagnostic.found) {
      scanSummary = "SSID visible, matches=" + String(scanDiagnostic.matches) +
                    ", RSSI=" + String(scanDiagnostic.rssi) +
                    ", channel=" + String(scanDiagnostic.channel) +
                    ", auth=" + String(wifiAuthModeToString(scanDiagnostic.authMode));
      Serial.printf("📡 WiFi scan match: %s\n", scanSummary.c_str());
    } else {
      scanSummary = "SSID not found in scan results";
      Serial.println("⚠️  Stored SSID was not found in scan results before connect attempt");
    }
    
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
      preferences.putString("last_error", "");
      preferences.putInt("last_error_code", WL_CONNECTED);
      preferences.putString("last_status_text", wifiStatusToString(WL_CONNECTED));
      preferences.putString("last_scan_result", scanSummary);
      preferences.end();
      
      return;
    } else {
      wl_status_t finalStatus = (wl_status_t)WiFi.status();
      String statusText = wifiStatusToString(finalStatus);
      Serial.printf("\n❌ Failed to connect to '%s' (Reason: %d - %s)\n",
                    stored_ssid.c_str(), finalStatus, statusText.c_str());
      Serial.printf("🔎 Scan diagnostics: %s\n", scanSummary.c_str());
      Serial.println("💡 Common issues:");
      Serial.println("   - Incorrect password");
      Serial.println("   - Extender/router security mode incompatibility (WPA3/WPA2 mixed mode)");
      Serial.println("   - Network out of range");
      Serial.println("   - Router temporarily unavailable");
      Serial.println("🔄 Falling back to Access Point mode...");
      
      // Clear failed credentials to prevent boot loops
      preferences.begin("ble_cfg", false);
      preferences.putString("last_error", "Connection failed - code " + String(finalStatus) + " (" + statusText + ")");
      preferences.putInt("last_error_code", finalStatus);
      preferences.putString("last_status_text", statusText);
      preferences.putString("last_scan_result", scanSummary);
      preferences.end();
    }
  } 
  
  Serial.println("[SETUP] No stored credentials or connection failed, starting AP mode");
  Serial.flush();
  delay(10);
  
  // Fallback to Access Point mode (this line was duplicated below)
  if (true) {
    Serial.println("🔍 No stored WiFi credentials found");
    Serial.println("🔄 Starting in Access Point mode...");
  }

  WiFi.mode(WIFI_AP);
  
  Serial.println("[SETUP] WiFi.mode(WIFI_AP) set");
  Serial.flush();
  delay(10);

  // Create a unique AP name based on MAC address
  uint64_t mac = ESP.getEfuseMac();
  String apName = "ESP32-BLE-Control-" + String((uint32_t)(mac & 0xFFFFFF), HEX);
  
  Serial.print("[SETUP] AP name: ");
  Serial.println(apName);
  Serial.flush();
  delay(10);

  // Start Access Point with a simple password
  const char* apPassword = "esp32ble";

  Serial.printf("Starting Access Point: '%s'\n", apName.c_str());
  Serial.flush();
  delay(10);
  Serial.printf("Password: '%s'\n", apPassword);
  Serial.flush();
  delay(10);

  bool result = WiFi.softAP(apName.c_str(), apPassword);
  
  Serial.println("[SETUP] WiFi.softAP() completed");
  Serial.flush();
  delay(10);
  
  if (result) {
    IPAddress apIP = WiFi.softAPIP();
    Serial.println("✅ Access Point started successfully!");
    Serial.flush();
    delay(10);
    Serial.printf("📡 Network Name: %s\n", apName.c_str());
    Serial.printf("🔐 Password: %s\n", apPassword);
    Serial.printf("🌐 IP Address: %s\n", apIP.toString().c_str());
    Serial.printf("💻 Web Dashboard: http://%s/\n", apIP.toString().c_str());
    Serial.printf("📊 WebSocket: ws://%s:81\n", apIP.toString().c_str());
    Serial.printf("🔧 WiFi Setup: http://%s/wificonfig\n", apIP.toString().c_str());
    Serial.println("📱 Connect your device to this network, then configure WiFi settings");
    Serial.flush();
    delay(10);
  } else {
    Serial.println("❌ Failed to start Access Point!");
    Serial.flush();
  }
  
  Serial.println("[SETUP] setupWiFi() completed");
  Serial.flush();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n========================================");
  Serial.println("  ESP32 BLE CLIENT MANAGER");
  Serial.println("  Project : esp32-ble-connection-manager");
  Serial.println("  Source  : src/main.cpp  (CLIENT build)");
  Serial.println("  Role    : BLE Central (scanner/connector)");
  Serial.println("  Built   : " __DATE__ " " __TIME__);
  Serial.println("========================================");
  Serial.println("Starting BLE Client Manager (ESP32-S3)");

  // Load previously saved parameters
  loadStoredParams();

  // Setup BLE client manager (central role)
  setupBLE();

  if (BLE_USB_ONLY_MODE) {
    WiFi.mode(WIFI_OFF);
    Serial.println("USB-only mode enabled: WiFi/WebServer/WebSocket are disabled.");
    printUsbCommandHelp();
  } else {
    // Setup WiFi, webserver, and websocket
    setupWiFi();
    setupWebServer();
    setupWebSocket();
  }

  // Add initial history entry
  addToHistory("startup");

  // print stored params
  Serial.printf("Stored params: min=%.2f max=%.2f latency=%d timeout=%.0f\n",
                storedParams.minInterval, storedParams.maxInterval, storedParams.slaveLatency, storedParams.supervisionTimeout);
}

void loop() {
  handleUsbSerialInput();

  if (!BLE_USB_ONLY_MODE) {
    webServer.handleClient();
    webSocket.loop();
  }

  // Safety watchdog: central firmware must never advertise.
  static unsigned long lastAdvWatchdogCheck = 0;
  if (millis() - lastAdvWatchdogCheck > 1000) {
    lastAdvWatchdogCheck = millis();
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    if (pAdvertising != nullptr && pAdvertising->isAdvertising()) {
      Serial.println("WARNING: advertising became active unexpectedly; forcing stop");
      pAdvertising->stop();
    }
  }

  // BLE Client scanning and connection management
  if (doScan && !isConnected) {
    if (scanFilterEnabled) {
      Serial.printf("Starting BLE scan with name filter: '%s'\n", scanNameFilter.c_str());
    } else {
      Serial.println("Starting BLE scan for peripherals...");
    }
    pScan->start(5, false); // Scan for 5 seconds, don't continue previous scan
    doScan = false;
    doConnect = pendingConnectAddress.length() > 0; // Connect only when a target address was selected
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
    String applyError = "";
    bool authWarning = false;
    bool applied = applyNextParametersInternal("Auto", &applyError, &authWarning);
    if (!applied) {
      Serial.println("Auto-apply failed: " + applyError);
    } else if (authWarning) {
      Serial.println("Auto-apply deferred: authentication required for active connection");
    }
    
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

    if (!BLE_USB_ONLY_MODE) {
      // Broadcast current state periodically
      broadcastState();
    }
  }

  // Run service discovery after connect in loop context.
  if (isConnected && servicesDiscoveryPending && !servicesDiscoveryInProgress &&
      millis() >= servicesDiscoveryNextAttemptMs) {
    Serial.printf("Starting deferred GATT discovery attempt %d/3\n", servicesDiscoveryRetryCount + 1);
    discoverConnectedServices(pClient);

    bool hasServices = discoveredServicesJson.length() > 2;
    if (!hasServices && servicesDiscoveryRetryCount < 2) {
      servicesDiscoveryRetryCount++;
      servicesDiscoveryNextAttemptMs = millis() + 1200;
      Serial.printf("GATT discovery empty, scheduling retry %d/3\n", servicesDiscoveryRetryCount + 1);
    } else {
      servicesDiscoveryPending = false;
      if (!hasServices) {
        Serial.println("GATT discovery completed with no services");
      } else {
        Serial.println("GATT discovery completed with services");
      }
    }
  }

  // Restart scanning periodically while disconnected.
  // Keep this active even with a pending connect request so a selected device can be found again.
  if (!isConnected && !doScan && millis() - lastScanRestartMs > 10000) {
    if (pendingConnectAddress.length() > 0) {
      Serial.printf("Pending BLE target %s - restarting scan...\n", pendingConnectAddress.c_str());
    } else {
      Serial.println("No connection - restarting scan...");
    }
    doScan = true;
    lastScanRestartMs = millis();
  }

  delay(10);
}