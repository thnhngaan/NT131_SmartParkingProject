/*
 * =============================================
 *  ESP32 DevKit V1 — TRUNG TÂM
 * =============================================
 *  Chức năng:
 *    - Đọc 2 cảm biến IR (trạng thái chỗ đỗ)
 *    - Kết nối WiFi + MQTT
 *    - Publish trạng thái chỗ đỗ lên server
 *    - Subscribe topic từ 2 ESP32-CAM để biết
 *      xe vào/ra (tùy chọn, để log hoặc xử lý thêm)
 *
 *  Thư viện cần cài (Library Manager):
 *    - PubSubClient by Nick O'Leary
 *
 *  Board: "ESP32 Dev Module"
 * =============================================
 */

#include <WiFi.h>
#include <PubSubClient.h>

// ═══════════════════════════════════════════
//  ⚙️  CẤU HÌNH — chỉnh sửa phần này
// ═══════════════════════════════════════════
#define WIFI_SSID   "Pmin"
#define WIFI_PASS   "13050709" // Sửa ở đây

#define MQTT_HOST   "broker.hivemq.com"
#define MQTT_PORT   1883
#define MQTT_CLIENT "esp32-center-01"

// Topic publish trạng thái IR
#define TOPIC_IR    "parking/slots"

// Topic subscribe để nhận thông báo từ ESP32-CAM
#define TOPIC_ENTRY "parking/entry"
#define TOPIC_EXIT  "parking/exit"
// ═══════════════════════════════════════════

// ── Chân IR Sensor ───────────────────────────
#define IR_PIN_1    34   // ô đỗ số 1 (chỉ INPUT, không OUTPUT được)
#define IR_PIN_2    35   // ô đỗ số 2

// ── Khoảng thời gian gửi trạng thái IR (ms) ──
#define IR_INTERVAL 5000  // gửi mỗi 5 giây

// ─────────────────────────────────────────────
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

unsigned long lastIRSend = 0;

// ── WiFi ──────────────────────────────────────
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting WiFi");
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    Serial.print(".");
    retry++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi OK: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi FAIL — reset sau 5s");
    delay(5000);
    ESP.restart();
  }
}

// ── MQTT callback — nhận tin từ ESP32-CAM ─────
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  String t = String(topic);

  if (t == TOPIC_ENTRY) {
    Serial.println("[ENTRY] Nhan du lieu tu cam vao:");
    // Chỉ in UID, không in ảnh vì quá dài
    int uidStart = msg.indexOf("\"uid\":\"") + 7;
    int uidEnd   = msg.indexOf("\"", uidStart);
    if (uidStart > 6 && uidEnd > uidStart) {
      String uid = msg.substring(uidStart, uidEnd);
      Serial.println("  UID: " + uid);
    }
  }

  if (t == TOPIC_EXIT) {
    Serial.println("[EXIT] Nhan du lieu tu cam ra:");
    int uidStart = msg.indexOf("\"uid\":\"") + 7;
    int uidEnd   = msg.indexOf("\"", uidStart);
    if (uidStart > 6 && uidEnd > uidStart) {
      String uid = msg.substring(uidStart, uidEnd);
      Serial.println("  UID: " + uid);
    }
  }
}

// ── MQTT ──────────────────────────────────────
void connectMQTT() {
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);

  int retry = 0;
  while (!mqtt.connected() && retry < 5) {
    Serial.print("Connecting MQTT...");
    if (mqtt.connect(MQTT_CLIENT)) {
      Serial.println("OK");
      // Subscribe để nhận thông báo xe vào/ra
      mqtt.subscribe(TOPIC_ENTRY);
      mqtt.subscribe(TOPIC_EXIT);
      Serial.println("Subscribed: parking/entry + parking/exit");
    } else {
      Serial.print("FAIL rc=");
      Serial.println(mqtt.state());
      delay(2000);
      retry++;
    }
  }
}

void ensureMQTT() {
  if (!mqtt.connected()) {
    Serial.println("MQTT mat ket noi, ket noi lai...");
    connectMQTT();
  }
  mqtt.loop();
}

// ── Đọc IR và publish ─────────────────────────
// IR active LOW: LOW = có xe, HIGH = trống
void publishIRStatus() {
  bool slot1Occupied = (digitalRead(IR_PIN_1) == LOW);
  bool slot2Occupied = (digitalRead(IR_PIN_2) == LOW);

  // JSON: {"slot1":true,"slot2":false}
  // true = có xe, false = trống
  String payload = "{";
  payload += "\"slot1\":" + String(slot1Occupied ? "true" : "false") + ",";
  payload += "\"slot2\":" + String(slot2Occupied ? "true" : "false");
  payload += "}";

  bool ok = mqtt.publish(TOPIC_IR, payload.c_str());

  Serial.print("IR Status: slot1=");
  Serial.print(slot1Occupied ? "CO XE" : "TRONG");
  Serial.print(" | slot2=");
  Serial.print(slot2Occupied ? "CO XE" : "TRONG");
  Serial.println(ok ? " [MQTT OK]" : " [MQTT FAIL]");
}

// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== ESP32 TRUNG TAM KHOI DONG ===");

  pinMode(IR_PIN_1, INPUT);
  pinMode(IR_PIN_2, INPUT);

  connectWiFi();
  connectMQTT();

  // Gửi ngay trạng thái lần đầu
  publishIRStatus();

  Serial.println("=== SAN SANG ===");
}

void loop() {
  // Giữ kết nối WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi mat ket noi, ket noi lai...");
    connectWiFi();
  }

  ensureMQTT();

  // Gửi trạng thái IR mỗi 5 giây
  unsigned long now = millis();
  if (now - lastIRSend >= IR_INTERVAL) {
    lastIRSend = now;
    publishIRStatus();
  }
}
