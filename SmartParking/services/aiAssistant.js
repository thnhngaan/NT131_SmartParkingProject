const { GoogleGenAI } = require('@google/genai');

// Khởi tạo đúng cách cho SDK mới
const ai = new GoogleGenAI({
  apiKey: process.env.GEMINI_API_KEY,
});

async function getAIResponse(prompt) {
  try {
    // SDK mới dùng method .models.generateContent()
    const response = await ai.models.generateContent({
      model: 'gemini-1.5-flash',  // Nên dùng gemini-2.0-flash thay vì 1.5
      contents: prompt,
    });

    // Trả về text từ response
    return response.text;
  } catch (error) {
    console.error('Error calling Gemini API:', error);
    throw new Error('Failed to get AI response');
  }
}

module.exports = {
  getAIResponse
};
