#include <Arduino.h>
#include <NimBLEDevice.h>
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

// Globals
Preferences preferences;
ConnectionParams storedParams;
ConnectionParams previousParams;       // Track previous values for comparison
ConnectionParams lastRequestedParams;  // last requested (what we sent in updateConnParams)
ConnectionParams nextParams = {0, 0, 0, 0}; // Parameters to be applied next
bool hasNextParams = false;
bool hasPreviousParams = false;

NimBLEServer* pServer = nullptr;
NimBLEService* pService = nullptr;
NimBLECharacteristic* pMinCharacteristic = nullptr;
NimBLECharacteristic* pMaxCharacteristic = nullptr;
NimBLECharacteristic* pTimeoutCharacteristic = nullptr;
NimBLECharacteristic* pLatencyCharacteristic = nullptr;

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

bool deviceConnected = false;
uint16_t currentConnHandle = 0xffff;  // invalid

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
class ServerCallbacks : public NimBLEServerCallbacks {
public:
  void onConnect(NimBLEServer* server, ble_gap_conn_desc* desc) override {
    deviceConnected = true;
    currentConnHandle = desc->conn_handle;
    Serial.printf("BLE Client connected (conn handle %u)\n", currentConnHandle);

    // Immediately request connection parameter update using stored values
    Serial.printf("Requesting conn params: min=%.2f max=%.2f latency=%d timeout=%.0f\n",
                  storedParams.minInterval, storedParams.maxInterval, storedParams.slaveLatency, storedParams.supervisionTimeout);
    uint16_t minUnits = ms_to_conn_interval_units(storedParams.minInterval);
    uint16_t maxUnits = ms_to_conn_interval_units(storedParams.maxInterval);
    uint16_t timeoutUnits = ms_to_timeout_units(storedParams.supervisionTimeout);
    server->updateConnParams(currentConnHandle, minUnits, maxUnits, storedParams.slaveLatency, timeoutUnits);
    // record last requested
    lastRequestedParams = storedParams;
    broadcastState();
  }

  void onDisconnect(NimBLEServer* server, ble_gap_conn_desc* desc) override {
    (void)server;
    (void)desc;
    deviceConnected = false;
    currentConnHandle = 0xffff;
    Serial.println("BLE Client disconnected");
    broadcastState();
  }
};

// Characteristic callbacks: write updates storedParams and save
class ParamCharCallbacks : public NimBLECharacteristicCallbacks {
public:
  void onWrite(NimBLECharacteristic* pCharacteristic) override {
    std::string v = pCharacteristic->getValue();
    const char* uuid = pCharacteristic->getUUID().toString().c_str();
    if (v.size() != sizeof(uint16_t)) {
      Serial.println("Invalid write size");
      return;
    }
    if (v.size() >= 2) {
      uint16_t val = (uint8_t)v[1];
      val = (val << 8) | (uint8_t)v[0];

      if (pCharacteristic == pMinCharacteristic) {
        uint16_t units = ms_to_conn_interval_units(storedParams.minInterval);
        storedParams.minInterval = conn_interval_units_to_ms(val);
        Serial.printf("Characteristic write: minInterval = %.2f ms\n", storedParams.minInterval);
      } else if (pCharacteristic == pMaxCharacteristic) {
        uint16_t units = ms_to_conn_interval_units(storedParams.maxInterval);
        storedParams.maxInterval = conn_interval_units_to_ms(val);
        Serial.printf("Characteristic write: maxInterval = %.2f ms\n", storedParams.maxInterval);
      } else if (pCharacteristic == pTimeoutCharacteristic) {
        uint16_t units = ms_to_timeout_units(storedParams.supervisionTimeout);
        storedParams.supervisionTimeout = timeout_units_to_ms(val);
        Serial.printf("Characteristic write: supervisionTimeout = %.0f ms\n", storedParams.supervisionTimeout);
      } else if (pCharacteristic == pLatencyCharacteristic) {
        storedParams.slaveLatency = val;
        Serial.printf("Characteristic write: slaveLatency = %d\n", storedParams.slaveLatency);
      }
      saveStoredParams();
      addToHistory("BLE");

      // If a central is connected, immediately request the update
      if (deviceConnected && pServer && currentConnHandle != 0xffff) {
        Serial.println("Device connected: sending updateConnParams with newly saved params");
        uint16_t minUnits = ms_to_conn_interval_units(storedParams.minInterval);
        uint16_t maxUnits = ms_to_conn_interval_units(storedParams.maxInterval);
        uint16_t timeoutUnits = ms_to_timeout_units(storedParams.supervisionTimeout);
        pServer->updateConnParams(currentConnHandle, minUnits, maxUnits, storedParams.slaveLatency, timeoutUnits);
        lastRequestedParams = storedParams;
      }

      broadcastState();
    }
  }
};

// Setup BLE peripheral service and characteristics
void setupBLE() {
  NimBLEDevice::init("BLE_Config_Server");
  // allow for larger than 23 bytes of BLE message.
  NimBLEDevice::setMTU(100);  // or desired value
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  pService = pServer->createService(SERVICE_UUID);

  pMinCharacteristic = pService->createCharacteristic(MIN_INTERVAL_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  pMaxCharacteristic = pService->createCharacteristic(MAX_INTERVAL_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  pTimeoutCharacteristic = pService->createCharacteristic(SUPERVISION_TO_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  pLatencyCharacteristic = pService->createCharacteristic(SLAVE_LATENCY_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);

  ParamCharCallbacks* cb = new ParamCharCallbacks();
  pMinCharacteristic->setCallbacks(cb);
  pMaxCharacteristic->setCallbacks(cb);
  pTimeoutCharacteristic->setCallbacks(cb);
  pLatencyCharacteristic->setCallbacks(cb);

  // Initialize characteristic values from stored params
  uint16_t minUnits = ms_to_conn_interval_units(storedParams.minInterval);
  uint16_t maxUnits = ms_to_conn_interval_units(storedParams.maxInterval);
  uint16_t timeoutUnits = ms_to_timeout_units(storedParams.supervisionTimeout);
  uint16_t latency = storedParams.slaveLatency;

  pMinCharacteristic->setValue((const uint8_t*)&minUnits, sizeof(minUnits));
  pMaxCharacteristic->setValue((const uint8_t*)&maxUnits, sizeof(maxUnits));
  pTimeoutCharacteristic->setValue((const uint8_t*)&timeoutUnits, sizeof(timeoutUnits));
  pLatencyCharacteristic->setValue((const uint8_t*)&latency, sizeof(latency));

  pService->start();

  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  // Add service UUID (will be added in advertising payload)
  pAdvertising->addServiceUUID(SERVICE_UUID);
  // Keep the advertisement payload small to avoid exceeding 31 bytes.
  // Use a short device name and put optional data in the scan response.
  NimBLEAdvertisementData advData;
  advData.setName("BLE_CFG_desktop");  // short name
  pAdvertising->setAdvertisementData(advData);
  pAdvertising->setScanResponseData(advData);
  NimBLEDevice::startAdvertising();

  Serial.println("BLE Config Server Ready and Advertising");
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
  status["isAdvertising"] = !deviceConnected; // Advertising when not connected
  status["isConnected"] = deviceConnected;
  status["connectedDeviceName"] = deviceConnected ? "BLE Device" : nullptr;
  status["browserConnected"] = true; // Always true if we're broadcasting

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
  status["isAdvertising"] = !deviceConnected;
  status["isConnected"] = deviceConnected;
  status["connectedDeviceName"] = deviceConnected ? "BLE Device" : nullptr;
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

void handleApiApplyParameters() {
  // Add CORS headers
  webServer.sendHeader("Access-Control-Allow-Origin", "*");

  if (hasNextParams) {
    // Apply next parameters
    if (nextParams.minInterval > 0) storedParams.minInterval = nextParams.minInterval;
    if (nextParams.maxInterval > 0) storedParams.maxInterval = nextParams.maxInterval;
    if (nextParams.slaveLatency >= 0) storedParams.slaveLatency = nextParams.slaveLatency;
    if (nextParams.supervisionTimeout > 0) storedParams.supervisionTimeout = nextParams.supervisionTimeout;

    saveStoredParams();
    addToHistory("Web API");

    // Update BLE characteristics
    uint16_t minUnits = ms_to_conn_interval_units(storedParams.minInterval);
    uint16_t maxUnits = ms_to_conn_interval_units(storedParams.maxInterval);
    uint16_t timeoutUnits = ms_to_timeout_units(storedParams.supervisionTimeout);
    uint16_t latency = storedParams.slaveLatency;

    pMinCharacteristic->setValue((const uint8_t*)&minUnits, sizeof(minUnits));
    pMaxCharacteristic->setValue((const uint8_t*)&maxUnits, sizeof(maxUnits));
    pTimeoutCharacteristic->setValue((const uint8_t*)&timeoutUnits, sizeof(timeoutUnits));
    pLatencyCharacteristic->setValue((const uint8_t*)&latency, sizeof(latency));

    // If connected, update connection parameters
    if (deviceConnected && pServer && currentConnHandle != 0xffff) {
      Serial.println("Applying new params to connected BLE central");
      pServer->updateConnParams(currentConnHandle, minUnits, maxUnits, storedParams.slaveLatency, timeoutUnits);
      lastRequestedParams = storedParams;
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
    Serial.printf("Saving new WiFi credentials: SSID='%s'\n", ss.c_str());
    // store in preferences
    preferences.begin("ble_cfg", false);
    preferences.putString("ssid", ss);
    preferences.putString("pass", pw);
    preferences.end();

    // attempt to connect
    WiFi.mode(WIFI_STA);
    WiFi.begin(ss.c_str(), pw.c_str());
    Serial.println("Attempting to connect with new credentials...");

    // Give it a short time then redirect to root
    delay(2000);
  }
  webServer.sendHeader("Location", "/");
  webServer.send(302, "text/plain", "");
}

// Serve static files for the client
void serveClientFiles() {
  // Serve index.html for all routes that don't start with /api
  webServer.onNotFound([]() {
    if (webServer.uri().startsWith("/api")) {
      webServer.send(404, "application/json", "{\"error\":\"API endpoint not found\"}");
    } else {
      // Serve a smart redirect page that tries to find the React dev server
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
        "<div class='status info'>📡 Network: ESP32-BLE-Control Access Point</div>"
        "<h3>Quick Access:</h3>"
        "<p>Try these React Dashboard links (depending on your PC's IP):</p>"
        "<a href='http://192.168.4.2:5173/' class='button'>Dashboard (IP .2)</a>"
        "<a href='http://192.168.4.3:5173/' class='button'>Dashboard (IP .3)</a>"
        "<a href='http://192.168.4.100:5173/' class='button'>Dashboard (IP .100)</a><br>"
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
        "<li>POST /api/parameters/apply - Apply staged parameters</li>"
        "<li>WebSocket: <code>ws://192.168.4.1:81</code> for real-time updates</li>"
        "</ul>"
        "<p><small>ESP32 IP: 192.168.4.1 | Time: " + String(millis()/1000) + "s</small></p>"
        "</div></body></html>");
    }
  });
}

void setupWebServer() {
  // API routes
  webServer.on("/api/state", HTTP_GET, handleApiState);
  webServer.on("/api/history", HTTP_GET, handleApiHistory);
  webServer.on("/api/presets", HTTP_GET, handleApiPresets);
  webServer.on("/api/parameters/apply", HTTP_POST, handleApiApplyParameters);

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

  webServer.on("/api/parameters/apply", HTTP_OPTIONS, []() {
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
  // Start directly in Access Point mode for reliable cross-device connectivity
  // This eliminates 2.4GHz vs 5GHz WiFi band issues

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
    Serial.println("📱 Connect your device to this network, then open the IP address in a browser");
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

  // Check if we need to apply next parameters
  static unsigned long lastParamCheck = 0;
  if (hasNextParams && millis() - lastParamCheck > 3000) {
    lastParamCheck = millis();
    Serial.println("Auto-applying next parameters after 3 seconds...");
    handleApiApplyParameters();
  }

  // Periodic status logging: report last requested params and connection state
  unsigned long now = millis();
  if (now - lastStatusMs > 5000) {
    lastStatusMs = now;
    Serial.print("Status: ");
    Serial.printf("connected=%s, connHandle=%u, ", deviceConnected ? "true" : "false", currentConnHandle);
    Serial.printf("stored[min=%.2fms,max=%.2fms,sto=%.0fms,lat=%d], lastRequested[min=%.2fms,max=%.2fms,sto=%.0fms,lat=%d]\n",
                  storedParams.minInterval, storedParams.maxInterval, storedParams.supervisionTimeout, storedParams.slaveLatency,
                  lastRequestedParams.minInterval, lastRequestedParams.maxInterval, lastRequestedParams.supervisionTimeout, lastRequestedParams.slaveLatency);

    // Broadcast current state periodically
    broadcastState();
  }

  delay(10);
}