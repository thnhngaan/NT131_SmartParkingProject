const express = require('express');
const router = express.Router();
const { auth, adminOnly } = require('../middleware/auth');
const { getAIAssistantResponse } = require('../controllers/aiController');

router.post('/assistant', auth, adminOnly, getAIAssistantResponse);

module.exports = router;