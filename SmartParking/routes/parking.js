const express = require('express');
const router = express.Router();
const { auth, adminOnly } = require('../middleware/auth');
const { getSlots, addSlot, deleteSlot, getAnalytics } = require('../controllers/parkingController');

router.get('/', auth, getSlots);
router.post('/', auth, adminOnly, addSlot);
router.delete('/:id', auth, adminOnly, deleteSlot);
router.get('/analytics', auth, getAnalytics);

module.exports = router;