const ParkingSlot = require('../models/ParkingSlot');
const TOTAL_CAPACITY = 300;

const toSlotView = (slot) => ({
  ...slot.toObject(),
  status: slot.exitTime ? 'OUT' : 'IN',
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

exports.getHistoricalFlow = async (req, res) => {
  try {
    const date = req.query.date;
    const targetDate = date ? new Date(`${date}T00:00:00`) : new Date();
    if (Number.isNaN(targetDate.getTime())) {
      return res.status(400).json({ message: 'Invalid date format. Use YYYY-MM-DD.' });
    }

    const start = new Date(targetDate);
    start.setHours(0, 0, 0, 0);
    const end = new Date(targetDate);
    end.setHours(23, 59, 59, 999);

    const slots = await ParkingSlot.find({
      $or: [
        { entryTime: { $gte: start, $lte: end } },
        { exitTime: { $gte: start, $lte: end } },
      ],
    }).select('entryTime exitTime');

    const inBuckets = Array.from({ length: 24 }, () => 0);
    const outBuckets = Array.from({ length: 24 }, () => 0);

    slots.forEach((slot) => {
      if (slot.entryTime) {
        const d = new Date(slot.entryTime);
        if (d >= start && d <= end) inBuckets[d.getHours()] += 1;
      }
      if (slot.exitTime) {
        const d = new Date(slot.exitTime);
        if (d >= start && d <= end) outBuckets[d.getHours()] += 1;
      }
    });

    const result = Array.from({ length: 24 }, (_, hour) => {
      const ds = new Date(start);
      ds.setHours(hour, 0, 0, 0);
      return {
        ds: ds.toISOString().slice(0, 19).replace('T', ' '),
        in: inBuckets[hour],
        out: outBuckets[hour],
      };
    });

    res.json(result);
  } catch (err) {
    res.status(500).json({ message: err.message });
  }
};

function startEndOfLocalToday() {
  const start = new Date();
  start.setHours(0, 0, 0, 0);
  const end = new Date();
  end.setHours(23, 59, 59, 999);
  return { start, end };
}

exports.getTodaySummary = async (req, res) => {
  try {
    const { start, end } = startEndOfLocalToday();
    const enteredToday = await ParkingSlot.countDocuments({
      entryTime: { $gte: start, $lte: end },
    });
    const occupied = await ParkingSlot.countDocuments();
    const available = Math.max(TOTAL_CAPACITY - occupied, 0);
    res.json({
      enteredToday,
      available,
      totalCapacity: TOTAL_CAPACITY,
    });
  } catch (err) {
    res.status(500).json({ message: err.message });
  }
};