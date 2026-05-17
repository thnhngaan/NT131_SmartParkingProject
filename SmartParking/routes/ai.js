const express = require('express');
const router = express.Router();
const { auth, adminOnly } = require('../middleware/auth');
const { getAIAssistantResponse } = require('../controllers/aiController');

// ✅ Kiểm tra các handler có tồn tại không
console.log('🔍 Checking handlers:');
console.log('  - auth:', typeof auth);
console.log('  - adminOnly:', typeof adminOnly);
console.log('  - getAIAssistantResponse:', typeof getAIAssistantResponse);

router.post('/assistant', auth, adminOnly, getAIAssistantResponse);

module.exports = router;