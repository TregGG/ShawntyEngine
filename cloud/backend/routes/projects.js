const express = require('express');
const router = express.Router();

// Placeholder to get user's projects
router.get('/', (req, res) => {
    res.json([
        { id: "proj_123", name: "My First Game", serverToken: "token_abc123" }
    ]);
});

// Placeholder to create a new project
router.post('/', (req, res) => {
    res.json({ id: "proj_456", name: req.body.name || "New Project", serverToken: "token_def456" });
});

module.exports = router;
