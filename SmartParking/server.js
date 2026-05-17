const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const connectDB = require('./config/database');
const dotenv = require('dotenv');
const initMqttHandler = require('./mqttHandler');

dotenv.config();

const app = express();
const server = http.createServer(app);
const io = new Server(server, {
  cors: {
    origin: process.env.SOCKET_IO_CORS_ORIGIN || '*',
  },
});

app.use(express.json());
app.use(express.urlencoded({ extended: true }));

app.set('view engine', 'ejs');
app.set('io', io);
app.use(express.static('public'));

connectDB();

const authRoutes = require('./routes/auth');
const parkingRoutes = require('./routes/parking');
const aiRoutes = require('./routes/ai');

app.use('/api/auth', authRoutes);
app.use('/api/parking', parkingRoutes);
app.use('/api/ai', aiRoutes);

// Frontend routes
app.get('/', (req, res) => res.render('index'));
app.get('/login', (req, res) => res.render('login'));
app.get('/signup', (req, res) => res.render('signup'));
app.get('/verify-admin', (req, res) => res.render('verify-admin'));
app.get('/dashboard', (req, res) => res.render('dashboard'));
app.get('/analytics', (req, res) => res.render('analytics'));

const errorHandler = require('./middleware/error');
app.use(errorHandler);

const PORT = process.env.PORT || 3000;

io.on('connection', (socket) => {
  console.log(`Socket connected: ${socket.id}`);

  socket.on('disconnect', () => {
    console.log(`Socket disconnected: ${socket.id}`);
  });
});

initMqttHandler(io);

server.listen(PORT, () => console.log(`Server running on port ${PORT}`));