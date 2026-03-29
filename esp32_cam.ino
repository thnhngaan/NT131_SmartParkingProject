#include "esp_camera.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>

const char* ssid = "HTNMKD";
const char* password = "HTNMKD79";
const char* mqtt_server = "192.168.1.100";
const char* server_url = "http://192.168.1.100:5000/upload";

WiFiClient espClient;
PubSubClient client(espClient);

// Cấu hình chân Pin Camera AI-Thinker
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

void setup() {
  Serial.begin(115200);
  // (Phần khởi tạo Camera giống các ví dụ chuẩn của ESP32-CAM)
  initCamera();
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Khi nhận được UID từ con ESP32 chính
  String uid = "";
  for (int i = 0; i < length; i++) uid += (char)payload[i];
  
  Serial.println("Lenh chup anh cho UID: " + uid);
  takeAndUpload(uid);
}

void takeAndUpload(String uid) {
  camera_fb_t * fb = esp_camera_fb_get();
  if(!fb) return;

  HTTPClient http;
  http.begin(server_url);
  http.addHeader("UID", uid); // Gửi kèm UID để server nhận diện
  http.POST(fb->buf, fb->len);
  
  esp_camera_fb_return(fb);
  http.end();
}

void loop() {
  if (!client.connected()) {
    if (client.connect("ESP32_Camera_Module")) client.subscribe("parking/request_photo");
  }
  client.loop();
}

void initCamera() {
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
  esp_camera_init(&config);
}