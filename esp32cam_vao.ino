#include "esp_camera.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ================= WIFI / MQTT / HTTP =================
const char* WIFI_SSID = "Pmin";
const char* WIFI_PASS = "13050709";

const char* MQTT_HOST = "192.168.1.100";
const int   MQTT_PORT = 1883;

// Server HTTP nhận ảnh
const char* UPLOAD_URL = "http://192.168.1.100:5000/upload";

// ================= CAMERA PINS AI THINKER =================
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

WiFiClient espClient;
PubSubClient mqtt(espClient);

const char* CMD_TOPIC = "parking/cam/in/cmd";
const char* RESULT_TOPIC = "parking/cam/in/result";

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
    String clientId = "ESP32_CAM_IN_" + String((uint32_t)ESP.getEfuseMac(), HEX);
    Serial.print("Connecting MQTT...");
    if (mqtt.connect(clientId.c_str())) {
      Serial.println("OK");
      mqtt.subscribe(CMD_TOPIC);
    } else {
      Serial.print("Failed rc=");
      Serial.println(mqtt.state());
      delay(2000);
    }
  }
}

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QQVGA;
    config.jpeg_quality = 15;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }
  return true;
}

bool uploadImage(const String &eventId, const String &uid) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Capture failed");
    return false;
  }

  HTTPClient http;
  String url = String(UPLOAD_URL) + "?gate=in&event_id=" + eventId + "&uid=" + uid;
  http.begin(url);
  http.addHeader("Content-Type", "image/jpeg");

  int code = http.POST(fb->buf, fb->len);
  Serial.printf("HTTP upload code: %d\n", code);

  esp_camera_fb_return(fb);
  http.end();

  return (code > 0 && code < 300);
}

void publishResult(const String &eventId, const String &uid, bool ok) {
  StaticJsonDocument<192> doc;
  doc["event_id"] = eventId;
  doc["uid"] = uid;
  doc["ok"] = ok;

  char buf[192];
  serializeJson(doc, buf);
  mqtt.publish(RESULT_TOPIC, buf);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  if (String(topic) == CMD_TOPIC) {
    StaticJsonDocument<192> doc;
    DeserializationError err = deserializeJson(doc, msg);
    if (err) return;

    String eventId = doc["event_id"] | "";
    String uid = doc["uid"] | "";

    Serial.println("Capture command received");
    bool ok = uploadImage(eventId, uid);
    publishResult(eventId, uid, ok);
  }
}

void setup() {
  Serial.begin(115200);

  if (!initCamera()) {
    while (true) {
      delay(1000);
    }
  }

  connectWiFi();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();
}
