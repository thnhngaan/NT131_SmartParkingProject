const express = require('express');
const router = express.Router();
const { auth, adminOnly } = require('../middleware/auth');
const {
  getSlots,
  addSlot,
  deleteSlot,
  getAnalytics,
  getHistoricalFlow,
  getTodaySummary,
} = require('../controllers/parkingController');

router.get('/today-summary', auth, getTodaySummary);
router.get('/', auth, adminOnly, getSlots);
router.post('/', auth, adminOnly, addSlot);
router.delete('/:id', auth, adminOnly, deleteSlot);
router.get('/analytics', auth, adminOnly, getAnalytics);
router.get('/history', auth, adminOnly, getHistoricalFlow);

module.exports = router;