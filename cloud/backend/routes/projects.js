const express = require('express');
const router = express.Router();
const db = require('../db');
const { authenticateToken } = require('../middleware/auth');
const crypto = require('crypto');

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
        const { name } = req.body;
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
        
        const stmt = db.prepare('INSERT INTO projects (id, name, owner_id, server_token, scene_data) VALUES (?, ?, ?, ?, ?)');
        stmt.run(projectId, name || "New Project", req.user.id, serverToken, JSON.stringify(defaultScene));
        
        res.json({ id: projectId, name: name || "New Project", serverToken });
    } catch (err) {
        res.status(500).json({ error: "Server error" });
    }
});

// Get project scene
router.get('/:id/scene', (req, res) => {
    try {
        const stmt = db.prepare('SELECT name, scene_data FROM projects WHERE id = ? AND owner_id = ?');
        const project = stmt.get(req.params.id, req.user.id);
        
        if (!project) return res.status(404).json({ error: "Project not found" });
        
        res.json({ name: project.name, scene: JSON.parse(project.scene_data) });
    } catch (err) {
        res.status(500).json({ error: "Server error" });
    }
});

module.exports = router;
