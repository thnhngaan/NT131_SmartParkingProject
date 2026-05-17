const nodemailer = require('nodemailer');

const ORG_OWNER_EMAIL = process.env.ORG_OWNER_EMAIL || 'nganthanh368108@gmail.com';

function createTransport() {
  if (!process.env.SMTP_USER || !process.env.SMTP_PASS) {
    return null;
  }
  return nodemailer.createTransport({
    host: process.env.SMTP_HOST || 'smtp.gmail.com',
    port: Number(process.env.SMTP_PORT) || 587,
    secure: process.env.SMTP_SECURE === 'true',
    auth: {
      user: process.env.SMTP_USER,
      pass: process.env.SMTP_PASS,
    },
  });
}

async function sendAdminVerificationPin({ applicantUsername, applicantEmail, plainPin, expiresMinutes }) {
  const transporter = createTransport();
  if (!transporter) {
    throw new Error('Email is not configured (SMTP_USER / SMTP_PASS missing)');
  }

  const subject = 'Smart Parking — Admin registration verification PIN';
  const text = [
    'A new admin account registration was submitted.',
    '',
    `Applicant username: ${applicantUsername}`,
    `Applicant email: ${applicantEmail}`,
    '',
    `Verification PIN: ${plainPin}`,
    `This PIN expires in ${expiresMinutes} minutes.`,
    '',
    'Share this PIN only with the applicant after you approve the request.',
    'The applicant must enter this PIN on the verification page to complete registration.',
  ].join('\n');

  await transporter.sendMail({
    from: process.env.SMTP_FROM || process.env.SMTP_USER,
    to: ORG_OWNER_EMAIL,
    subject,
    text,
  });
}

module.exports = { sendAdminVerificationPin, ORG_OWNER_EMAIL };
