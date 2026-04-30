# Smart Parking System with ESP32 + ESP32-CAM + MQTT

## 1. Mô tả hệ thống

Đây là hệ thống bãi đỗ xe thông minh sử dụng:

- 1 ESP32 chính
- 2 ESP32-CAM
- 2 RC522 RFID
- 2 buzzer active
- 2 servo SG90
- 2 cảm biến hồng ngoại LM393
- 1 server Ubuntu
- MQTT để giao tiếp điều khiển
- HTTP để upload ảnh

### Chức năng chính

- Xe vào:
  - quẹt thẻ RFID ở cổng vào
  - ESP32 chính kiểm tra thẻ hợp lệ và kiểm tra còn chỗ trống
  - nếu hợp lệ thì mở cổng vào
  - đồng thời gửi lệnh cho ESP32-CAM vào chụp ảnh và upload lên server

- Xe ra:
  - quẹt thẻ RFID ở cổng ra
  - ESP32 chính kiểm tra thẻ hợp lệ và xe đó có đang ở trong bãi không
  - nếu hợp lệ thì gửi lệnh cho ESP32-CAM ra chụp ảnh và upload lên server
  - gửi yêu cầu kiểm tra ra server
  - chỉ khi server trả về `allow = true` thì mới mở cổng ra

- ESP32 chính cũng đọc 2 cảm biến LM393 để biết:
  - chỗ nào đang có xe
  - còn bao nhiêu chỗ trống
  - bãi đã đầy hay chưa

---

## 2. Kiến trúc hệ thống

### 2.1 ESP32 chính
Nhiệm vụ:
- Kết nối Wi-Fi
- Kết nối MQTT broker
- Đọc 2 RC522
- Đọc 2 cảm biến LM393
- Điều khiển 2 servo
- Điều khiển 2 buzzer
- Gửi trạng thái số chỗ qua MQTT
- Ra lệnh cho 2 ESP32-CAM chụp ảnh
- Nhận phản hồi cho cổng ra từ server

### 2.2 ESP32-CAM vào
Nhiệm vụ:
- Kết nối Wi-Fi
- Kết nối MQTT broker
- Subscribe topic lệnh chụp ảnh cổng vào
- Khi nhận lệnh:
  - chụp ảnh
  - upload ảnh lên server bằng HTTP
  - publish trạng thái kết quả về MQTT

### 2.3 ESP32-CAM ra
Nhiệm vụ:
- Kết nối Wi-Fi
- Kết nối MQTT broker
- Subscribe topic lệnh chụp ảnh cổng ra
- Khi nhận lệnh:
  - chụp ảnh
  - upload ảnh lên server bằng HTTP
  - publish trạng thái kết quả về MQTT

### 2.4 Server Ubuntu
Nhiệm vụ:
- Chạy MQTT broker (ví dụ Mosquitto)
- Chạy HTTP API nhận ảnh `/upload`
- Lưu ảnh
- Lưu log vào database
- Xử lý kiểm tra ảnh xe ra
- Publish quyết định cho phép ra hay không qua topic:
  - `parking/exit/decision`

---

## 3. Sơ đồ phần cứng

## 3.1 ESP32 chính

### RC522 cổng vào
- VCC -> 3.3V
- GND -> GND
- RST -> GPIO13
- MISO -> GPIO19
- MOSI -> GPIO23
- SCK -> GPIO18
- SDA/SS -> GPIO21

### RC522 cổng ra
- VCC -> 3.3V
- GND -> GND
- RST -> GPIO14
- MISO -> GPIO19
- MOSI -> GPIO23
- SCK -> GPIO18
- SDA/SS -> GPIO22

### Servo
- Servo cổng vào signal -> GPIO25
- Servo cổng ra signal -> GPIO26
- VCC servo -> nguồn 5V riêng
- GND servo -> GND chung

### Buzzer
- Buzzer cổng vào I/O -> GPIO32
- Buzzer cổng ra I/O -> GPIO33
- VCC -> 3.3V hoặc 5V tùy module
- GND -> GND

### LM393
- LM393 số 1 OUT -> GPIO34
- LM393 số 2 OUT -> GPIO35
- VCC -> 5V
- GND -> GND

---

## 3.2 ESP32-CAM
ESP32-CAM chỉ cần:
- 5V
- GND
- kết nối Wi-Fi

Lưu ý:
- Nên cấp nguồn riêng ổn định cho từng ESP32-CAM
- Không nên dùng một bộ nguồn yếu cho cả 2 ESP32-CAM
- Nếu dùng 2 board nạp để cấp riêng cho 2 ESP32-CAM thì được
- Phải nối GND chung toàn hệ thống

---

## 4. Nguồn cấp

### Khuyến nghị
- ESP32 chính: cấp qua USB hoặc 5V ổn định
- ESP32-CAM vào: 5V riêng
- ESP32-CAM ra: 5V riêng
- Servo: 5V riêng đủ dòng
- Tất cả GND nối chung

### Cảnh báo
- Không cấp servo từ 3.3V
- Không cấp 2 ESP32-CAM bằng nguồn boost yếu
- Nếu 5V tụt còn 2.5V thì nguồn không đủ dòng hoặc đấu sai

---

## 5. Thư viện cần cài trong Arduino IDE

Cài các thư viện sau:

- MFRC522
- PubSubClient
- ArduinoJson
- ESP32Servo

### Nếu lỗi kiểu:
- `ArduinoJson.h: No such file or directory`
- `MFRC522.h: No such file or directory`

thì vào:

- `Sketch`
- `Include Library`
- `Manage Libraries...`

rồi tìm và cài đúng tên thư viện.

---

## 6. Cấu hình mạng đang dùng

Trong code đã cấu hình:

- WiFi SSID: `Pmin`
- WiFi Password: `13050709`

Bạn cần sửa thêm đúng IP server Ubuntu tại các chỗ:

- `MQTT_HOST`
- `UPLOAD_URL`

Ví dụ:
- MQTT broker chạy trên máy Ubuntu: `192.168.1.100`
- HTTP upload API: `http://192.168.1.100:5000/upload`

---

## 7. MQTT topics

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

---

## 8. Format dữ liệu MQTT

### 8.1 Lệnh chụp ảnh gửi cho ESP32-CAM
Ví dụ:
```json
{
  "event_id": "ENTRY_12345",
  "uid": "A1B2C3D4"
}
