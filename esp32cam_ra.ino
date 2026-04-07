/*
 * =============================================
 *  ESP32-CAM AI-Thinker — MẠCH RA
 * =============================================
 *  Giống hệt mạch VÀO, chỉ khác:
 *    - MQTT_TOPIC = "parking/exit"
 *    - MQTT_CLIENT = "esp32cam-exit-01"
 *    - GATE_ID = "RGATE"
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
#define WIFI_PASS     "13050709" // Sửa ở đây

#define MQTT_HOST     "broker.hivemq.com"
#define MQTT_PORT     1883
#define MQTT_TOPIC    "parking/exit"        // ← khác mạch VÀO
#define MQTT_CLIENT   "esp32cam-exit-01"    // ← khác mạch VÀO

#define GATE_ID       "RGATE"              // ← khác mạch VÀO
#define SERVO_OPEN    90
#define SERVO_CLOSE   0
#define GATE_OPEN_MS  3000
// ═══════════════════════════════════════════

#define RC522_SS    2
#define RC522_RST   12
#define SERVO_PIN   4
#define BUZZER_PIN  16

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

MFRC522 rfid(RC522_SS, RC522_RST);
Servo gateServo;
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

void beep(int times, int onMs = 100, int offMs = 100) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(onMs);
    digitalWrite(BUZZER_PIN, LOW);
    if (i < times - 1) delay(offMs);
  }
}

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
    beep(2, 80, 80);
  } else {
    Serial.println("\nWiFi FAIL");
    beep(3, 300, 100);
  }
}

void connectMQTT() {
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(20000);
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
  config.frame_size   = FRAMESIZE_QVGA;
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

String getUID() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

void openGate() {
  gateServo.write(SERVO_OPEN);
  delay(GATE_OPEN_MS);
  gateServo.write(SERVO_CLOSE);
}

void publishData(String uid, String img) {
  ensureMQTT();
  String payload = "{";
  payload += "\"gate\":\"" + String(GATE_ID) + "\",";
  payload += "\"uid\":\"" + uid + "\",";
  payload += "\"image\":\"" + img + "\"";
  payload += "}";
  bool ok = mqtt.publish(MQTT_TOPIC, payload.c_str(), payload.length());
  Serial.println(ok ? "MQTT publish OK" : "MQTT publish FAIL");
}

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  SPI.begin(14, 13, 15, 2);
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

  beep(1, 400);
  Serial.println("=== SAN SANG ===");
}

void loop() {
  ensureMQTT();

  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    delay(100);
    return;
  }

  String uid = getUID();
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  Serial.println("The: " + uid);

  beep(1, 100);

  String img = captureBase64();

  beep(2, 80, 80);
  openGate();

  if (img.length() > 0) {
    publishData(uid, img);
  } else {
    Serial.println("Khong chup duoc anh");
  }

  delay(2000);
}
