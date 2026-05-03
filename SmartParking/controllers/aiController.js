const { getAIResponse } = require('../services/aiAssistant');

// ✅ EXPORT ĐÚNG TÊN (phải là getAIAssistantResponse)
exports.getAIAssistantResponse = async (req, res) => {
  try {
    const userId = req.user.id;
    const { prompt } = req.body;

    if (!prompt || prompt.trim() === '') {
      return res.status(400).json({ message: 'Prompt is required' });
    }

    const response = await getAIResponse(userId, prompt);

    // 👉 CHÚ Ý: Trả về response trong field "data" (như frontend mong đợi)
    res.json({ 
      success: true, 
      data: response,  // 👈 PHẢI LÀ "data", không phải "response"
      provider: 'Groq' 
    });
    
  } catch (err) {
    console.error('[Controller] Error:', err);
    res.status(500).json({ 
      success: false,
      message: err.message || 'Failed to get AI response' 
    });
  }
};