/*
 * =============================================
 *  ESP32-CAM AI-Thinker — MẠCH VÀO
 * =============================================
 *  Chức năng:
 *    - Đọc thẻ RFID (RC522)
 *    - Chụp ảnh
 *    - Mở servo (cần gạt)
 *    - Bíp buzzer
 *    - Tự kết nối WiFi + publish MQTT
 *
 *  Thư viện cần cài (Library Manager):
 *    - MFRC522 by GithubCommunity
 *    - ESP32Servo by Kevin Harrington
 *    - PubSubClient by Nick O'Leary
 *
 *  Board: "AI Thinker ESP32-CAM"
 * =============================================
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include "esp_camera.h"
#include "base64.h"

// ═══════════════════════════════════════════
//  ⚙️  CẤU HÌNH — chỉnh sửa phần này
// ═══════════════════════════════════════════
#define WIFI_SSID     "Pmin"
#define WIFI_PASS     "13050709"

#define MQTT_HOST     "broker.hivemq.com"
#define MQTT_PORT     1883
#define MQTT_TOPIC    "parking/entry"       // topic cổng vào
#define MQTT_CLIENT   "esp32cam-entry-01"   // tên duy nhất, không trùng

#define GATE_ID       "VGATE"
#define SERVO_OPEN    90
#define SERVO_CLOSE   0
#define GATE_OPEN_MS  3000
// ═══════════════════════════════════════════

// ── Chân RC522 ───────────────────────────────
#define RC522_SS    2
#define RC522_RST   12
// SCK=14, MOSI=15, MISO=13

// ── Chân Servo & Buzzer ───────────────────────
#define SERVO_PIN   4
#define BUZZER_PIN  16

// ── Camera AI-Thinker pinout ──────────────────
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ─────────────────────────────────────────────
MFRC522 rfid(RC522_SS, RC522_RST);
Servo gateServo;
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// ── Buzzer ────────────────────────────────────
void beep(int times, int onMs = 100, int offMs = 100) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(onMs);
    digitalWrite(BUZZER_PIN, LOW);
    if (i < times - 1) delay(offMs);
  }
}

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
    beep(2, 80, 80);  // 2 bíp ngắn = WiFi OK
  } else {
    Serial.println("\nWiFi FAIL");
    beep(3, 300, 100);  // 3 bíp dài = WiFi lỗi
  }
}

// ── MQTT ──────────────────────────────────────
void connectMQTT() {
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(20000);  // cần to vì payload có ảnh base64

  int retry = 0;
  while (!mqtt.connected() && retry < 5) {
    Serial.print("Connecting MQTT...");
    if (mqtt.connect(MQTT_CLIENT)) {
      Serial.println("OK");
    } else {
      Serial.print("FAIL rc=");
      Serial.println(mqtt.state());
      delay(2000);
      retry++;
    }
  }
}

void ensureMQTT() {
  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();
}

// ── Camera ────────────────────────────────────
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = FRAMESIZE_QVGA;  // 320x240 — nhỏ gọn, gửi nhanh
  config.jpeg_quality = 20;
  config.fb_count     = 1;
  return (esp_camera_init(&config) == ESP_OK);
}

String captureBase64() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) return "";
  String encoded = base64::encode(fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return encoded;
}

// ── RFID ──────────────────────────────────────
String getUID() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

// ── Servo ─────────────────────────────────────
void openGate() {
  gateServo.write(SERVO_OPEN);
  delay(GATE_OPEN_MS);
  gateServo.write(SERVO_CLOSE);
}

// ── Publish MQTT ──────────────────────────────
// Payload JSON: {"gate":"VGATE","uid":"AABBCCDD","image":"base64..."}
void publishData(String uid, String img) {
  ensureMQTT();

  String payload = "{";
  payload += "\"gate\":\"" + String(GATE_ID) + "\",";
  payload += "\"uid\":\"" + uid + "\",";
  payload += "\"image\":\"" + img + "\"";
  payload += "}";

  bool ok = mqtt.publish(MQTT_TOPIC, payload.c_str(), payload.length());
  if (ok) {
    Serial.println("MQTT publish OK");
  } else {
    Serial.println("MQTT publish FAIL");
  }
}

// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  SPI.begin(14, 13, 15, 2);  // SCK, MISO, MOSI, SS
  rfid.PCD_Init();
  Serial.println("RFID OK");

  gateServo.attach(SERVO_PIN);
  gateServo.write(SERVO_CLOSE);

  if (!initCamera()) {
    Serial.println("Camera FAIL");
    beep(5, 500, 200);
    while (1) delay(1000);
  }
  Serial.println("Camera OK");

  connectWiFi();
  connectMQTT();

  // Sẵn sàng: 1 bíp dài
  beep(1, 400);
  Serial.println("=== SAN SANG ===");
}

void loop() {
  // Giữ kết nối MQTT
  ensureMQTT();

  // Kiểm tra thẻ RFID
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    delay(100);
    return;
  }

  String uid = getUID();
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  Serial.println("The: " + uid);

  // Bíp 1 tiếng báo đọc thẻ
  beep(1, 100);

  // Chụp ảnh
  String img = captureBase64();

  // Mở cần gạt + bíp 2 tiếng
  beep(2, 80, 80);
  openGate();

  // Gửi lên MQTT
  if (img.length() > 0) {
    publishData(uid, img);
  } else {
    Serial.println("Khong chup duoc anh");
  }

  // Chống đọc thẻ liên tiếp
  delay(2000);
}
