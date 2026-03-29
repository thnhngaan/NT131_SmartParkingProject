#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>

// --- Thay đổi thông số ở đây ---
const char* ssid = "HTNMKD"; 
const char* password = "HTNMKD79";
const char* mqtt_server = "TÍNH SAU"; // IP máy tính chạy Broker

#define SS_PIN  5
#define RST_PIN 22
#define SERVO_PIN 13

MFRC522 rfid(SS_PIN, RST_PIN);
Servo gateServo;
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();
  gateServo.attach(SERVO_PIN);
  gateServo.write(0);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Nhận lệnh mở cổng từ Server
  String msg;
  for (int i = 0; i < length; i++) msg += (char)payload[i];
  if (msg == "OPEN") {
    gateServo.write(90);
    delay(5000);
    gateServo.write(0);
  }
}

void loop() {
  if (!client.connected()) {
    if (client.connect("ESP32_Gate_Controller")) client.subscribe("parking/gate");
  }
  client.loop();

  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uid += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  
  Serial.println("Quet thẻ: " + uid);
  // PUBLISH lệnh cho Camera
  client.publish("parking/request_photo", uid.c_str());
  delay(2000);
}