const mongoose = require('mongoose');

const parkingSchema = new mongoose.Schema({
  uid: {
    type: String,
    required: true,
    trim: true,
  },
  slotNumber: {
    type: String,
    required: true,
    trim: true,
    default: 'UNKNOWN',
  },
  entryTime: {
    type: Date,
    default: Date.now,
  },
  exitTime: {
    type: Date,
    default: null,
  },
  status: {
    type: String,
    enum: ['IN', 'OUT'],
    required: true,
    default: 'IN',
  },
  createdAt: {
    type: Date,
    default: Date.now,
  },
}, {
  versionKey: false,
});

parkingSchema.index({ uid: 1, status: 1 });
parkingSchema.index({ entryTime: -1 });

module.exports = mongoose.model('Parking', parkingSchema);
