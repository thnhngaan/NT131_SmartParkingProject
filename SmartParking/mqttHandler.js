const path = require('path');
const fs = require('fs');
const axios = require('axios');
const mqtt = require('mqtt');
const { processParkingCapture } = require('./controllers/parkingController');
const runYOLO = require('./utils/runYOLO');

const MQTT_TOPICS = ['parking/cam/in/cmd', 'parking/exit/request'];
const IMAGES_DIR = path.join(__dirname, 'public', 'images');

function normalizeStatus(status) {
  return String(status || 'IN').trim().toUpperCase() === 'OUT' ? 'OUT' : 'IN';
}

/**
 * Downloads an image from a URL and saves it to the images directory.
 * @param {string} url - The URL to download the image from.
 * @param {string} filename - The filename to save the image as.
 * @returns {Promise<string>} - The full path to the saved image.
 */
async function downloadImage(url, filename) {
  const destPath = path.join(IMAGES_DIR, filename);

  // Ensure the images directory exists
  if (!fs.existsSync(IMAGES_DIR)) {
    fs.mkdirSync(IMAGES_DIR, { recursive: true });
  }

  let response;
  try {
    response = await axios.get(url, {
      responseType: 'stream',
      timeout: 10000, // 10s timeout
    });
  } catch (error) {
    throw new Error(`Failed to download image from ${url}: ${error.message}`);
  }

  return new Promise((resolve, reject) => {
    const writer = fs.createWriteStream(destPath);
    response.data.pipe(writer);
    writer.on('finish', () => resolve(destPath));
    writer.on('error', (err) => {
      fs.unlink(destPath, () => {}); // clean up partial file
      reject(new Error(`Failed to save image to ${destPath}: ${err.message}`));
    });
  });
}

/**
 * Resolves the image path from the MQTT message.
 * Supports both `photo_url` (download) and `image` (local file) fields.
 * @param {object} message - The parsed MQTT message.
 * @param {string} token - Used to generate a unique filename for downloaded images.
 * @returns {Promise<string>} - The resolved local image path.
 */
async function resolveImagePath(message, token) {
  const photoUrl = String(message.photo_url || '').trim();
  const imageName = String(message.image || '').trim();

  if (photoUrl) {
    const timestamp = token || Date.now();
    const filename = `capture_${timestamp}.jpg`;
    console.log(`[MQTT] Image source: URL → downloading from ${photoUrl}`);
    const savedPath = await downloadImage(photoUrl, filename);
    console.log(`[MQTT] Image downloaded and saved to: ${savedPath}`);
    return savedPath;
  }

  if (imageName) {
    const localPath = path.join(IMAGES_DIR, imageName);
    console.log(`[MQTT] Image source: local file → ${localPath}`);
    return localPath;
  }

  return null;
}

function initMqttHandler(io) {
  const brokerUrl = process.env.MQTT_BROKER_URL;

  if (!brokerUrl) {
    console.warn('[MQTT] MQTT_BROKER_URL is not configured. MQTT handler is disabled.');
    return null;
  }

  const client = mqtt.connect(brokerUrl, {
    username: process.env.MQTT_USERNAME || undefined,
    password: process.env.MQTT_PASSWORD || undefined,
    clientId: process.env.MQTT_CLIENT_ID || `smartparking-backend-${Date.now()}`,
  });

  client.on('connect', () => {
    console.log(`[MQTT] Connected to broker: ${brokerUrl}`);
    MQTT_TOPICS.forEach((topic) => {
      client.subscribe(topic, (error) => {
        if (error) {
          console.error(`[MQTT] Failed to subscribe to ${topic}:`, error.message);
          return;
        }
        console.log(`[MQTT] Subscribed to topic: ${topic}`);
      });
    });
  });

  client.on('error', (error) => {
    console.error('[MQTT] Connection error:', error.message);
  });

  client.on('message', async (topic, payload) => {
    if (!MQTT_TOPICS.includes(topic)) return;

    console.log(`[MQTT] Message received on topic: ${topic}`);

    // --- Parse payload ---
    let message;
    try {
      message = JSON.parse(payload.toString());
    } catch (error) {
      console.error('[MQTT] Invalid JSON payload:', error.message);
      return;
    }

    console.log('[MQTT] Parsed message:', message);

    // --- Extract & normalize fields ---
    const uid = String(message.uid || '').trim().toUpperCase();
    const token = String(message.token || '').trim();
    const status = normalizeStatus(message.status);

    if (!uid) {
      console.warn('[MQTT] Missing uid in message:', message);
      return;
    }

    // --- Validate image source ---
    const hasPhotoUrl = Boolean(String(message.photo_url || '').trim());
    const hasImage = Boolean(String(message.image || '').trim());

    if (!hasPhotoUrl && !hasImage) {
      console.warn('[MQTT] Message missing both `photo_url` and `image`. Skipping:', message);
      return;
    }

    // --- Resolve image path (download or local) ---
    let imagePath;
    try {
      imagePath = await resolveImagePath(message, token);
    } catch (error) {
      console.error('[MQTT] Failed to resolve image path:', error.message);
      return;
    }

    console.log(`[MQTT] Final image path: ${imagePath}`);

    // --- Run YOLO detection ---
    let yoloResult = null;
    try {
      yoloResult = await runYOLO(imagePath, uid, token);
    } catch (error) {
      console.error('[MQTT] YOLO detection failed:', error.message);
      if (error.stderr) console.error('[MQTT] YOLO stderr:', error.stderr);
      return;
    }

    // --- Process parking capture ---
    try {
      const imageUrl = `/images/${path.basename(imagePath)}`;
      console.log(`[MQTT] Passing imageUrl to processParkingCapture: ${imageUrl}`);
      await processParkingCapture({ uid, status, yoloResult, imageUrl, io });
    } catch (error) {
      console.error('[MQTT] Failed to process parking capture:', error.message);
    }
  });

  return client;
}

module.exports = initMqttHandler;
