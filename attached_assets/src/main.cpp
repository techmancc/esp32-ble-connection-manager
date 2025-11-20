#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>

//const char* dbPath = "/data/hvac.db";  // adjust path as needed
//const char* createTableSQL = "CREATE TABLE IF NOT EXISTS temps (timestamp TEXT, value REAL);";

// --- Configuration ---
// Edit these with your network before building (or change in code at runtime)
const char* SSID = "PrincessAndMoana-2.4G-ext";  // <-- update
const char* PASSWORD = "0urC@tCalli3_1";         // <-- update

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
  uint16_t minInterval;         // 1.25ms units
  uint16_t maxInterval;         // 1.25ms units
  uint16_t supervisionTimeout;  // 10ms units
  uint16_t slaveLatency;        // count
};

// Globals
Preferences preferences;
ConnectionParams storedParams;
ConnectionParams lastRequestedParams;  // last requested (what we sent in updateConnParams)

NimBLEServer* pServer = nullptr;
NimBLEService* pService = nullptr;
NimBLECharacteristic* pMinCharacteristic = nullptr;
NimBLECharacteristic* pMaxCharacteristic = nullptr;
NimBLECharacteristic* pTimeoutCharacteristic = nullptr;
NimBLECharacteristic* pLatencyCharacteristic = nullptr;

WebServer webServer(80);

// AP fallback SSID prefix
const char* AP_SSID_PREFIX = "ESP_desk_";

bool deviceConnected = false;
uint16_t currentConnHandle = 0xffff;  // invalid

unsigned long lastStatusMs = 0;

// Forward declarations
void loadStoredParams();
void saveStoredParams();
void setupBLE();
void setupWebServer();
void setupWiFi();

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

// Save / Load
void loadStoredParams() {
  preferences.begin("ble_cfg", false);
  storedParams.minInterval = preferences.getUShort("min", DEFAULT_MIN_CONN_INTERVAL);
  storedParams.maxInterval = preferences.getUShort("max", DEFAULT_MAX_CONN_INTERVAL);
  storedParams.supervisionTimeout = preferences.getUShort("sto", DEFAULT_SUPERVISION_TIMEOUT);
  storedParams.slaveLatency = preferences.getUShort("lat", DEFAULT_SLAVE_LATENCY);
  preferences.end();

  // copy to lastRequested as initial
  lastRequestedParams = storedParams;
}

void saveStoredParams() {
  preferences.begin("ble_cfg", false);
  preferences.putUShort("min", storedParams.minInterval);
  preferences.putUShort("max", storedParams.maxInterval);
  preferences.putUShort("sto", storedParams.supervisionTimeout);
  preferences.putUShort("lat", storedParams.slaveLatency);
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
    Serial.printf("Requesting conn params: min=%u max=%u latency=%u timeout=%u\n",
                  storedParams.minInterval, storedParams.maxInterval, storedParams.slaveLatency, storedParams.supervisionTimeout);
    server->updateConnParams(currentConnHandle,
                             storedParams.minInterval,
                             storedParams.maxInterval,
                             storedParams.slaveLatency,
                             storedParams.supervisionTimeout);
    // record last requested
    lastRequestedParams = storedParams;
  }

  void onDisconnect(NimBLEServer* server, ble_gap_conn_desc* desc) override {
    (void)server;
    (void)desc;
    deviceConnected = false;
    currentConnHandle = 0xffff;
    Serial.println("BLE Client disconnected");
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
        storedParams.minInterval = val;
        Serial.printf("Characteristic write: minInterval (units) = %u (%.2f ms)\n", val, conn_interval_units_to_ms(val));
      } else if (pCharacteristic == pMaxCharacteristic) {
        storedParams.maxInterval = val;
        Serial.printf("Characteristic write: maxInterval (units) = %u (%.2f ms)\n", val, conn_interval_units_to_ms(val));
      } else if (pCharacteristic == pTimeoutCharacteristic) {
        storedParams.supervisionTimeout = val;
        Serial.printf("Characteristic write: supervisionTimeout (units) = %u (%d ms)\n", val, (int)timeout_units_to_ms(val));
      } else if (pCharacteristic == pLatencyCharacteristic) {
        storedParams.slaveLatency = val;
        Serial.printf("Characteristic write: slaveLatency = %u\n", val);
      }
      saveStoredParams();

      // If a central is connected, immediately request the update
      if (deviceConnected && pServer && currentConnHandle != 0xffff) {
        Serial.println("Device connected: sending updateConnParams with newly saved params");
        pServer->updateConnParams(currentConnHandle,
                                  storedParams.minInterval,
                                  storedParams.maxInterval,
                                  storedParams.slaveLatency,
                                  storedParams.supervisionTimeout);
        lastRequestedParams = storedParams;
      }
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
  pMinCharacteristic->setValue((const uint8_t*)&storedParams.minInterval, sizeof(storedParams.minInterval));
  pMaxCharacteristic->setValue((const uint8_t*)&storedParams.maxInterval, sizeof(storedParams.maxInterval));
  pTimeoutCharacteristic->setValue((const uint8_t*)&storedParams.supervisionTimeout, sizeof(storedParams.supervisionTimeout));
  pLatencyCharacteristic->setValue((const uint8_t*)&storedParams.slaveLatency, sizeof(storedParams.slaveLatency));

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

// --- Web server handlers ---
String pageHtml() {
  // Minimal UI: a form that sends GET to /set with ms values
  return String("<html><head><meta name=viewport content=width=device-width, initial-scale=1>") +
  "<title>BLE Conn Param Config</title></head><body><h3>BLE Connection Parameter Config - HOME</h3>" +
  "<form action=\"/set\" method=\"get\">" +
  "Min Interval (ms): <input name=\"min_ms\" type=number value=\"300\"><br>" +
  "Max Interval (ms): <input name=\"max_ms\" type=number value=\"480\"><br>" +
  "Supervision Timeout (ms): <input name=\"sto_ms\" type=number value=\"5760\"><br>" +
  "Slave Latency: <input name=\"lat\" type=number value=\"3\"><br>" +
  "<input type=submit value=\"Set & Apply\">" +
  "</form><p>Use /params to read current values (JSON-like)</p></body></html>";
}

void handleRoot() {
  webServer.send(200, "text/html", pageHtml());
}

void handleParams() {
  // Return a simple JSON string constructed manually
  String s = "{";
  s += "\"min_ms\":" + String(conn_interval_units_to_ms(storedParams.minInterval), 2) + ",";
  s += "\"max_ms\":" + String(conn_interval_units_to_ms(storedParams.maxInterval), 2) + ",";
  s += "\"sto_ms\":" + String(timeout_units_to_ms(storedParams.supervisionTimeout), 0) + ",";
  s += "\"lat\":" + String(storedParams.slaveLatency) + ",";
  s += "\"connected\":" + String(deviceConnected ? "true" : "false");
  s += "}";
  webServer.send(200, "application/json", s);
}

void handleSet() {
  // Accept ms-based values via query parameters and convert
  bool changed = false;
  if (webServer.hasArg("min_ms")) {
    float minms = webServer.arg("min_ms").toFloat();
    uint16_t units = ms_to_conn_interval_units(minms);
    storedParams.minInterval = units;
    changed = true;
  }
  if (webServer.hasArg("max_ms")) {
    float maxms = webServer.arg("max_ms").toFloat();
    uint16_t units = ms_to_conn_interval_units(maxms);
    storedParams.maxInterval = units;
    changed = true;
  }
  if (webServer.hasArg("sto_ms")) {
    float stoms = webServer.arg("sto_ms").toFloat();
    uint16_t units = ms_to_timeout_units(stoms);
    storedParams.supervisionTimeout = units;
    changed = true;
  }
  if (webServer.hasArg("lat")) {
    int lat = webServer.arg("lat").toInt();
    storedParams.slaveLatency = (uint16_t)lat;
    changed = true;
  }

  if (changed) {
    saveStoredParams();
    // Update characteristics values so GATT clients reading them see new values
    pMinCharacteristic->setValue((const uint8_t*)&storedParams.minInterval, sizeof(storedParams.minInterval));
    pMaxCharacteristic->setValue((const uint8_t*)&storedParams.maxInterval, sizeof(storedParams.maxInterval));
    pTimeoutCharacteristic->setValue((const uint8_t*)&storedParams.supervisionTimeout, sizeof(storedParams.supervisionTimeout));
    pLatencyCharacteristic->setValue((const uint8_t*)&storedParams.slaveLatency, sizeof(storedParams.slaveLatency));

    // If a BLE Central is currently connected, request connection parameter update
    if (deviceConnected && pServer && currentConnHandle != 0xffff) {
      Serial.println("Web request: applying new params to connected central via updateConnParams()");
      pServer->updateConnParams(currentConnHandle,
                                storedParams.minInterval,
                                storedParams.maxInterval,
                                storedParams.slaveLatency,
                                storedParams.supervisionTimeout);
      lastRequestedParams = storedParams;
    }
  }

  // redirect back to root
  webServer.sendHeader("Location", "/");
  webServer.send(302, "text/plain", "");
}

// WiFi configuration UI (served in AP fallback)
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

void setupWebServer() {
  webServer.on("/", HTTP_GET, handleRoot);
  webServer.on("/params", HTTP_GET, handleParams);
  webServer.on("/set", HTTP_GET, handleSet);
  webServer.on("/wifi", HTTP_GET, handleWifiPage);
  webServer.on("/savewifi", HTTP_GET, handleSaveWifi);
  webServer.begin();
  Serial.println("Web server started (http)");
}

void setupWiFi() {
  // Provide more helpful debug: scan networks, register event handler and attempt connect
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);

  Serial.println("Scanning for WiFi networks...");
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("  No networks found");
  } else {
    Serial.printf("  %d networks found:\n", n);
    for (int i = 0; i < n; ++i) {
      Serial.printf("    %d: %s (RSSI %d) %s\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "OPEN" : "SECURE");
      delay(10);
    }
  }

  // Lightweight event logging (reports disconnect reasons etc.)
  WiFi.onEvent([](WiFiEvent_t event) {
    Serial.printf("WiFi event: %d\n", (int)event);
  });

  // Try to load credentials from Preferences first
  String saved_ssid, saved_pass;
  preferences.begin("ble_cfg", false);
  saved_ssid = preferences.getString("ssid", "");
  saved_pass = preferences.getString("pass", "");
  preferences.end();

  if (saved_ssid.length()) {
    Serial.printf("Found saved WiFi credentials for SSID '%s'\n", saved_ssid.c_str());
    Serial.println("Attempting to connect using saved credentials...");
    WiFi.begin(saved_ssid.c_str(), saved_pass.c_str());
  } else {
    Serial.printf("Connecting to SSID '%s'\n", SSID);
    WiFi.begin(SSID, PASSWORD);
  }

  Serial.print("Connecting");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 60) {
    delay(500);
    Serial.print('.');
    attempts++;
  }

  int st = WiFi.status();
  if (st == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed (entering AP fallback)");
    Serial.printf("  WiFi.status() = %d\n", st);
    // start AP for configuration
    uint64_t mac = ESP.getEfuseMac();
    String apname = String(AP_SSID_PREFIX) + String((uint32_t)(mac & 0xFFFFFF), HEX);
    Serial.printf("Starting softAP '%s'\n", apname.c_str());
    WiFi.softAP(apname.c_str());
    IPAddress apIP = WiFi.softAPIP();
    Serial.printf("AP IP: %s\n", apIP.toString().c_str());
    Serial.println("Connect to the AP and open http://192.168.4.1/ to configure WiFi (or visit /wifi).");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nStarting BLE Peripheral + WebServer (main_v2)");

  // Load previously saved parameters
  loadStoredParams();

  // Setup BLE peripheral
  setupBLE();

  // Setup WiFi and webserver
  setupWiFi();
  setupWebServer();

  // print stored params
  Serial.printf("Stored params (units): min=%u max=%u latency=%u sto=%u\n",
                storedParams.minInterval, storedParams.maxInterval, storedParams.slaveLatency, storedParams.supervisionTimeout);
}

void loop() {
  webServer.handleClient();

  // Periodic status logging: report last requested params and connection state
  unsigned long now = millis();
  if (now - lastStatusMs > 2000) {
    lastStatusMs = now;
    Serial.print("Status: ");
    Serial.printf("connected=%s, connHandle=%u, ", deviceConnected ? "true" : "false", currentConnHandle);
    Serial.printf("stored[min=%.2fms,max=%.2fms,sto=%dms,lat=%u], lastRequested[min=%.2fms,max=%.2fms,sto=%dms,lat=%u]\n",
                  conn_interval_units_to_ms(storedParams.minInterval),
                  conn_interval_units_to_ms(storedParams.maxInterval),
                  (int)timeout_units_to_ms(storedParams.supervisionTimeout),
                  storedParams.slaveLatency,
                  conn_interval_units_to_ms(lastRequestedParams.minInterval),
                  conn_interval_units_to_ms(lastRequestedParams.maxInterval),
                  (int)timeout_units_to_ms(lastRequestedParams.supervisionTimeout),
                  lastRequestedParams.slaveLatency);
  }

  delay(10);
}

// Main function - this replaces the implicit Arduino main()
int main() {
  // Initialize the Arduino framework
  init();

  #if defined(USBCON)
    USBDevice.attach();
  #endif

  // Call Arduino setup function
  setup();

  // Arduino main loop equivalent
  for (;;) {
    loop();
    if (serialEventRun) serialEventRun();
  }

  return 0;
}