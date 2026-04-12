#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>

// ================= WIFI / MQTT =================
const char* WIFI_SSID = "Pmin";
const char* WIFI_PASS = "13050709";

const char* MQTT_HOST = "192.168.1.100";   // IP server Ubuntu chạy mosquitto
const int   MQTT_PORT = 1883;

// ================= PIN MAP =================
// RC522 #1 - ENTRY
#define SS_ENTRY   21
#define RST_ENTRY  13

// RC522 #2 - EXIT
#define SS_EXIT    22
#define RST_EXIT   14

// SPI shared
#define SPI_SCK    18
#define SPI_MISO   19
#define SPI_MOSI   23

// Servo
#define SERVO_IN_PIN   25
#define SERVO_OUT_PIN  26

// Buzzer
#define BUZZER_IN_PIN  32
#define BUZZER_OUT_PIN 33

// IR slots
#define IR_SLOT_1      34
#define IR_SLOT_2      35

// ================= OBJECTS =================
WiFiClient espClient;
PubSubClient mqtt(espClient);

MFRC522 rfidEntry(SS_ENTRY, RST_ENTRY);
MFRC522 rfidExit(SS_EXIT, RST_EXIT);

Servo servoIn;
Servo servoOut;

// ================= CONFIG =================
const int SERVO_CLOSED = 0;
const int SERVO_OPEN   = 90;

bool slot1Occupied = false;
bool slot2Occupied = false;
int availableSlots = 0;

// Danh sách UID hợp lệ demo
String validUIDs[] = {
  "A1B2C3D4",
  "11223344",
  "DEADBEEF"
};
const int validUIDCount = sizeof(validUIDs) / sizeof(validUIDs[0]);

// Lưu trạng thái xe đang trong bãi theo UID
String parkedUIDs[50];
int parkedCount = 0;

// Chờ server duyệt cổng ra
String pendingExitEventId = "";
String pendingExitUID = "";
bool waitingExitDecision = false;
unsigned long exitRequestMillis = 0;
const unsigned long EXIT_TIMEOUT = 15000;

// publish trạng thái định kỳ
unsigned long lastStatusPub = 0;

// ================= HELPERS =================
String uidToString(MFRC522::Uid *uid) {
  String s = "";
  for (byte i = 0; i < uid->size; i++) {
    if (uid->uidByte[i] < 0x10) s += "0";
    s += String(uid->uidByte[i], HEX);
  }
  s.toUpperCase();
  return s;
}

bool isValidUID(const String &uid) {
  for (int i = 0; i < validUIDCount; i++) {
    if (uid == validUIDs[i]) return true;
  }
  return false;
}

bool isParked(const String &uid) {
  for (int i = 0; i < parkedCount; i++) {
    if (parkedUIDs[i] == uid) return true;
  }
  return false;
}

void addParked(const String &uid) {
  if (isParked(uid)) return;
  if (parkedCount < 50) {
    parkedUIDs[parkedCount++] = uid;
  }
}

void removeParked(const String &uid) {
  for (int i = 0; i < parkedCount; i++) {
    if (parkedUIDs[i] == uid) {
      for (int j = i; j < parkedCount - 1; j++) {
        parkedUIDs[j] = parkedUIDs[j + 1];
      }
      parkedCount--;
      return;
    }
  }
}

String makeEventId(const String &prefix) {
  return prefix + "_" + String((uint32_t)millis());
}

void beep(int pin, int times, int onMs, int offMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, HIGH);
    delay(onMs);
    digitalWrite(pin, LOW);
    delay(offMs);
  }
}

void openGate(Servo &servo, int pin) {
  servo.write(SERVO_OPEN);
  delay(2500);
  servo.write(SERVO_CLOSED);
}

void publishSlots() {
  StaticJsonDocument<128> doc;
  doc["slot1"] = slot1Occupied;
  doc["slot2"] = slot2Occupied;
  doc["occupied"] = (slot1Occupied ? 1 : 0) + (slot2Occupied ? 1 : 0);
  doc["available"] = availableSlots;
  doc["full"] = (availableSlots == 0);

  char buf[128];
  serializeJson(doc, buf);
  mqtt.publish("parking/slots", buf, true);
}

void requestCameraCapture(const char* topic, const String &eventId, const String &uid) {
  StaticJsonDocument<192> doc;
  doc["event_id"] = eventId;
  doc["uid"] = uid;

  char buf[192];
  serializeJson(doc, buf);
  mqtt.publish(topic, buf);
}

void publishEntryRequest(const String &eventId, const String &uid) {
  StaticJsonDocument<192> doc;
  doc["event_id"] = eventId;
  doc["uid"] = uid;
  doc["gate"] = "in";

  char buf[192];
  serializeJson(doc, buf);
  mqtt.publish("parking/entry/request", buf);
}

void publishExitRequest(const String &eventId, const String &uid) {
  StaticJsonDocument<192> doc;
  doc["event_id"] = eventId;
  doc["uid"] = uid;
  doc["gate"] = "out";

  char buf[192];
  serializeJson(doc, buf);
  mqtt.publish("parking/exit/request", buf);
}

void updateSlots() {
  // LM393 thường: LOW = có xe, HIGH = trống
  slot1Occupied = (digitalRead(IR_SLOT_1) == LOW);
  slot2Occupied = (digitalRead(IR_SLOT_2) == LOW);

  int occupied = (slot1Occupied ? 1 : 0) + (slot2Occupied ? 1 : 0);
  availableSlots = 2 - occupied;
}

void handleEntryCard() {
  if (!rfidEntry.PICC_IsNewCardPresent()) return;
  if (!rfidEntry.PICC_ReadCardSerial()) return;

  String uid = uidToString(&rfidEntry.uid);
  Serial.println("ENTRY UID: " + uid);

  rfidEntry.PICC_HaltA();
  rfidEntry.PCD_StopCrypto1();

  updateSlots();

  if (!isValidUID(uid)) {
    Serial.println("Entry denied: invalid UID");
    beep(BUZZER_IN_PIN, 3, 100, 100);
    return;
  }

  if (availableSlots <= 0) {
    Serial.println("Entry denied: full");
    beep(BUZZER_IN_PIN, 2, 300, 150);
    return;
  }

  if (isParked(uid)) {
    Serial.println("Entry denied: already parked");
    beep(BUZZER_IN_PIN, 3, 80, 80);
    return;
  }

  String eventId = makeEventId("ENTRY");

  requestCameraCapture("parking/cam/in/cmd", eventId, uid);
  publishEntryRequest(eventId, uid);

  beep(BUZZER_IN_PIN, 1, 150, 50);
  openGate(servoIn, SERVO_IN_PIN);

  addParked(uid);
  Serial.println("Entry allowed");
}

void handleExitCard() {
  if (!rfidExit.PICC_IsNewCardPresent()) return;
  if (!rfidExit.PICC_ReadCardSerial()) return;

  String uid = uidToString(&rfidExit.uid);
  Serial.println("EXIT UID: " + uid);

  rfidExit.PICC_HaltA();
  rfidExit.PCD_StopCrypto1();

  if (!isValidUID(uid)) {
    Serial.println("Exit denied: invalid UID");
    beep(BUZZER_OUT_PIN, 3, 100, 100);
    return;
  }

  if (!isParked(uid)) {
    Serial.println("Exit denied: UID not found in parking");
    beep(BUZZER_OUT_PIN, 2, 300, 100);
    return;
  }

  if (waitingExitDecision) {
    Serial.println("Still waiting previous exit decision");
    beep(BUZZER_OUT_PIN, 2, 80, 80);
    return;
  }

  pendingExitUID = uid;
  pendingExitEventId = makeEventId("EXIT");
  waitingExitDecision = true;
  exitRequestMillis = millis();

  requestCameraCapture("parking/cam/out/cmd", pendingExitEventId, uid);
  publishExitRequest(pendingExitEventId, uid);

  Serial.println("Exit request sent, waiting server decision...");
  beep(BUZZER_OUT_PIN, 1, 80, 50);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.print("MQTT [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(msg);

  if (String(topic) == "parking/exit/decision") {
    StaticJsonDocument<192> doc;
    DeserializationError err = deserializeJson(doc, msg);
    if (err) return;

    String eventId = doc["event_id"] | "";
    bool allow = doc["allow"] | false;

    if (waitingExitDecision && eventId == pendingExitEventId) {
      if (allow) {
        Serial.println("Exit approved by server");
        beep(BUZZER_OUT_PIN, 2, 120, 80);
        openGate(servoOut, SERVO_OUT_PIN);
        removeParked(pendingExitUID);
      } else {
        Serial.println("Exit denied by server");
        beep(BUZZER_OUT_PIN, 3, 120, 100);
      }

      waitingExitDecision = false;
      pendingExitEventId = "";
      pendingExitUID = "";
    }
  }
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.println(WiFi.localIP());
}

void connectMQTT() {
  while (!mqtt.connected()) {
    String clientId = "ESP32_MAIN_" + String((uint32_t)ESP.getEfuseMac(), HEX);
    Serial.print("Connecting MQTT...");
    if (mqtt.connect(clientId.c_str())) {
      Serial.println("OK");
      mqtt.subscribe("parking/exit/decision");
      publishSlots();
    } else {
      Serial.print("Failed, rc=");
      Serial.println(mqtt.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_IN_PIN, OUTPUT);
  pinMode(BUZZER_OUT_PIN, OUTPUT);
  digitalWrite(BUZZER_IN_PIN, LOW);
  digitalWrite(BUZZER_OUT_PIN, LOW);

  pinMode(IR_SLOT_1, INPUT);
  pinMode(IR_SLOT_2, INPUT);

  servoIn.setPeriodHertz(50);
  servoOut.setPeriodHertz(50);
  servoIn.attach(SERVO_IN_PIN, 500, 2400);
  servoOut.attach(SERVO_OUT_PIN, 500, 2400);
  servoIn.write(SERVO_CLOSED);
  servoOut.write(SERVO_CLOSED);

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  rfidEntry.PCD_Init();
  rfidExit.PCD_Init();

  connectWiFi();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);

  updateSlots();
  publishSlots();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();

  updateSlots();

  if (millis() - lastStatusPub > 3000) {
    lastStatusPub = millis();
    publishSlots();
  }

  if (waitingExitDecision && millis() - exitRequestMillis > EXIT_TIMEOUT) {
    Serial.println("Exit decision timeout");
    waitingExitDecision = false;
    pendingExitEventId = "";
    pendingExitUID = "";
    beep(BUZZER_OUT_PIN, 3, 200, 100);
  }

  handleEntryCard();
  handleExitCard();
}
