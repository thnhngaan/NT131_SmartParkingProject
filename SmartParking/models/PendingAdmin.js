const mongoose = require('mongoose');

const pendingAdminSchema = new mongoose.Schema({
  username: { type: String, required: true },
  email: { type: String, required: true, unique: true, lowercase: true, trim: true },
  password: { type: String, required: true },
  pinHash: { type: String, required: true },
  expiresAt: { type: Date, required: true },
}, { timestamps: true });

pendingAdminSchema.index({ expiresAt: 1 }, { expireAfterSeconds: 0 });

module.exports = mongoose.model('PendingAdmin', pendingAdminSchema);
