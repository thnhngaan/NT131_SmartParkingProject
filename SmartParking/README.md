# Smart Parking Dashboard
Web phục vụ cho đồ án SmartParking của môn Hệ thống nhúng và Mạng không dây

## WebDashboard Structure
```text
smartparking/
├── ai/
│   └── license_plate_detection.py   # Python script for license plate detection
├── AI model/
│   └── yolo_model_files/            # Trained YOLO license plate model files
├── config/
│   └── database.js                  # MongoDB connection logic
├── controllers/
│   ├── aiController.js              # AI assistant endpoint handler
│   ├── authController.js            # Signup, signin, signout, verify admin
│   └── parkingController.js         # Parking record CRUD, analytics and summary APIs
├── middleware/
│   ├── auth.js                      # JWT authentication and admin authorization
│   └── error.js                     # Global error handler
├── models/
│   ├── Parking.js                   # Parking log schema
│   └── User.js                      # User schema
├── public/
│   ├── css/
│   ├── images/
│   └── js/                          # Static assets for CSS, JS, and images
├── routes/
│   ├── ai.js                        # AI assistant route
│   ├── auth.js                      # Authentication routes
│   └── parking.js                   # Parking management and analytics routes
├── views/                           # EJS templates for frontend pages
├── .env                             # Environment variables (PORT, MONGODB_URI, JWT_SECRET)
├── mqttHandler.js                   # MQTT gateway for parking event ingestion
├── package.json                     # Node.js dependencies and scripts
└── server.js                        # Main Express server and Socket.IO setup


## Hướng dẫn cài đặt

1. Clone repos này về mày

2. Cài đặt các thư viện
   ```bash
   npm install (Cài đặt các thư viện trong require)
   ```

3. Cấu hình biến môi trường
   Tạo một file mới tên là .env nằm ngay tại thư mục gốc (root directory) của dự án và dán đoạn cấu hình sau vào:
   ```
   PORT=3000
   MONGODB_URI=mongodb://localhost:27017/smartparking
   JWT_SECRET= ( cá nhân )
   ```

4. Bật MongDB
   

5. Chạy web
   ```bash
   npm run dev
   ```
Mở web `http://localhost:3000` trong duyệt trình bất kì.
