const ParkingSlot = require('../models/ParkingSlot');
const TOTAL_CAPACITY = 300;

const toSlotView = (slot) => ({
  ...slot.toObject(),
  status: 'Occupied',
});

exports.getSlots = async (req, res) => {
  try {
    const slots = await ParkingSlot.find().sort({ createdAt: -1 });
    res.json(slots.map(toSlotView));
  } catch (err) {
    res.status(500).json({ message: err.message });
  }
};

exports.addSlot = async (req, res) => {
  try {
    const { slotNumber } = req.body;
    if (!slotNumber) {
      return res.status(400).json({ message: 'slotNumber is required' });
    }

    const occupied = await ParkingSlot.countDocuments();
    if (occupied >= TOTAL_CAPACITY) {
      return res.status(400).json({ message: 'Parking is full. No available slots.' });
    }

    const slot = new ParkingSlot({
      slotNumber,
      entryTime: new Date(),
    });
    await slot.save();
    res.status(201).json(toSlotView(slot));
  } catch (err) {
    if (err.code === 11000) {
      return res.status(400).json({ message: 'slotNumber already exists' });
    }
    res.status(400).json({ message: err.message });
  }
};

exports.deleteSlot = async (req, res) => {
  try {
    const { id } = req.params;
    const slot = await ParkingSlot.findById(id);
    if (!slot) {
      return res.status(404).json({ message: 'Slot not found' });
    }

    slot.exitTime = new Date();
    await slot.deleteOne();

    const occupied = await ParkingSlot.countDocuments();
    res.json({
      message: 'Car exited and slot released',
      exitTime: slot.exitTime,
      analytics: {
        total: TOTAL_CAPACITY,
        occupied,
        available: Math.max(TOTAL_CAPACITY - occupied, 0),
      },
    });
  } catch (err) {
    res.status(500).json({ message: err.message });
  }
};

exports.getAnalytics = async (req, res) => {
  try {
    const occupied = await ParkingSlot.countDocuments();
    const available = Math.max(TOTAL_CAPACITY - occupied, 0);
    res.json({ total: TOTAL_CAPACITY, occupied, available });
  } catch (err) {
    res.status(500).json({ message: err.message });
  }
};