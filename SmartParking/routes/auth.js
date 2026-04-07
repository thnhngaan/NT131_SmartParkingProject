const express = require('express');
const router = express.Router();
const { signup, signin, signout, me } = require('../controllers/authController');

router.post('/signup', signup);
router.post('/signin', signin);
router.post('/signout', signout);
router.get('/me', require('../middleware/auth').auth, me);

module.exports = router;