const Parking = require('../models/Parking');
const TOTAL_CAPACITY = 300;

const normalizePlate = (plate) => {
  const value = String(plate || '').trim().toUpperCase();
  return value || 'UNKNOWN';
};

const emitParkingUpdate = (io, record) => {
  if (io && record) {
    io.emit('parking_update', record.toObject ? record.toObject() : record);
  }
};

async function createEntryRecord({ uid, plateNumber, entryTime = new Date() }) {
  const record = await Parking.create({
    uid,
    slotNumber: normalizePlate(plateNumber),
    entryTime,
    status: 'IN',
    createdAt: entryTime,
  });

  return record;
}

async function processExitRecord({ activeRecord, detectedPlate, exitTime = new Date() }) {
  const normalizedDetectedPlate = normalizePlate(detectedPlate);
  const normalizedStoredPlate = normalizePlate(activeRecord.slotNumber);

  if (
    normalizedStoredPlate !== 'UNKNOWN' &&
    normalizedDetectedPlate !== 'UNKNOWN' &&
    normalizedStoredPlate !== normalizedDetectedPlate
  ) {
    console.warn(
      `Plate mismatch for UID ${activeRecord.uid}: stored=${normalizedStoredPlate}, detected=${normalizedDetectedPlate}`
    );
  }

  activeRecord.exitTime = exitTime;
  activeRecord.status = 'OUT';
  await activeRecord.save();

  return activeRecord;
}

async function processParkingCapture({ uid, status = 'IN', yoloResult, io }) {
  const normalizedStatus = String(status || 'IN').trim().toUpperCase() === 'OUT' ? 'OUT' : 'IN';
  const detectedPlate = normalizePlate(
    yoloResult && yoloResult.detected ? yoloResult.plate_text : 'UNKNOWN'
  );

  if (normalizedStatus === 'OUT') {
    const activeRecord = await Parking.findOne({ uid, status: 'IN' }).sort({ entryTime: -1 });
    if (!activeRecord) {
      console.warn(`No active parking record found for UID ${uid} during exit.`);
      return null;
    }

    const updatedRecord = await processExitRecord({
      activeRecord,
      detectedPlate,
    });

    emitParkingUpdate(io, updatedRecord);
    return updatedRecord;
  }

  const activeRecord = await Parking.findOne({ uid, status: 'IN' }).sort({ entryTime: -1 });
  if (activeRecord) {
    return activeRecord;
  }

  const createdRecord = await createEntryRecord({
    uid,
    plateNumber: detectedPlate,
  });

  emitParkingUpdate(io, createdRecord);
  return createdRecord;
}

exports.processParkingCapture = processParkingCapture;

exports.getSlots = async (req, res) => {
  try {
    const records = await Parking.find().sort({ createdAt: -1 });
    res.json(records);
  } catch (err) {
    res.status(500).json({ message: err.message });
  }
};

exports.addSlot = async (req, res) => {
  try {
    const { slotNumber, uid = '' } = req.body;
    if (!slotNumber) {
      return res.status(400).json({ message: 'slotNumber is required' });
    }

    const record = await createEntryRecord({
      uid,
      plateNumber: slotNumber,
    });

    emitParkingUpdate(req.app.get('io'), record);
    res.status(201).json(record);
  } catch (err) {
    res.status(400).json({ message: err.message });
  }
};

exports.deleteSlot = async (req, res) => {
  try {
    const { id } = req.params;
    const record = await Parking.findById(id);
    if (!record) {
      return res.status(404).json({ message: 'Parking record not found' });
    }

    if (record.status === 'OUT') {
      return res.status(400).json({ message: 'Parking record already checked out' });
    }

    const updatedRecord = await processExitRecord({
      activeRecord: record,
      detectedPlate: record.slotNumber,
    });

    emitParkingUpdate(req.app.get('io'), updatedRecord);
    res.json(updatedRecord);
  } catch (err) {
    res.status(500).json({ message: err.message });
  }
};

exports.getAnalytics = async (req, res) => {
  try {
    const occupied = await Parking.countDocuments({ status: 'IN' });
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

    const records = await Parking.find({
      $or: [
        { entryTime: { $gte: start, $lte: end } },
        { exitTime: { $gte: start, $lte: end } },
      ],
    }).select('entryTime exitTime');

    const inBuckets = Array.from({ length: 24 }, () => 0);
    const outBuckets = Array.from({ length: 24 }, () => 0);

    records.forEach((record) => {
      if (record.entryTime) {
        const d = new Date(record.entryTime);
        if (d >= start && d <= end) inBuckets[d.getHours()] += 1;
      }
      if (record.exitTime) {
        const d = new Date(record.exitTime);
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
    const enteredToday = await Parking.countDocuments({
      entryTime: { $gte: start, $lte: end },
    });
    const occupied = await Parking.countDocuments({ status: 'IN' });
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