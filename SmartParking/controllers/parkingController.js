const Parking = require('../models/Parking'); // sử dụng các trường trong model Parking: uid, slotNumber, entryTime, exitTime, status, createdAt, imageUrl. 
const TOTAL_CAPACITY = 300;
// file controller xử lý logic liên quan đến parking
const normalizePlate = (plate) => {
  const value = String(plate || '').trim().toUpperCase();
  return value || 'UNKNOWN';
};

/**
 * Emit a parking update event via Socket.IO.
 * Accepts both Mongoose documents and plain objects.
 */
const emitParkingUpdate = (io, record) => {
  if (io && record) {
    const payload = record.toObject ? record.toObject() : record;
    console.log('[Parking] Emitting parking_update via Socket.IO:', {
      uid: payload.uid,
      status: payload.status,
      hasImageUrl: !!payload.imageUrl,
      imageUrl: payload.imageUrl
    });
    io.emit('parking_update', payload);
  } else {
    console.warn('[Parking] Cannot emit update - missing io or record:', { hasIo: !!io, hasRecord: !!record });
  }
};

/**
 * Create a new IN record for an entering vehicle.
 */
async function createEntryRecord({ uid, plateNumber, imageUrl, entryTime = new Date() }) {
  const record = await Parking.create({
    uid,
    slotNumber: normalizePlate(plateNumber),
    entryTime,
    status: 'IN',
    createdAt: entryTime,
    imageUrl,
  });
  return record;
}

/**
 * Mark an active record as OUT.
 * Returns a PLAIN OBJECT (not a Mongoose document) that may include
 * mismatch metadata (plateMismatch, storedPlate, detectedPlate, entryImageUrl)
 * for the frontend to consume.
 */
async function processExitRecord({ activeRecord, detectedPlate, imageUrl, exitTime = new Date() }) {
  const normalizedDetectedPlate = normalizePlate(detectedPlate);
  const normalizedStoredPlate   = normalizePlate(activeRecord.slotNumber);

  // ── Plate mismatch check ──
  let plateMismatch = false;
  if (
    normalizedStoredPlate   !== 'UNKNOWN' &&
    normalizedDetectedPlate !== 'UNKNOWN' &&
    normalizedStoredPlate   !== normalizedDetectedPlate
  ) {
    console.warn(
      `[Parking] ⚠️  Plate mismatch for UID ${activeRecord.uid}: ` +
      `stored=${normalizedStoredPlate}, detected=${normalizedDetectedPlate}`
    );
    plateMismatch = true;
  }if (
    (normalizedStoredPlate   == 'UNKNOWN' ||
    normalizedDetectedPlate == 'UNKNOWN' )&&
    normalizedStoredPlate   !== normalizedDetectedPlate
  ) {
    console.warn(
      `[Parking] ⚠️  Plate mismatch for UID ${activeRecord.uid}: ` +
      `stored=${normalizedStoredPlate}, detected=${normalizedDetectedPlate}`
    );
    plateMismatch = true;
  }

  // Preserve entry image before overwriting
  const entryImageUrl = activeRecord.imageUrl || null;

  // Update the record
  if (imageUrl) activeRecord.imageUrl = imageUrl;
  activeRecord.exitTime = exitTime;
  activeRecord.status   = 'OUT';
  await activeRecord.save();

  // Build plain-object payload with mismatch metadata
  const result = activeRecord.toObject();

  if (plateMismatch) {
    result.plateMismatch  = true;
    result.storedPlate    = normalizedStoredPlate;
    result.detectedPlate  = normalizedDetectedPlate;
    result.entryImageUrl  = entryImageUrl; // image captured when car entered
  }

  return result; // plain object — safe to emit directly
}

/**
 * Main entry point called by the MQTT handler.
 * Handles both IN and OUT events.
 */
async function processParkingCapture({ uid, status = 'IN', yoloResult, imageUrl, io }) {
  const normalizedStatus = String(status || 'IN').trim().toUpperCase() === 'OUT' ? 'OUT' : 'IN';

  // Prefer plate_text from YOLO result
  const detectedPlate = normalizePlate(yoloResult?.plate_text || 'UNKNOWN');

  console.log('[Parking] ━━━ PARKING CAPTURE START ━━━');
  console.log('[Parking] YOLO result:', JSON.stringify(yoloResult));
  console.log('[Parking] Detected plate:', detectedPlate);
  console.log('[Parking] Status:', normalizedStatus);
  console.log('[Parking] Image URL:', imageUrl);

  // ── EXIT flow ──
  if (normalizedStatus === 'OUT') {
    const activeRecord = await Parking.findOne({ uid, status: 'IN' }).sort({ entryTime: -1 });

    if (!activeRecord) {
      console.warn(`[Parking] No active record found for UID ${uid} during exit.`);
      return null;
    }

    // processExitRecord returns a plain object (may include mismatch fields)
    const updatedRecord = await processExitRecord({
      activeRecord,
      detectedPlate,
      imageUrl,
    });

    console.log('[Parking] Exit record processed with imageUrl:', updatedRecord.imageUrl);
    emitParkingUpdate(io, updatedRecord);
    console.log('[Parking] ━━━ PARKING CAPTURE END (EXIT) ━━━');
    return updatedRecord;
  }

  // ── ENTRY flow ──
  const activeRecord = await Parking.findOne({ uid, status: 'IN' }).sort({ entryTime: -1 });
  if (activeRecord) {
    console.log(`[Parking] UID ${uid} already has an active record. Skipping duplicate entry.`);
    return activeRecord;
  }

  const createdRecord = await createEntryRecord({
    uid,
    plateNumber: detectedPlate,
    imageUrl,
  });

  console.log('[Parking] Entry record created with imageUrl:', createdRecord.imageUrl);
  emitParkingUpdate(io, createdRecord);
  console.log('[Parking] ━━━ PARKING CAPTURE END (ENTRY) ━━━');
  return createdRecord;
}

exports.processParkingCapture = processParkingCapture;

/* ════════════════════════════════════════
   REST API handlers
════════════════════════════════════════ */

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
    // Bây giờ uid sẽ lấy giá trị từ req.body gửi lên, không còn bị mặc định là '' nữa
    const { slotNumber, uid } = req.body; 

    if (!slotNumber) {
      return res.status(400).json({ message: 'Biển số xe là bắt buộc' });
    }

    // Nếu không nhập UID, bạn có thể kiểm tra hoặc để trống tùy ý
    const finalUid = uid || "MANUAL_ENTRY"; // Nếu trống thì ghi chú là nhập tay

    const record = await createEntryRecord({ 
      uid: finalUid, 
      plateNumber: slotNumber 
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
      activeRecord:   record,
      detectedPlate:  record.slotNumber,
    });

    emitParkingUpdate(req.app.get('io'), updatedRecord);
    res.json(updatedRecord);
  } catch (err) {
    res.status(500).json({ message: err.message });
  }
};

exports.getAnalytics = async (req, res) => {
  try {
    const occupied  = await Parking.countDocuments({ status: 'IN' });
    const available = Math.max(TOTAL_CAPACITY - occupied, 0);
    res.json({ total: TOTAL_CAPACITY, occupied, available });
  } catch (err) {
    res.status(500).json({ message: err.message });
  }
};

exports.getHistoricalFlow = async (req, res) => {
  try {
    const date       = req.query.date;
    const targetDate = date ? new Date(`${date}T00:00:00`) : new Date();

    if (Number.isNaN(targetDate.getTime())) {
      return res.status(400).json({ message: 'Invalid date format. Use YYYY-MM-DD.' });
    }

    const start = new Date(targetDate); start.setHours(0,  0,  0,   0);
    const end   = new Date(targetDate); end.setHours(23, 59, 59, 999);

    const records = await Parking.find({
      $or: [
        { entryTime: { $gte: start, $lte: end } },
        { exitTime:  { $gte: start, $lte: end } },
      ],
    }).select('entryTime exitTime');

    const inBuckets  = Array.from({ length: 24 }, () => 0);
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
        ds:  ds.toISOString().slice(0, 19).replace('T', ' '),
        in:  inBuckets[hour],
        out: outBuckets[hour],
      };
    });

    res.json(result);
  } catch (err) {
    res.status(500).json({ message: err.message });
  }
};

function startEndOfLocalToday() {
  const start = new Date(); start.setHours(0,  0,  0,   0);
  const end   = new Date(); end.setHours(23, 59, 59, 999);
  return { start, end };
}

exports.getTodaySummary = async (req, res) => {
  try {
    const { start, end } = startEndOfLocalToday();
    const enteredToday   = await Parking.countDocuments({ entryTime: { $gte: start, $lte: end } });
    const occupied       = await Parking.countDocuments({ status: 'IN' });
    const available      = Math.max(TOTAL_CAPACITY - occupied, 0);
    res.json({ enteredToday, available, totalCapacity: TOTAL_CAPACITY });
  } catch (err) {
    res.status(500).json({ message: err.message });
  }
};