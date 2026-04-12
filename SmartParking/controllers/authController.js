const bcrypt = require('bcryptjs');
const jwt = require('jsonwebtoken');
const crypto = require('crypto');
const User = require('../models/User');
const PendingAdmin = require('../models/PendingAdmin');
const { sendAdminVerificationPin } = require('../services/email');

const PIN_EXPIRY_MINUTES = 5;

function generatePin() {
  return String(crypto.randomInt(100000, 1000000));
}

exports.signup = async (req, res) => {
  try {
    const { username, email, password, role } = req.body;
    const normalizedEmail = (email || '').trim().toLowerCase();
    const chosenRole = role === 'admin' ? 'admin' : 'user';

    if (!username || !normalizedEmail || !password) {
      return res.status(400).json({ message: 'Username, email, and password are required' });
    }

    const existingUser = await User.findOne({ email: normalizedEmail });
    if (existingUser) {
      return res.status(400).json({ message: 'Email already registered' });
    }

    if (chosenRole === 'user') {
      const hashedPassword = await bcrypt.hash(password, 10);
      await User.create({
        username: username.trim(),
        email: normalizedEmail,
        password: hashedPassword,
        role: 'user',
      });
      return res.status(201).json({ message: 'User created', requiresVerification: false });
    }

    const hashedPassword = await bcrypt.hash(password, 10);
    const plainPin = generatePin();
    const pinHash = await bcrypt.hash(plainPin, 10);
    const expiresAt = new Date(Date.now() + PIN_EXPIRY_MINUTES * 60 * 1000);

    await PendingAdmin.deleteMany({ email: normalizedEmail });

    await PendingAdmin.create({
      username: username.trim(),
      email: normalizedEmail,
      password: hashedPassword,
      pinHash,
      expiresAt,
    });

    try {
      await sendAdminVerificationPin({
        applicantUsername: username.trim(),
        applicantEmail: normalizedEmail,
        plainPin,
        expiresMinutes: PIN_EXPIRY_MINUTES,
      });
    } catch (mailErr) {
      await PendingAdmin.deleteMany({ email: normalizedEmail });
      return res.status(503).json({
        message: 'Could not send verification email. Check SMTP settings or try again later.',
        detail: mailErr.message,
      });
    }

    return res.status(202).json({
      message: 'Admin registration pending. A PIN was sent to the organization email for approval.',
      requiresVerification: true,
      email: normalizedEmail,
    });
  } catch (err) {
    res.status(400).json({ message: err.message });
  }
};

exports.verifyAdminPin = async (req, res) => {
  try {
    const { email, pin } = req.body;
    const normalizedEmail = (email || '').trim().toLowerCase();
    const pinStr = (pin || '').toString().trim();

    if (!normalizedEmail || !pinStr) {
      return res.status(400).json({ message: 'Email and PIN are required' });
    }

    const pending = await PendingAdmin.findOne({ email: normalizedEmail });
    if (!pending) {
      return res.status(400).json({ message: 'No pending admin registration found for this email' });
    }

    if (new Date() > pending.expiresAt) {
      await PendingAdmin.deleteOne({ _id: pending._id });
      return res.status(400).json({ message: 'PIN expired. Please register again as admin.' });
    }

    const pinOk = await bcrypt.compare(pinStr, pending.pinHash);
    if (!pinOk) {
      return res.status(400).json({ message: 'Invalid PIN' });
    }

    const exists = await User.findOne({ email: normalizedEmail });
    if (exists) {
      await PendingAdmin.deleteOne({ _id: pending._id });
      return res.status(400).json({ message: 'Account already exists for this email' });
    }

    await User.create({
      username: pending.username,
      email: pending.email,
      password: pending.password,
      role: 'admin',
    });

    await PendingAdmin.deleteOne({ _id: pending._id });

    return res.status(201).json({ message: 'Admin account created. You can log in now.' });
  } catch (err) {
    res.status(500).json({ message: err.message });
  }
};

exports.signin = async (req, res) => {
  try {
    const { email, password } = req.body;
    const user = await User.findOne({ email: (email || '').trim().toLowerCase() });
    if (!user || !await bcrypt.compare(password, user.password)) {
      return res.status(400).json({ message: 'Invalid credentials' });
    }
    const token = jwt.sign(
      { id: user._id.toString(), role: user.role, username: user.username, email: user.email },
      process.env.JWT_SECRET
    );
    res.json({ token });
  } catch (err) {
    res.status(500).json({ message: err.message });
  }
};

exports.signout = (req, res) => {
  res.json({ message: 'Signed out' });
};

exports.me = async (req, res) => {
  try {
    const user = await User.findById(req.user.id).select('-password');
    if (!user) return res.status(404).json({ message: 'User not found' });
    res.json(user);
  } catch (err) {
    res.status(500).json({ message: err.message });
  }
};
