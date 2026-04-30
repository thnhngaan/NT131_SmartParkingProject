const { getAIResponse } = require('../services/aiAssistant');

exports.getAIAssistantResponse = async (req, res) => {
  try {
    const { prompt } = req.body;

    if (!prompt || typeof prompt !== 'string' || prompt.trim().length === 0) {
      return res.status(400).json({ message: 'Prompt is required and must be a non-empty string' });
    }

    const response = await getAIResponse(prompt.trim());
    res.json({ response });
  } catch (error) {
    console.error('AI Assistant error:', error);
    res.status(500).json({ message: 'Failed to get AI response' });
  }
};