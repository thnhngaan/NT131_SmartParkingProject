const mongoose = require('mongoose');
// file kết nối MongoDB, sử dụng URI từ biến môi trường MONGODB_URI. Hàm connectDB sẽ được gọi khi server khởi động để thiết lập kết nối với cơ sở dữ liệu. Nếu kết nối thành công, sẽ log "MongoDB connected". Nếu có lỗi, sẽ log lỗi và thoát process với mã 1.
const connectDB = async () => {
  try {
    await mongoose.connect(process.env.MONGODB_URI);
    console.log('MongoDB connected');
  } catch (err) {
    console.error(err);
    process.exit(1);
  }
};

module.exports = connectDB;