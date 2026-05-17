HƯỚNG DẪN SỬ DỤNG

Đoạn code dùng để nạp cho các thiết bị bao gồm: 
- ESP32 CAM IN
- ESP32 CAM OUT
- ESP32 MAIN

---

## 1. Thư viện cần cài trong Arduino IDE
Cài các thư viện sau:

- MFRC522
- PubSubClient
- ArduinoJson
- ESP32Servo
---

## 2. Cấu hình mạng đang dùng

Trong code đã cấu hình:

- WiFi SSID: `Pmin`
- WiFi Password: `13050709`

(SỬA LẠI THEO AP CỦA BẠN)

---

## 3. MQTT topics (Tên kênh và message được gửi đến MQTT Broker)

### ESP32 chính publish
- `parking/slots`
- `parking/entry/request`
- `parking/exit/request`
- `parking/cam/in/cmd`
- `parking/cam/out/cmd`

### ESP32-CAM publish
- `parking/cam/in/result`
- `parking/cam/out/result`

### Server publish
- `parking/exit/decision`


