const express = require('express');
const connectDB = require('./config/database');
const dotenv = require('dotenv');

dotenv.config();

const app = express();

app.use(express.json());
app.use(express.urlencoded({ extended: true }));

app.set('view engine', 'ejs');
app.use(express.static('public'));

connectDB();

const authRoutes = require('./routes/auth');
const parkingRoutes = require('./routes/parking');

app.use('/api/auth', authRoutes);
app.use('/api/parking', parkingRoutes);

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
app.listen(PORT, () => console.log(`Server running on port ${PORT}`));