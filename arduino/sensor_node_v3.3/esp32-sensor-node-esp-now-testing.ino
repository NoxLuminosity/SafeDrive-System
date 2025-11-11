/* Sensor Node v3.3 (ESP-NOW sender + WiFi connected, NO API calls)
   - Sends data to Display Node via ESP-NOW
   - Connects to Wi-Fi (no API POSTs)
   - Buzzer: ON only when (Status == MOVING) AND (eyes closed OR grip LOOSE)
             stops immediately when eyes OPEN or grip becomes FIRM
*/

#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include <MPU6050_light.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"

// ========== WiFi CONFIGURATION ==========
const char* WIFI_SSID = "Maeil Cafe";       // change as needed
const char* WIFI_PASS = "eggbreadisgood";   // change as needed

// ========== Pin Setup ==========
#define SDA_PIN 21
#define SCL_PIN 22
#define FSR1_PIN 34
#define FSR2_PIN 35
#define RED_LED_PIN 13
#define BUZZER_PIN 12

// ========== MPU6050 ==========
MPU6050 mpu(Wire);
String carState = "STATIONARY";
int moveCount = 0;
int stillCount = 0;

// ========== MAX30105 ==========
MAX30105 particleSensor;
#define BUFFER_SIZE 50
uint32_t irBuffer[BUFFER_SIZE];
uint32_t redBuffer[BUFFER_SIZE];
int32_t spo2;
int8_t validSPO2;
int32_t heartRate;
int8_t validHeartRate;
int sampleIndex = 0;

// ========== FSR ==========
int fsr1Value = 0;
int fsr2Value = 0;
int fsrThreshold = 300;

// ========== Timer ==========
unsigned long timer = 0;

// ===== ESP-NOW peer (Display Node MAC) =====
uint8_t receiverMAC[] = {0x88, 0x57, 0x21, 0x79, 0x5F, 0xDC}; // keep your display MAC

typedef struct struct_sensorData {
  char carState[12];
  char grip[8];
  int heartRate;
  int spo2;
} sensorData;
sensorData outgoingData;

// ===== Command from Display Node =====
typedef struct struct_message {
  bool drowsy;
} struct_message;
struct_message recvMessage;

volatile bool phoneDrowsyReceived = false;
unsigned long phoneDrowsyTimestamp = 0;
const unsigned long PHONE_DROWSY_EXPIRE_MS = 10000; // 10s

// ===== ESP-NOW send =====
void sendSensorData() {
  strncpy(outgoingData.carState, carState.c_str(), sizeof(outgoingData.carState));
  bool firmGrip = (fsr1Value > fsrThreshold || fsr2Value > fsrThreshold);
  strncpy(outgoingData.grip, firmGrip ? "FIRM" : "LOOSE", sizeof(outgoingData.grip));
  outgoingData.heartRate = validHeartRate ? heartRate : -1;
  outgoingData.spo2 = validSPO2 ? spo2 : -1;
  esp_err_t res = esp_now_send(receiverMAC, (uint8_t *)&outgoingData, sizeof(outgoingData));
  // Optional: debug send result
  // Serial.print("esp_now_send res: "); Serial.println(res);
}

// ===== ESP-NOW receive callback =====
void onCommandRecv(const esp_now_recv_info_t *info, const uint8_t *incoming, int len) {
  if (len >= (int)sizeof(recvMessage)) {
    memcpy(&recvMessage, incoming, sizeof(recvMessage));
    phoneDrowsyReceived = recvMessage.drowsy;
    phoneDrowsyTimestamp = millis();
    Serial.print("📩 Received phone drowsy flag: ");
    Serial.println(phoneDrowsyReceived ? "TRUE" : "FALSE");
  }
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  Serial.println("\nInitializing sensors...");

  pinMode(FSR1_PIN, INPUT);
  pinMode(FSR2_PIN, INPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // ---- WiFi (connect but no API calls) ----
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
    // avoid infinite block if wifi unavailable for too long (optional)
    if (millis() - wifiStart > 20000) {
      Serial.println("\n⚠️ WiFi connect timeout — continuing without WiFi");
      break;
    }
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Connected to WiFi");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nℹ️ Not connected to WiFi (continuing).");
  }

  // ---- MPU6050 ----
  Wire.begin(SDA_PIN, SCL_PIN, 100000);
  Serial.println("Initializing MPU6050...");
  if (mpu.begin() != 0) {
    Serial.println("❌ MPU6050 connection failed!");
    while (1);
  }
  mpu.calcOffsets();
  Serial.println("✅ MPU6050 ready!");

  // ---- MAX30105 ----
  Serial.println("Initializing MAX30105...");
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println("❌ MAX30105 not found!");
    while (1);
  }
  particleSensor.setup(0x1F, 4, 2, 400, 411, 4096);
  for (int i = 0; i < BUFFER_SIZE; i++) {
    while (!particleSensor.check());
    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
  }

  // ---- ESP-NOW ----
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init failed!");
    while (1);
  }
  esp_now_register_recv_cb(onCommandRecv);
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  Serial.println("✅ Sensor Node ready\n");
}

// ===== Main Loop =====
void loop() {
  mpu.update();

  // Determine movement state (coarse)
  float ax = abs(mpu.getAccX());
  float ay = abs(mpu.getAccY());
  float az = abs(mpu.getAccZ() - 1.0);
  float gx = abs(mpu.getGyroX());
  float gy = abs(mpu.getGyroY());
  float gz = abs(mpu.getGyroZ());

  bool moving = (ax > 0.08 || ay > 0.08 || az > 0.15 || gx > 20 || gy > 20 || gz > 20);
  if (moving) { moveCount++; stillCount = 0; }
  else { stillCount++; moveCount = 0; }

  if (moveCount > 25) carState = "MOVING";
  if (stillCount > 40) carState = "STATIONARY";

  // FSRs
  fsr1Value = analogRead(FSR1_PIN);
  fsr2Value = analogRead(FSR2_PIN);
  bool firmGrip = (fsr1Value > fsrThreshold || fsr2Value > fsrThreshold);
  String gripState = firmGrip ? "FIRM" : "LOOSE";

  // phone drowsy expiry
  if (phoneDrowsyReceived && (millis() - phoneDrowsyTimestamp > PHONE_DROWSY_EXPIRE_MS)) {
    phoneDrowsyReceived = false;
    Serial.println("ℹ️ Phone drowsy expired");
  }

  // Buzzer logic (immediate on/off)
  // Only buzz while moving AND (eyes closed OR grip loose)
  bool eyesClosed = phoneDrowsyReceived; // phone sends drowsy=true when eyes closed
  bool shouldBuzz = (carState == "MOVING") && (eyesClosed || !firmGrip);
  digitalWrite(BUZZER_PIN, shouldBuzz ? HIGH : LOW);

  // Sensor fault LED (MOVING + LOOSE)
  bool sensorFault = (carState == "MOVING" && !firmGrip);
  digitalWrite(RED_LED_PIN, sensorFault ? HIGH : LOW);

  // MAX30105 read
  while (!particleSensor.check());
  redBuffer[sampleIndex] = particleSensor.getRed();
  irBuffer[sampleIndex] = particleSensor.getIR();
  sampleIndex++;
  if (sampleIndex >= BUFFER_SIZE) {
    maxim_heart_rate_and_oxygen_saturation(irBuffer, BUFFER_SIZE, redBuffer,
                                           &spo2, &validSPO2, &heartRate, &validHeartRate);
    sampleIndex = 0;
  }

  // Periodic Serial output & ESP-NOW send
  if (millis() - timer > 2000) {
    timer = millis();
    String eyesStatus = phoneDrowsyReceived ? "CLOSED" : "OPEN";

    // ======= SERIAL MONITOR OUTPUT =======
    Serial.println("\n================ SENSOR NODE DATA ================");
    Serial.print("Status: "); Serial.println(carState);
    Serial.print("Grip Status: "); Serial.println(gripState);
    Serial.print("FSR1 Value: "); Serial.println(fsr1Value);
    Serial.print("FSR2 Value: "); Serial.println(fsr2Value);
    Serial.print("Eyes (from Phone): "); Serial.println(eyesStatus);
    Serial.print("Heart Rate (BPM): "); Serial.println(validHeartRate ? heartRate : -1);
    Serial.print("Oxygen (SpO2): "); Serial.println(validSPO2 ? spo2 : -1);
    Serial.print("Buzzer: "); Serial.println(shouldBuzz ? "ON" : "OFF");
    Serial.println("=================================================\n");

    // Send to Display Node via ESP-NOW
    sendSensorData();

    // NOTE: intentionally NOT calling API here (Display Node will POST)
  }

  delay(5);
}
