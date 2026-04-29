#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>

// ================= WIFI =================
const char* WIFI_SSID = "Pmin";
const char* WIFI_PASS = "13050709";

// ================= MQTT =================
const char* MQTT_HOST = "10.62.149.182";
const int MQTT_PORT = 1883;

// ================= RFID =================
#define SPI_SCK   18
#define SPI_MISO  19
#define SPI_MOSI  23

#define SS_IN     21
#define RST_IN    13

#define SS_OUT    27
#define RST_OUT   26

// ================= IR =================
#define IR1_PIN   34
#define IR2_PIN   35

// ================= BUZZER =================
#define BUZZER_IN_PIN   32
#define BUZZER_OUT_PIN  33

// ================= SERVO =================
#define SERVO_IN_PIN    25
#define SERVO_OUT_PIN   14

WiFiClient espClient;
PubSubClient mqtt(espClient);

MFRC522 rfidIn(SS_IN, RST_IN);
MFRC522 rfidOut(SS_OUT, RST_OUT);

Servo servoIn;
Servo servoOut;

// Nếu buzzer của bạn LOW mới kêu thì để true
bool BUZZER_ACTIVE_LOW = true;

String parkedUIDs[50];
int parkedCount = 0;

String pendingExitUID = "";
String pendingExitToken = "";
bool waitingExitDecision = false;
unsigned long exitRequestMillis = 0;
const unsigned long EXIT_TIMEOUT_MS = 15000;

unsigned long lastIRPublish = 0;

String uidToString(MFRC522::Uid *uid) {
  String s = "";
  for (byte i = 0; i < uid->size; i++) {
    if (uid->uidByte[i] < 0x10) s += "0";
    s += String(uid->uidByte[i], HEX);
  }
  s.toUpperCase();
  return s;
}

// ===== SUA O DAY NEU MUON WHITELIST =====
// Hien tai: the nao quet duoc cung hop le
bool isValidUID(const String &uid) {
  return true;
}

bool isParked(const String &uid) {
  for (int i = 0; i < parkedCount; i++) {
    if (parkedUIDs[i] == uid) return true;
  }
  return false;
}

void addParked(const String &uid) {
  if (isParked(uid)) return;
  if (parkedCount < 50) parkedUIDs[parkedCount++] = uid;
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

void buzzerOn(int pin) {
  digitalWrite(pin, BUZZER_ACTIVE_LOW ? LOW : HIGH);
}

void buzzerOff(int pin) {
  digitalWrite(pin, BUZZER_ACTIVE_LOW ? HIGH : LOW);
}

void beep(int pin, int times, int onMs, int offMs) {
  for (int i = 0; i < times; i++) {
    buzzerOn(pin);
    delay(onMs);
    buzzerOff(pin);
    delay(offMs);
  }
}

void moveServoSmooth(Servo &servo, int fromAngle, int toAngle, int stepDelayMs) {
  if (fromAngle < toAngle) {
    for (int a = fromAngle; a <= toAngle; a++) {
      servo.write(a);
      delay(stepDelayMs);
    }
  } else {
    for (int a = fromAngle; a >= toAngle; a--) {
      servo.write(a);
      delay(stepDelayMs);
    }
  }
}

void openGateSmooth(Servo &servo) {
  moveServoSmooth(servo, 0, 90, 15);
  delay(700);
  moveServoSmooth(servo, 90, 0, 15);
}

bool slot1Occupied() {
  return digitalRead(IR1_PIN) == LOW;
}

bool slot2Occupied() {
  return digitalRead(IR2_PIN) == LOW;
}

int availableSlots() {
  int occupied = (slot1Occupied() ? 1 : 0) + (slot2Occupied() ? 1 : 0);
  return 2 - occupied;
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Dang ket noi WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 12000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi chua ket noi duoc");
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.print("MQTT [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(msg);

  if (String(topic) == "parking/cam/in/result") {
    // CHI LOG - KHONG CAN XU LY THEM O DAY
  }

  if (String(topic) == "parking/exit/decision") {
    int allowPos = msg.indexOf("\"allow\":");
    bool allow = false;
    if (allowPos >= 0) {
      String sub = msg.substring(allowPos + 8);
      allow = sub.startsWith("true");
    }

    int tokenPos = msg.indexOf("\"token\":\"");
    String token = "";
    if (tokenPos >= 0) {
      tokenPos += 9;
      int endPos = msg.indexOf("\"", tokenPos);
      if (endPos > tokenPos) token = msg.substring(tokenPos, endPos);
    }

    if (waitingExitDecision && token == pendingExitToken) {
      if (allow) {
        Serial.println("Server cho phep xe ra");
        beep(BUZZER_OUT_PIN, 2, 120, 80);
        openGateSmooth(servoOut);
        removeParked(pendingExitUID);
      } else {
        Serial.println("Server tu choi xe ra");
        beep(BUZZER_OUT_PIN, 3, 120, 80);
      }

      waitingExitDecision = false;
      pendingExitUID = "";
      pendingExitToken = "";
    }
  }
}

void connectMQTT() {
  if (mqtt.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;

  while (!mqtt.connected()) {
    String clientId = "ESP32_MAIN_" + String((uint32_t)ESP.getEfuseMac(), HEX);
    Serial.print("Dang ket noi MQTT...");
    if (mqtt.connect(clientId.c_str())) {
      Serial.println("OK");
      mqtt.subscribe("parking/cam/in/result");
      mqtt.subscribe("parking/exit/decision");
    } else {
      Serial.print("Loi rc=");
      Serial.println(mqtt.state());
      delay(2000);
      connectWiFi();
      if (WiFi.status() != WL_CONNECTED) break;
    }
  }
}

void publishIRStatus() {
  String msg = "{";
  msg += "\"ir1\":" + String(slot1Occupied() ? "true" : "false") + ",";
  msg += "\"ir2\":" + String(slot2Occupied() ? "true" : "false") + ",";
  msg += "\"available\":" + String(availableSlots());
  msg += "}";

  mqtt.publish("parking/ir/status", msg.c_str(), true);
}

void publishCamInCmd(const String &uid, const String &token) {
  String msg = "{";
  msg += "\"cmd\":\"capture\",";
  msg += "\"uid\":\"" + uid + "\",";
  msg += "\"token\":\"" + token + "\",";
  msg += "\"cam_in_ip\":\"10.62.149.44\",";
  msg += "\"photo_url\":\"http://10.62.149.44/photo?t=" + token + "\"";
  msg += "}";

  mqtt.publish("parking/cam/in/cmd", msg.c_str());
  Serial.println("CAM IN CMD: " + msg);
}

void publishCamOutCmd(const String &uid, const String &token) {
  String msg = "{\"cmd\":\"capture\",\"uid\":\"" + uid + "\",\"token\":\"" + token + "\"}";
  mqtt.publish("parking/cam/out/cmd", msg.c_str());
}

void publishExitRequest(const String &uid, const String &token) {
  String msg = "{";
  msg += "\"uid\":\"" + uid + "\",";
  msg += "\"token\":\"" + token + "\",";
  msg += "\"cam_out_ip\":\"10.62.149.43\",";
  msg += "\"photo_url\":\"http://10.62.149.43/photo?t=" + token + "\"";
  msg += "}";

  mqtt.publish("parking/exit/request", msg.c_str());
}

void handleEntry() {
  digitalWrite(SS_OUT, HIGH);

  if (!rfidIn.PICC_IsNewCardPresent()) return;
  if (!rfidIn.PICC_ReadCardSerial()) return;

  String uid = uidToString(&rfidIn.uid);
  Serial.print("IN UID: ");
  Serial.println(uid);

  if (!isValidUID(uid)) {
    Serial.println("The vao khong hop le");
    beep(BUZZER_IN_PIN, 3, 100, 80);
  } else if (availableSlots() <= 0) {
    Serial.println("Bai xe da day");
    beep(BUZZER_IN_PIN, 2, 300, 120);
  } else if (isParked(uid)) {
    Serial.println("The nay da co xe trong bai");
    beep(BUZZER_IN_PIN, 3, 80, 80);
  } else {
    String token = String(millis());
    beep(BUZZER_IN_PIN, 1, 120, 50);
    publishCamInCmd(uid, token);
    openGateSmooth(servoIn);
    addParked(uid);
  }

  rfidIn.PICC_HaltA();
  rfidIn.PCD_StopCrypto1();
  delay(500);
}

void handleExit() {
  digitalWrite(SS_IN, HIGH);

  if (!rfidOut.PICC_IsNewCardPresent()) return;
  if (!rfidOut.PICC_ReadCardSerial()) return;

  String uid = uidToString(&rfidOut.uid);
  Serial.print("OUT UID: ");
  Serial.println(uid);

  if (!isValidUID(uid)) {
    Serial.println("The ra khong hop le");
    beep(BUZZER_OUT_PIN, 3, 100, 80);
  } else if (!isParked(uid)) {
    Serial.println("Khong tim thay xe trong bai");
    beep(BUZZER_OUT_PIN, 2, 250, 100);
  } else if (waitingExitDecision) {
    Serial.println("Dang cho xac nhan xe ra truoc do");
    beep(BUZZER_OUT_PIN, 2, 80, 80);
  } else {
    pendingExitUID = uid;
    pendingExitToken = String(millis());
    waitingExitDecision = true;
    exitRequestMillis = millis();

    beep(BUZZER_OUT_PIN, 1, 120, 50);
    publishCamOutCmd(uid, pendingExitToken);
    publishExitRequest(uid, pendingExitToken);
    Serial.println("Da gui yeu cau xe ra len server");
  }

  rfidOut.PICC_HaltA();
  rfidOut.PCD_StopCrypto1();
  delay(500);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(IR1_PIN, INPUT);
  pinMode(IR2_PIN, INPUT);

  pinMode(BUZZER_IN_PIN, OUTPUT);
  pinMode(BUZZER_OUT_PIN, OUTPUT);
  buzzerOff(BUZZER_IN_PIN);
  buzzerOff(BUZZER_OUT_PIN);

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  pinMode(SS_IN, OUTPUT);
  pinMode(SS_OUT, OUTPUT);
  digitalWrite(SS_IN, HIGH);
  digitalWrite(SS_OUT, HIGH);

  rfidIn.PCD_Init();
  delay(50);
  rfidOut.PCD_Init();
  delay(50);

  servoIn.setPeriodHertz(50);
  servoOut.setPeriodHertz(50);
  servoIn.attach(SERVO_IN_PIN, 500, 2400);
  servoOut.attach(SERVO_OUT_PIN, 500, 2400);
  servoIn.write(0);
  servoOut.write(0);

  connectWiFi();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);

  Serial.println("ESP32 MAIN READY");
}

void loop() {
  connectWiFi();
  connectMQTT();

  if (mqtt.connected()) {
    mqtt.loop();

    if (millis() - lastIRPublish > 3000) {
      lastIRPublish = millis();
      publishIRStatus();
    }
  }

  if (waitingExitDecision && millis() - exitRequestMillis > EXIT_TIMEOUT_MS) {
    Serial.println("Het thoi gian cho server xac nhan xe ra");
    waitingExitDecision = false;
    pendingExitUID = "";
    pendingExitToken = "";
    beep(BUZZER_OUT_PIN, 3, 200, 100);
  }

  handleEntry();
  handleExit();
}
