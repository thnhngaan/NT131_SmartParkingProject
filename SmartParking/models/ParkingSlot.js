const mongoose = require('mongoose');

const parkingSlotSchema = new mongoose.Schema({
  slotNumber: { type: String, required: true, unique: true },
  occupiedBy: { type: mongoose.Schema.Types.ObjectId, ref: 'User', default: null },
  uid: { type: String, default: '' },
  token: { type: String, default: '' },
  entryTime: { type: Date, default: Date.now },
  exitTime: { type: Date, default: null },
}, { timestamps: true });

module.exports = mongoose.model('ParkingSlot', parkingSlotSchema);