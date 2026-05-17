const Groq = require('groq-sdk');
const Parking = require('../models/Parking');

const groq = new Groq({
  apiKey: process.env.GROQ_API_KEY,
});

const TZ = 'Asia/Ho_Chi_Minh';

// ================= TIMEOUT =================
function withTimeout(promise, timeoutMs = 30000) {
  return Promise.race([
    promise,
    new Promise((_, reject) =>
      setTimeout(() => reject(new Error(`Timeout ${timeoutMs}ms`)), timeoutMs)
    ),
  ]);
}

// ================= MAIN =================
async function getAIResponse(userIdOrPrompt, promptArg) {
  let userId = null;
  let prompt = '';

  if (typeof promptArg === 'string') {
    userId = userIdOrPrompt;
    prompt = promptArg;
  } else {
    prompt = userIdOrPrompt;
  }

  if (!prompt?.trim()) throw new Error('Prompt is required');
  prompt = prompt.trim();

  try {
    console.log('[AI] Prompt:', prompt);

    const today = getLocalDate();
    const intent = detectIntent(prompt);
    console.log('[AI] Intent:', intent);

    let data = null;
    if (intent !== 'general') {
      data = await getParkingData(prompt, today, intent);
    }

    console.log('[AI] Data:', JSON.stringify(data, null, 2));

    const enhancedPrompt = buildUserPrompt(prompt, data);

    const chatCompletion = await withTimeout(
      groq.chat.completions.create({
        messages: [
          {
            role: 'system',
            content: `Bạn là Parking AI — trợ lí ảo thông minh của hệ thống Smart Parking.

PHONG CÁCH GIAO TIẾP:
- Chào hỏi tự nhiên, thân thiện như một nhân viên hỗ trợ chuyên nghiệp.
- Dùng ngôn ngữ rõ ràng, dễ hiểu, tránh dùng thuật ngữ kỹ thuật khô khan.
- Khi trình bày số liệu, hãy diễn giải ý nghĩa ngắn gọn (ví dụ: "Đây là khung giờ cao điểm nhất trong ngày").
- Kết thúc câu trả lời có thể hỏi thêm hoặc gợi ý câu hỏi liên quan nếu phù hợp.

QUY TẮC XỬ LÝ DỮ LIỆU:
- CHỈ trả lời dựa trên dữ liệu được cung cấp, KHÔNG được suy đoán hay bịa đặt.
- Nếu totalIn = 0 hoặc totalOut = 0 → thông báo "Không có dữ liệu xe trong khoảng thời gian này."
- Nếu data rỗng hoặc null → thông báo "Hiện tại chưa có dữ liệu để phân tích."
- Giờ trong dữ liệu đã là giờ Việt Nam (UTC+7), hiển thị đúng định dạng HH:00.
- Khi có dữ liệu xe vào VÀ xe ra, hãy trình bày song song để dễ so sánh.`,
          },
          {
            role: 'user',
            content: enhancedPrompt,
          },
        ],
        model: process.env.GROQ_MODEL || 'llama-3.3-70b-versatile',
        temperature: 0.4,
      }),
      30000
    );

    return chatCompletion.choices[0]?.message?.content || '';
  } catch (err) {
    console.error('[AI ERROR]', err);
    throw err;
  }
}

// ================= INTENT =================
function detectIntent(prompt) {
  const text = prompt.toLowerCase();

  if (/xe.*ra|ra.*xe|exit|lượt ra|giờ ra/.test(text)) return 'peak_exit';
  if (/xe.*vào|vào.*xe|entry|lượt vào|giờ vào/.test(text)) return 'peak_entry';

  if (/tổng|bao nhiêu|lưu lượng|traffic/.test(text)) return 'total';
  if (/trung bình|average/.test(text)) return 'avg';
  if (/xu hướng|tăng|giảm|trend/.test(text)) return 'trend';
  if (/so sánh|vs/.test(text)) return 'compare';

  return 'general';
}

// ================= DATE =================
function getLocalDate() {
  return new Date().toLocaleDateString('en-CA', { timeZone: TZ });
}

function parseDateVN(dateStr, end = false) {
  const [y, m, d] = dateStr.split('-').map(Number);
  const offsetMs = 7 * 60 * 60 * 1000;
  const base = end
    ? Date.UTC(y, m - 1, d, 23, 59, 59, 999) - offsetMs
    : Date.UTC(y, m - 1, d, 0, 0, 0, 0) - offsetMs;
  return new Date(base);
}

function extractDateFromPrompt(text) {
  const match = text.match(/(\d{1,2})[\/\-](\d{1,2})/);
  if (!match) return null;

  const day = parseInt(match[1]);
  const month = parseInt(match[2]);
  const year = new Date().getFullYear();
  const date = new Date(year, month - 1, day);

  return date.toLocaleDateString('en-CA');
}

// ================= DATA =================
async function getParkingData(prompt, today, intent) {
  const text = prompt.toLowerCase();

  const date = extractDateFromPrompt(text) || today;
  const dateRange = { start: date, end: date };

  console.log('[DATA] Date:', dateRange, '| Intent:', intent);

  if (intent === 'peak_entry') return getPeakHours(dateRange, 'entry');
  if (intent === 'peak_exit')  return getPeakHours(dateRange, 'exit');
  if (intent === 'total')      return getDailyTotal(dateRange);
  if (intent === 'avg')        return getAverageTraffic(dateRange);
  if (intent === 'trend')      return getTrend(dateRange);
  if (intent === 'compare')    return getComparisonData(dateRange);

  return null;
}

// ================= QUERY =================

async function getPeakHours({ start, end }, type = 'entry') {
  const s = parseDateVN(start);
  const e = parseDateVN(end, true);

  const timeField = type === 'exit' ? 'exitTime' : 'entryTime';

  const data = await Parking.aggregate([
    {
      $match: {
        [timeField]: { $gte: s, $lte: e, $ne: null },
      },
    },
    {
      $group: {
        _id: {
          $hour: { date: `$${timeField}`, timezone: TZ },
        },
        total: { $sum: 1 },
      },
    },
    { $sort: { total: -1 } },
    { $limit: 5 },
  ]);

  return {
    type: type === 'exit' ? 'Xe ra' : 'Xe vào',
    peakHours: data.map(i => ({ hour: `${i._id}:00`, total: i.total })),
  };
}

async function getDailyTotal({ start, end }) {
  const s = parseDateVN(start);
  const e = parseDateVN(end, true);

  const [totalIn, totalOut] = await Promise.all([
    Parking.countDocuments({ entryTime: { $gte: s, $lte: e } }),
    Parking.countDocuments({ exitTime: { $gte: s, $lte: e, $ne: null } }),
  ]);

  return { totalIn, totalOut, period: `${start} → ${end}` };
}

// ✅ Tách riêng trung bình xe vào và xe ra theo từng giờ
async function getAverageTraffic({ start, end }) {
  const s = parseDateVN(start);
  const e = parseDateVN(end, true);

  const groupByHour = (timeField) =>
    Parking.aggregate([
      { $match: { [timeField]: { $gte: s, $lte: e, $ne: null } } },
      {
        $group: {
          _id: { $hour: { date: `$${timeField}`, timezone: TZ } },
          total: { $sum: 1 },
        },
      },
      { $sort: { _id: 1 } },
    ]);

  const [entryData, exitData] = await Promise.all([
    groupByHour('entryTime'),
    groupByHour('exitTime'),
  ]);

  const calcAvg = (data) =>
    data.length === 0
      ? 0
      : Math.round((data.reduce((a, b) => a + b.total, 0) / data.length) * 10) / 10;

  return {
    entry: {
      avgPerHour: calcAvg(entryData),
      hours: entryData.map(i => ({ hour: `${i._id}:00`, total: i.total })),
    },
    exit: {
      avgPerHour: calcAvg(exitData),
      hours: exitData.map(i => ({ hour: `${i._id}:00`, total: i.total })),
    },
    period: `${start} → ${end}`,
  };
}

// ✅ Tách riêng xu hướng xe vào và xe ra theo ngày
async function getTrend({ start, end }) {
  const s = parseDateVN(start);
  const e = parseDateVN(end, true);

  const groupByDate = (timeField) =>
    Parking.aggregate([
      { $match: { [timeField]: { $gte: s, $lte: e, $ne: null } } },
      {
        $group: {
          _id: {
            $dateToString: {
              format: '%Y-%m-%d',
              date: `$${timeField}`,
              timezone: TZ,
            },
          },
          total: { $sum: 1 },
        },
      },
      { $sort: { _id: 1 } },
    ]);

  const [entryTrend, exitTrend] = await Promise.all([
    groupByDate('entryTime'),
    groupByDate('exitTime'),
  ]);

  return {
    entry: entryTrend.map(i => ({ date: i._id, total: i.total })),
    exit:  exitTrend.map(i => ({ date: i._id, total: i.total })),
    period: `${start} → ${end}`,
  };
}

async function getComparisonData(range) {
  const today = await getDailyTotal(range);
  return { today };
}

// ================= PROMPT =================
function buildUserPrompt(prompt, data) {
  if (!data) return `Câu hỏi: ${prompt}\n\nLưu ý: Không có dữ liệu liên quan.`;

  return `Câu hỏi: ${prompt}\n\nDữ liệu phân tích:\n${JSON.stringify(data, null, 2)}`;
}

module.exports = { getAIResponse };