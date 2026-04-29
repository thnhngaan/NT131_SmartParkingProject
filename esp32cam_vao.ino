#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>

// ===== WiFi =====
const char* WIFI_SSID = "Pmin";
const char* WIFI_PASS = "13050709";

// ===== MQTT =====
const char* MQTT_HOST = "10.62.149.182";
const int MQTT_PORT = 1883;

WebServer server(80);
WiFiClient espClient;
PubSubClient mqtt(espClient);

// ===== AI Thinker ESP32-CAM pins =====
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

String lastUID = "Chua co";
String lastToken = "0";
String lastStatus = "Chua chup";

uint8_t* lastImageBuf = nullptr;
size_t lastImageLen = 0;
unsigned long lastCaptureMillis = 0;

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

  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 10;
  config.fb_count = 1;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_brightness(s, 1);
    s->set_contrast(s, 1);
    s->set_saturation(s, 0);
  }

  Serial.println("CAM IN init OK");
  return true;
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
    Serial.print("IP CAM IN: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi chua ket noi duoc");
  }
}

void publishResult(const String &status, const String &uid, const String &token) {
  String msg = "{\"status\":\"" + status + "\",\"uid\":\"" + uid + "\",\"token\":\"" + token + "\"}";
  mqtt.publish("parking/cam/in/result", msg.c_str());
  Serial.println(msg);
}

bool captureAndStore() {
  if (millis() - lastCaptureMillis < 500) return false;
  lastCaptureMillis = millis();

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Capture failed");
    return false;
  }

  if (lastImageBuf != nullptr) {
    free(lastImageBuf);
    lastImageBuf = nullptr;
    lastImageLen = 0;
  }

  lastImageBuf = (uint8_t*)malloc(fb->len);
  if (!lastImageBuf) {
    Serial.println("Khong du RAM de luu anh");
    esp_camera_fb_return(fb);
    return false;
  }

  memcpy(lastImageBuf, fb->buf, fb->len);
  lastImageLen = fb->len;
  esp_camera_fb_return(fb);

  Serial.print("CAM IN capture OK, size = ");
  Serial.println(lastImageLen);
  return true;
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.print("MQTT [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(msg);

  if (String(topic) == "parking/cam/in/cmd") {
    int uPos = msg.indexOf("\"uid\":\"");
    if (uPos >= 0) {
      uPos += 7;
      int ePos = msg.indexOf("\"", uPos);
      if (ePos > uPos) lastUID = msg.substring(uPos, ePos);
    }

    int tPos = msg.indexOf("\"token\":\"");
    if (tPos >= 0) {
      tPos += 9;
      int ePos = msg.indexOf("\"", tPos);
      if (ePos > tPos) lastToken = msg.substring(tPos, ePos);
    }

    bool ok = captureAndStore();
    lastStatus = ok ? "capture_ok" : "capture_fail";
    publishResult(lastStatus, lastUID, lastToken);
  }
}

void connectMQTT() {
  if (mqtt.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;

  while (!mqtt.connected()) {
    String clientId = "ESP32_CAM_IN_" + String((uint32_t)ESP.getEfuseMac(), HEX);
    Serial.print("Dang ket noi MQTT...");
    if (mqtt.connect(clientId.c_str())) {
      Serial.println("OK");
      mqtt.subscribe("parking/cam/in/cmd");
    } else {
      Serial.print("Loi rc=");
      Serial.println(mqtt.state());
      delay(2000);
      connectWiFi();
      if (WiFi.status() != WL_CONNECTED) break;
    }
  }
}

void handleRoot() {
  String html = "<html><body>";
  html += "<h2>ESP32-CAM IN</h2>";
  html += "<p>IP: " + WiFi.localIP().toString() + "</p>";
  html += "<p>UID: " + lastUID + "</p>";
  html += "<p>Status: " + lastStatus + "</p>";
  html += "<p>Token: " + lastToken + "</p>";
  html += "<p><a href='/photo'>Xem anh</a></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handlePhoto() {
  if (lastImageBuf == nullptr || lastImageLen == 0) {
    server.send(404, "text/plain", "Chua co anh nao duoc chup");
    return;
  }

  WiFiClient client = server.client();
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: image/jpeg");
  client.print("Content-Length: ");
  client.println(lastImageLen);
  client.println("Cache-Control: no-cache, no-store, must-revalidate");
  client.println("Pragma: no-cache");
  client.println("Expires: 0");
  client.println("Connection: close");
  client.println();
  client.write(lastImageBuf, lastImageLen);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!initCamera()) {
    while (true) delay(1000);
  }

  connectWiFi();

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);

  server.on("/", handleRoot);
  server.on("/photo", handlePhoto);
  server.begin();

  Serial.println("ESP32-CAM IN READY");
}

void loop() {
  connectWiFi();
  connectMQTT();

  if (mqtt.connected()) mqtt.loop();
  server.handleClient();
}
