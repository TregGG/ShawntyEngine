const express = require('express');
const router = express.Router();
const bcrypt = require('bcryptjs');
const jwt = require('jsonwebtoken');
const db = require('../db');
const { JWT_SECRET } = require('../middleware/auth');

router.post('/register', async (req, res) => {
    try {
        const { email, password } = req.body;
        if (!email || !password) return res.status(400).json({ error: "Email and password required" });
        
        const hash = await bcrypt.hash(password, 10);
        
        const stmt = db.prepare('INSERT INTO users (email, password_hash) VALUES (?, ?)');
        const info = stmt.run(email, hash);
        
        res.json({ message: "User registered successfully", userId: info.lastInsertRowid });
    } catch (err) {
        if (err.message.includes('UNIQUE constraint failed')) {
            return res.status(400).json({ error: "Email already exists" });
        }
        res.status(500).json({ error: "Server error" });
    }
});

router.post('/login', async (req, res) => {
    try {
        const { email, password } = req.body;
        const stmt = db.prepare('SELECT * FROM users WHERE email = ?');
        const user = stmt.get(email);
        
        if (!user) return res.status(400).json({ error: "Invalid credentials" });
        
        const match = await bcrypt.compare(password, user.password_hash);
        if (!match) return res.status(400).json({ error: "Invalid credentials" });
        
        const token = jwt.sign({ id: user.id, email: user.email }, JWT_SECRET, { expiresIn: '7d' });
        res.json({ token, user: { id: user.id, email: user.email } });
    } catch (err) {
        res.status(500).json({ error: "Server error" });
    }
});

module.exports = router;
