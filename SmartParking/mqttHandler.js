const path = require('path');
const mqtt = require('mqtt');
const { processParkingCapture } = require('./controllers/parkingController');
const runYOLO = require('./utils/runYOLO');

const MQTT_TOPIC = 'parking/capture';

function normalizeStatus(status) {
  return String(status || 'IN').trim().toUpperCase() === 'OUT' ? 'OUT' : 'IN';
} 

function initMqttHandler(io) {
  const brokerUrl = process.env.MQTT_BROKER_URL;

  if (!brokerUrl) {
    console.warn('MQTT_BROKER_URL is not configured. MQTT handler is disabled.');
    return null;
  }

  const client = mqtt.connect(brokerUrl, {
    username: process.env.MQTT_USERNAME || undefined,
    password: process.env.MQTT_PASSWORD || undefined,
    clientId: process.env.MQTT_CLIENT_ID || `smartparking-backend-${Date.now()}`,
  });

  client.on('connect', () => {
    console.log(`MQTT connected to ${brokerUrl}`);
    client.subscribe(MQTT_TOPIC, (error) => {
      if (error) {
        console.error(`Failed to subscribe to ${MQTT_TOPIC}:`, error.message);
        return;
      }

      console.log(`Subscribed to MQTT topic ${MQTT_TOPIC}`);
    });
  });

  client.on('error', (error) => {
    console.error('MQTT error:', error.message);
  });

  client.on('message', async (topic, payload) => {
    if (topic !== MQTT_TOPIC) {
      return;
    }

    let message;
    try {
      message = JSON.parse(payload.toString());
    } catch (error) {
      console.error('Invalid MQTT payload:', error.message);
      return;
    }

    const uid = String(message.uid || '').trim();
    const imageName = String(message.image || '').trim();
    const token = String(message.token || '').trim();
    const status = normalizeStatus(message.status);

    if (!uid || !imageName) {
      console.warn('MQTT message missing uid or image:', message);
      return;
    }

    const imagePath = path.join(__dirname, 'public', 'images', imageName);

    let yoloResult = null;
    try {
      yoloResult = await runYOLO(imagePath, uid, token);
    } catch (error) {
      console.error('YOLO detection failed:', error.message);
      if (error.stderr) {
        console.error(error.stderr);
      }
      return;
    }

    try {
      await processParkingCapture({
        uid,
        status,
        yoloResult,
        io,
      });
    } catch (error) {
      console.error('Failed to process parking capture:', error.message);
    }
  });

  return client;
}

module.exports = initMqttHandler;
