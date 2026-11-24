const express = require('express');
const router = express.Router();
const db = require('../db');
const { authenticateToken } = require('../middleware/auth');
const crypto = require('crypto');

const { rateLimit } = require('express-rate-limit');
const { buildProjectBundle } = require('../services/bundler');
const serverManager = require('../services/serverManager');
const { unwrapScene } = require('../lib/validate');

router.use('/by-token', rateLimit({ windowMs: 60 * 1000, limit: 60, standardHeaders: 'draft-8', legacyHeaders: false }));

// Public route to get project info by server_token
router.get('/by-token/:token/info', (req, res) => {
    try {
        const stmt = db.prepare('SELECT id, name, created_at FROM projects WHERE server_token = ?');
        const project = stmt.get(req.params.token);
        if (!project) return res.status(404).json({ error: "Project not found for given token" });
        res.setHeader('Cache-Control', 'no-store');
        res.json(project);
    } catch (err) {
        res.status(500).json({ error: "Server error" });
    }
});

// Public route to download full project bundle by server_token
router.get('/by-token/:token/bundle', (req, res) => {
    try {
        const stmt = db.prepare('SELECT id, name FROM projects WHERE server_token = ?');
        const project = stmt.get(req.params.token);
        if (!project) return res.status(404).json({ error: "Project not found for given token" });

        const zipBuffer = buildProjectBundle(project.id);
        if (!zipBuffer) return res.status(500).json({ error: "Failed to generate project bundle" });

        res.setHeader('Cache-Control', 'no-store');
        res.setHeader('Content-Type', 'application/zip');
        res.setHeader('Content-Disposition', `attachment; filename="${project.name.replace(/[^a-zA-Z0-9_-]/g, '_')}.spack"`);
        res.send(zipBuffer);
    } catch (err) {
        console.error(err);
        res.status(500).json({ error: "Server error" });
    }
});

router.use(authenticateToken);

// Get user's projects
router.get('/', (req, res) => {
    try {
        const stmt = db.prepare('SELECT id, name, server_token, created_at FROM projects WHERE owner_id = ? ORDER BY created_at DESC');
        const projects = stmt.all(req.user.id);
        res.json(projects);
    } catch (err) {
        res.status(500).json({ error: "Server error" });
    }
});

// Create a new project
router.post('/', (req, res) => {
    try {
        const name = (req.body || {}).name || "New Project";
        if (typeof name !== 'string' || name.length > 100) return res.status(400).json({ error: "Invalid project name" });
        const projectId = 'proj_' + crypto.randomBytes(6).toString('hex');
        const serverToken = 'token_' + crypto.randomBytes(16).toString('hex');
        
        // Default empty scene
        const defaultScene = {
            entities: [
                {
                    editorId: 'entity_camera',
                    name: 'Camera',
                    category: 'Environment',
                    components: {
                        transform: { position: [0, 0], size: [1, 1], rotation: 0 }
                    }
                }
            ],
            relationships: []
        };
        
        const stmt = db.prepare('INSERT INTO projects (id, name, owner_id, server_token, scene_data, join_code) VALUES (?, ?, ?, ?, ?, ?)');
        stmt.run(projectId, name, req.user.id, serverToken, JSON.stringify(defaultScene), db.newJoinCode());
        
        res.json({ id: projectId, name, serverToken });
    } catch (err) {
        res.status(500).json({ error: "Server error" });
    }
});

// Replace the join code; the old one stops working for new players
router.post('/:id/join-code', (req, res) => {
    try {
        const code = db.newJoinCode();
        const info = db.prepare('UPDATE projects SET join_code = ? WHERE id = ? AND owner_id = ?').run(code, req.params.id, req.user.id);
        if (info.changes === 0) return res.status(404).json({ error: "Project not found" });
        serverManager.reloadServer(req.params.id);
        res.json({ joinCode: code });
    } catch (err) {
        console.error(err);
        res.status(500).json({ error: "Server error" });
    }
});

// Get project scene
router.get('/:id/scene', (req, res) => {
    try {
        const stmt = db.prepare('SELECT name, scene_data FROM projects WHERE id = ? AND owner_id = ?');
        const project = stmt.get(req.params.id, req.user.id);
        
        if (!project) return res.status(404).json({ error: "Project not found" });
        
        res.json({ name: project.name, scene: unwrapScene(JSON.parse(project.scene_data)) || { entities: [], relationships: [] } });
    } catch (err) {
        res.status(500).json({ error: "Server error" });
    }
});

module.exports = router;
