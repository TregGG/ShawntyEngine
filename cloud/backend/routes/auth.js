const express = require('express');
const router = express.Router();
const bcrypt = require('bcryptjs');
const { rateLimit } = require('express-rate-limit');
const db = require('../db');
const { signToken } = require('../middleware/auth');

const MIN_PASSWORD = 10;
const MAX_PASSWORD_BYTES = 72; // bcrypt ignores anything longer
// Compared against when the account doesn't exist, so timing doesn't reveal it
const DUMMY_HASH = bcrypt.hashSync('no-such-account', 12);

router.use(rateLimit({
    windowMs: 15 * 60 * 1000,
    limit: 30,
    standardHeaders: 'draft-8',
    legacyHeaders: false,
    message: { error: 'Too many attempts. Try again later.' }
}));

router.post('/register', async (req, res) => {
    try {
        const { email, password } = req.body || {};
        if (typeof email !== 'string' || typeof password !== 'string') {
            return res.status(400).json({ error: "Email and password required" });
        }
        if (email.length > 254 || !/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email)) {
            return res.status(400).json({ error: "Invalid email" });
        }
        if (password.length < MIN_PASSWORD || Buffer.byteLength(password) > MAX_PASSWORD_BYTES) {
            return res.status(400).json({ error: `Password must be ${MIN_PASSWORD} to ${MAX_PASSWORD_BYTES} characters` });
        }

        const hash = await bcrypt.hash(password, 12);
        const info = db.prepare('INSERT INTO users (email, password_hash) VALUES (?, ?)').run(email, hash);
        res.json({ message: "User registered successfully", userId: info.lastInsertRowid });
    } catch (err) {
        if (db.isUniqueViolation(err)) return res.status(400).json({ error: "Email already exists" });
        console.error('Register failed:', err);
        res.status(500).json({ error: "Server error" });
    }
});

router.post('/login', async (req, res) => {
    try {
        const { email, password } = req.body || {};
        if (typeof email !== 'string' || typeof password !== 'string') {
            return res.status(400).json({ error: "Invalid credentials" });
        }
        const user = db.prepare('SELECT * FROM users WHERE email = ?').get(email);
        const match = await bcrypt.compare(password, user ? user.password_hash : DUMMY_HASH);
        if (!user || !match) return res.status(400).json({ error: "Invalid credentials" });

        res.json({ token: signToken(user), user: { id: user.id, email: user.email } });
    } catch (err) {
        console.error('Login failed:', err);
        res.status(500).json({ error: "Server error" });
    }
});

module.exports = router;
