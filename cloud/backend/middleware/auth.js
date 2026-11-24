const jwt = require('jsonwebtoken');
const db = require('../db');
const { jwtSecret } = require('../config');

const ALGORITHM = 'HS256';

function signToken(user) {
    return jwt.sign({ id: user.id, email: user.email, v: user.token_version }, jwtSecret, { expiresIn: '7d', algorithm: ALGORITHM });
}

function verifyToken(token) {
    const claims = jwt.verify(token, jwtSecret, { algorithms: [ALGORITHM] });
    // A password reset bumps token_version, which revokes older tokens
    const user = db.prepare('SELECT token_version FROM users WHERE id = ?').get(claims.id);
    if (!user || user.token_version !== claims.v) throw new Error('Token revoked');
    return claims;
}

function authenticateToken(req, res, next) {
    const authHeader = req.headers['authorization'];
    const token = authHeader && authHeader.split(' ')[1]; // Format: "Bearer TOKEN"
    if (!token) return res.sendStatus(401);

    try {
        req.user = verifyToken(token);
    } catch {
        return res.sendStatus(403);
    }
    next();
}

module.exports = { authenticateToken, signToken, verifyToken };
