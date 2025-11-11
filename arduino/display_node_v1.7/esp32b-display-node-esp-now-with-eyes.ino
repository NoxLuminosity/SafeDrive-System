/* Display Node v1.7 (OLED + Serial Mirror + Backend POST)
   - Receives sensor data via ESP-NOW
   - Runs webserver for manual alert/drowsy
   - POSTs incoming sensor + phone state to backend API
   - OLED and Serial Monitor now show the same status text
*/

#include <WiFi.h>
#include <WebServer.h>
#include <esp_now.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HTTPClient.h>

// ====== Wi-Fi Credentials ======
const char* ssid = "Maeil Cafe";
const char* password = "eggbreadisgood";

// ====== Backend URL ======
const char* BACKEND_URL = "https://safedriveapp.loca.lt/api/prototype";

// ===== OLED Display =====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define DC_PIN 13
#define CS_PIN 12
#define RESET_PIN 15
#define SDA_PIN 14
#define SCL_PIN 2

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, DC_PIN, RESET_PIN, CS_PIN);

// ===== Sensor Node MAC =====
uint8_t sensorNodeMAC[] = {0x84, 0x1F, 0xE8, 0x69, 0x89, 0x08};

// ===== WebServer =====
WebServer server(80);

// ===== Phone-sent eye/drowsy state =====
bool phoneDrowsy = false;
bool havePhoneState = false;
unsigned long phoneStateTimestamp = 0;

// ===== Message struct to sensor node =====
typedef struct struct_message {
  bool drowsy;
} struct_message;
struct_message messageToSend;

// ===== Sensor data struct =====
typedef struct struct_sensorData {
  char carState[12];
  char grip[8];
  int heartRate;
  int spo2;
} sensorData;
sensorData incomingData = {"---", "---", -1, -1};

// ===== POST control =====
bool dataUpdated = false;
unsigned long lastPostMillis = 0;
const unsigned long POST_INTERVAL_MS = 3000;
String lastPostedJson = "";

// ===== ESP-NOW Callbacks =====
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  // Optional: can print target + status
}

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(incomingData)) {
    Serial.println("⚠️ Received data length mismatch.");
    return;
  }
  memcpy(&incomingData, data, sizeof(incomingData));
  dataUpdated = true;

  Serial.println("\n✅ Received sensor data:");
  Serial.print("Car: "); Serial.println(incomingData.carState);
  Serial.print("Grip: "); Serial.println(incomingData.grip);
  Serial.print("BPM: "); Serial.println(incomingData.heartRate);
  Serial.print("SpO2: "); Serial.println(incomingData.spo2);
}

// ===== Web Handlers =====
void handleRoot() {
  String html = "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'/>"
                "<title>Driver Monitor</title></head><body style='font-family:Helvetica,Arial;'>"
                "<h3>Driver Monitor</h3>"
                "<p><a href='/drowsy'><button style='padding:10px 20px;font-size:18px;background:#ff6666;border:none;border-radius:6px;'>Drowsy</button></a>"
                " <a href='/alert'><button style='padding:10px 20px;font-size:18px;background:#66cc66;border:none;border-radius:6px;'>Alert</button></a></p>"
                "<p>Tip: Add this page to Home Screen or use Shortcuts to automate it.</p>"
                "</body></html>";
  server.send(200, "text/html", html);
}

void handleDrowsy() {
  phoneDrowsy = true;
  havePhoneState = true;
  phoneStateTimestamp = millis();

  messageToSend.drowsy = true;
  esp_err_t res = esp_now_send(sensorNodeMAC, (uint8_t*)&messageToSend, sizeof(messageToSend));
  Serial.print("Sent drowsy command, esp_now_send result: ");
  Serial.println(res);

  Serial.println("📲 Phone triggered: DROWSY");
  dataUpdated = true;
  server.send(200, "text/plain", "OK: DROWSY");
}

void handleAlert() {
  phoneDrowsy = false;
  havePhoneState = true;
  phoneStateTimestamp = millis();

  messageToSend.drowsy = false;
  esp_err_t res = esp_now_send(sensorNodeMAC, (uint8_t*)&messageToSend, sizeof(messageToSend));
  Serial.print("Sent alert command, esp_now_send result: ");
  Serial.println(res);

  Serial.println("📲 Phone triggered: ALERT");
  dataUpdated = true;
  server.send(200, "text/plain", "OK: ALERT");
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

const char* computeStatus() {
  unsigned long now = millis();
  if (havePhoneState && (now - phoneStateTimestamp > 10000)) {
    havePhoneState = false;
  }
  if (havePhoneState) return phoneDrowsy ? "DROWSY" : "ALERT";
  return "--";
}

// Build JSON string from incomingData + eyesStatus
String buildPayloadJson() {
  String eyesStatus = havePhoneState ? (phoneDrowsy ? "CLOSED" : "OPEN") : "--";
  String json = "{";
  json += "\"gripStatus\":\""; json += String(incomingData.grip); json += "\",";
  json += "\"eyesStatus\":\""; json += eyesStatus; json += "\",";
  if (incomingData.heartRate >= 0) {
    json += "\"bpm\":"; json += String(incomingData.heartRate); json += ",";
  }
  if (incomingData.spo2 >= 0) {
    json += "\"spo2\":"; json += String(incomingData.spo2); json += ",";
  }
  json += "\"carStatus\":\""; json += String(incomingData.carState); json += "\"";
  json += "}";
  return json;
}

void sendToBackendIfNeeded() {
  unsigned long now = millis();
  if (!dataUpdated && (now - lastPostMillis) < POST_INTERVAL_MS) return;

  String payload = buildPayloadJson();

  // avoid sending identical payload repeatedly
  if (payload.equals(lastPostedJson) && (now - lastPostMillis) < (POST_INTERVAL_MS * 2)) {
    dataUpdated = false;
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(BACKEND_URL);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(payload);
    Serial.print("🌐 POST to backend, code: ");
    Serial.println(code);

    if (code > 0) {
      String resp = http.getString();
      Serial.println("Response: " + resp);
      lastPostedJson = payload;
      lastPostMillis = now;
      dataUpdated = false;
    } else {
      Serial.println("⚠️ Failed to POST to backend");
    }
    http.end();
  } else {
    Serial.println("⚠️ Not connected to WiFi - can't POST");
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // OLED init
  SPI.begin(SCL_PIN, -1, SDA_PIN, CS_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 init failed"));
    for (;;) ;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Connecting Wi-Fi...");
  display.display();

  // WiFi connect
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi ");
  Serial.print(ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Wi-Fi Connected!");
  display.print("IP: ");
  display.println(WiFi.localIP());
  display.display();
  delay(1000);

  // ESP-NOW init
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
  } else {
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, sensorNodeMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  // Web server
  server.on("/", handleRoot);
  server.on("/drowsy", handleDrowsy);
  server.on("/alert", handleAlert);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();

  if (havePhoneState && (millis() - phoneStateTimestamp > 10000)) {
    havePhoneState = false;
    dataUpdated = true;
  }

  const char* statusStr = computeStatus();
  const char* eyeStr = havePhoneState ? (phoneDrowsy ? "CLOSED" : "OPEN") : "--";

  // ===== OLED Display =====
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Status: "); display.println(statusStr);
  display.print("Car: "); display.println(incomingData.carState);
  display.print("Eye: "); display.println(eyeStr);
  display.print("Grip: "); display.println(incomingData.grip);
  display.print("BPM: ");
  if (incomingData.heartRate == -1) display.println("___");
  else display.println(incomingData.heartRate);
  display.print("SpO2: ");
  if (incomingData.spo2 == -1) display.println("___");
  else display.println(incomingData.spo2);
  display.display();

  // ===== Serial Mirror =====
  Serial.println("-------------");
  Serial.print("Status: "); Serial.println(statusStr);
  Serial.print("Car: "); Serial.println(incomingData.carState);
  Serial.print("Eye: "); Serial.println(eyeStr);
  Serial.print("Grip: "); Serial.println(incomingData.grip);
  Serial.print("BPM: ");
  if (incomingData.heartRate == -1) Serial.println("___");
  else Serial.println(incomingData.heartRate);
  Serial.print("SpO2: ");
  if (incomingData.spo2 == -1) Serial.println("___");
  else Serial.println(incomingData.spo2);
  Serial.println("-------------");

  // POST to backend when new data arrives or interval passes
  sendToBackendIfNeeded();

  delay(200);
}
