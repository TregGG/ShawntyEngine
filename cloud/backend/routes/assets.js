const express = require('express');
const router = express.Router();
const db = require('../db');
const { authenticateToken } = require('../middleware/auth');
const multer = require('multer');
const fs = require('fs');
const path = require('path');
const { exec } = require('child_process');
const dgram = require('dgram');

const sendReloadPacket = () => {
    try {
        const client = dgram.createSocket('udp4');
        client.send('reload', 9090, '127.0.0.1', (err) => {
            client.close();
        });
    } catch (err) {
        console.error('Failed to send reload packet:', err);
    }
};

router.use(authenticateToken);

// Middleware to check project ownership
const checkProjectOwnership = (req, res, next) => {
    const projectId = req.params.projectId;
    try {
        const stmt = db.prepare('SELECT id FROM projects WHERE id = ? AND owner_id = ?');
        const project = stmt.get(projectId, req.user.id);
        if (!project) return res.status(404).json({ error: "Project not found or unauthorized" });
        next();
    } catch (err) {
        res.status(500).json({ error: "Server error" });
    }
};

// Multer setup for textures
const storage = multer.diskStorage({
    destination: (req, file, cb) => {
        const projectId = req.params.projectId;
        const dir = path.join(__dirname, '..', 'uploads', projectId, 'textures');
        fs.mkdirSync(dir, { recursive: true });
        cb(null, dir);
    },
    filename: (req, file, cb) => {
        cb(null, 'temp_' + file.originalname);
    }
});
const upload = multer({ storage });

// TEXTURES
router.post('/:projectId/assets/upload', checkProjectOwnership, upload.single('file'), (req, res) => {
    if (!req.file) return res.status(400).json({ error: "No file uploaded" });

    const projectId = req.params.projectId;
    const name = path.parse(req.file.originalname).name;
    const tempPath = req.file.path;
    const dir = path.dirname(tempPath);
    
    const tgaPath = path.join(dir, `${name}.tga`);
    const pngPath = path.join(dir, `${name}.png`);

    // Helper to convert using imagemagick
    const convertImg = (src, dest) => {
        return new Promise((resolve, reject) => {
            exec(`convert "${src}" "${dest}"`, (err) => {
                if (err) {
                    exec(`magick "${src}" "${dest}"`, (err2) => {
                        if (err2) reject(err2);
                        else resolve();
                    });
                } else resolve();
            });
        });
    };

    const processUpload = async () => {
        try {
            const ext = path.extname(req.file.originalname).toLowerCase();
            if (ext === '.tga') {
                fs.copyFileSync(tempPath, tgaPath);
                await convertImg(tempPath, pngPath);
            } else if (ext === '.png') {
                fs.copyFileSync(tempPath, pngPath);
                await convertImg(tempPath, tgaPath);
            } else {
                await convertImg(tempPath, tgaPath);
                await convertImg(tempPath, pngPath);
            }
            fs.unlinkSync(tempPath);
            sendReloadPacket();
            res.json({ status: "ok", name, filename: `${name}.tga` });
        } catch (err) {
            console.error('Image conversion failed:', err);
            if (fs.existsSync(tempPath)) fs.unlinkSync(tempPath);
            res.status(500).json({ error: "Image conversion failed" });
        }
    };

    processUpload();
});

router.get('/:projectId/textures/:name/image', checkProjectOwnership, (req, res) => {
    const projectId = req.params.projectId;
    const name = req.params.name;
    const pngPath = path.join(__dirname, '..', 'uploads', projectId, 'textures', `${name}.png`);
    
    if (fs.existsSync(pngPath)) {
        res.sendFile(pngPath);
    } else {
        res.status(404).json({ error: "Texture not found" });
    }
});

// TEXT ASSETS (spritesheets, animations, objects, scripts, prefabs)
router.get('/:projectId/:type', checkProjectOwnership, (req, res) => {
    const { projectId, type } = req.params;
    try {
        const stmt = db.prepare('SELECT name FROM assets_text WHERE project_id = ? AND type = ?');
        const rows = stmt.all(projectId, type);
        res.json(rows.map(r => r.name));
    } catch (err) {
        res.status(500).json({ error: "Server error" });
    }
});

router.get('/:projectId/:type/:name', checkProjectOwnership, (req, res) => {
    const { projectId, type, name } = req.params;
    try {
        const stmt = db.prepare('SELECT content FROM assets_text WHERE project_id = ? AND type = ? AND name = ?');
        const row = stmt.get(projectId, type, name);
        if (!row) return res.status(404).json({ error: "Asset not found" });
        
        // Match the python backend's response format
        if (type === 'animations') {
            const clips = [];
            let currentClip = null;
            row.content.trim().split('\n').forEach(line => {
                line = line.trim();
                if (line.startsWith('clip:')) {
                    if (currentClip) clips.push(currentClip);
                    currentClip = { name: line.split(':')[1].trim(), frames: [] };
                } else if (currentClip && line.includes(':')) {
                    const parts = line.split(':')[1].trim().split(' ');
                    const frameData = {};
                    parts.forEach(p => {
                        if (p.includes('=')) {
                            const [k, v] = p.split('=');
                            frameData[k] = v.includes('.') ? parseFloat(v) : parseInt(v);
                        }
                    });
                    currentClip.frames.push(frameData);
                }
            });
            if (currentClip) clips.push(currentClip);
            return res.json({ name, clips });
        } else if (type === 'objects') {
            const data = {};
            row.content.trim().split('\n').forEach(line => {
                if (line.includes(':')) {
                    const [k, v] = line.split(':');
                    data[k.trim()] = v.trim();
                }
            });
            return res.json({ name, ...data });
        }
        
        res.json({ name, content: row.content });
    } catch (err) {
        res.status(500).json({ error: "Server error" });
    }
});

router.post('/:projectId/:type', checkProjectOwnership, (req, res) => {
    const { projectId, type } = req.params;
    const body = req.body;
    let name = body.name || 'new_asset';
    let content = body.content || '';

    if (type === 'spritesheets') {
        const texture = body.texture || name;
        const width = body.width || 32;
        const height = body.height || 32;
        const cols = body.cols || 1;
        const rows = body.rows || 1;
        
        const lines = [`texture:${texture}`];
        let idx = 0;
        for (let row = 0; row < rows; row++) {
            for (let col = 0; col < cols; col++) {
                lines.push(`${idx}: x=${col * width} y=${row * height} w=${width} h=${height}`);
                idx++;
            }
        }
        content = lines.join('\n') + '\n';
    } else if (type === 'animations') {
        const clips = body.clips || [{ name: 'idle', frames: [{ frame: 0, duration: 1.0 }] }];
        const lines = [];
        clips.forEach(clip => {
            lines.push(`clip:${clip.name}`);
            clip.frames.forEach((frame, i) => {
                lines.push(`${i}: frame=${frame.frame !== undefined ? frame.frame : i} duration=${frame.duration !== undefined ? frame.duration : 0.1}`);
            });
        });
        content = lines.join('\n') + '\n';
    } else if (type === 'objects') {
        const spritesheet = body.spritesheet || name;
        const animations = body.animations || name;
        content = `spritesheet:${spritesheet}\nanimations:${animations}\n`;
    } else if (type === 'scripts') {
        if (!name.endsWith('.py')) name += '.py';
        const classname = name.replace('.py', '').replace(/_/g, ' ').replace(/\b\w/g, l => l.toUpperCase()).replace(/ /g, '');
        content = `"""\n${name} — Auto-generated script\n"""\nimport shawnty\n\nclass ${classname}:\n    def OnStart(self, entity, input):\n        pass\n\n    def OnUpdate(self, entity, dt, input):\n        pass\n`;
    }

    try {
        const stmt = db.prepare('INSERT INTO assets_text (project_id, type, name, content) VALUES (?, ?, ?, ?)');
        stmt.run(projectId, type, name, content);
        sendReloadPacket();
        res.json({ status: "ok", name });
    } catch (err) {
        if (err.code === 'SQLITE_CONSTRAINT_UNIQUE') {
            res.status(409).json({ error: "Asset already exists" });
        } else {
            res.status(500).json({ error: "Server error" });
        }
    }
});

router.put('/:projectId/:type/:name', checkProjectOwnership, (req, res) => {
    const { projectId, type, name } = req.params;
    let content = req.body.content || '';

    if (type === 'animations' && req.body.clips) {
        const lines = [];
        req.body.clips.forEach(clip => {
            lines.push(`clip:${clip.name}`);
            clip.frames.forEach((frame, i) => {
                lines.push(`${i}: frame=${frame.frame !== undefined ? frame.frame : i} duration=${frame.duration !== undefined ? frame.duration : 0.1}`);
            });
        });
        content = lines.join('\n') + '\n';
    }

    try {
        const stmt = db.prepare('UPDATE assets_text SET content = ? WHERE project_id = ? AND type = ? AND name = ?');
        const info = stmt.run(content, projectId, type, name);
        if (info.changes === 0) return res.status(404).json({ error: "Asset not found" });
        sendReloadPacket();
        res.json({ status: "ok", name });
    } catch (err) {
        res.status(500).json({ error: "Server error" });
    }
});

// Helper for appending spritesheet frames (like local editor)
router.post('/:projectId/spritesheets/:name/append', checkProjectOwnership, upload.single('file'), async (req, res) => {
    if (!req.file) return res.status(400).json({ error: "No file uploaded" });

    const { projectId, name } = req.params;
    
    // Read current spritesheet from DB
    let ssheetContent = '';
    try {
        const stmt = db.prepare('SELECT content FROM assets_text WHERE project_id = ? AND type = "spritesheets" AND name = ?');
        const row = stmt.get(projectId, name);
        if (!row) return res.status(404).json({ error: "Spritesheet not found" });
        ssheetContent = row.content;
    } catch (err) {
        return res.status(500).json({ error: "Database error" });
    }

    const lines = ssheetContent.trim().split('\n');
    let textureName = name;
    const frames = [];

    lines.forEach(line => {
        line = line.trim();
        if (line.startsWith('texture:')) {
            textureName = line.split(':')[1].trim();
        } else if (line.includes(':') && line.includes('=')) {
            const parts = line.split(':');
            const idx = parseInt(parts[0].trim());
            const frameData = {};
            parts[1].trim().split(' ').forEach(p => {
                if (p.includes('=')) {
                    const [k, v] = p.split('=');
                    frameData[k] = parseInt(v);
                }
            });
            frames.push({ idx, ...frameData });
        }
    });

    frames.sort((a, b) => a.idx - b.idx);
    const nextIdx = frames.length;
    let nextX = 0;
    if (frames.length > 0) {
        const last = frames[frames.length - 1];
        nextX = (last.x || 0) + (last.w || 32);
    }

    const tempPath = req.file.path;
    let newW = 32, newH = 32;

    const getDims = (path) => {
        return new Promise((resolve) => {
            exec(`identify -format "%w %h" "${path}"`, (err, stdout) => {
                if (!err && stdout) {
                    const parts = stdout.trim().split(' ');
                    if (parts.length === 2) {
                        newW = parseInt(parts[0]);
                        newH = parseInt(parts[1]);
                    }
                }
                resolve();
            });
        });
    };

    await getDims(tempPath);

    const dir = path.dirname(tempPath);
    const tgaPath = path.join(dir, `${textureName}.tga`);
    const pngPath = path.join(dir, `${textureName}.png`);

    const runAppend = (src, dest) => {
        return new Promise((resolve, reject) => {
            exec(`convert "${dest}" "${src}" +append "${dest}"`, (err) => {
                if (err) {
                    exec(`magick "${dest}" "${src}" +append "${dest}"`, (err2) => {
                        if (err2) reject(err2); else resolve();
                    });
                } else resolve();
            });
        });
    };

    const runConvert = (src, dest) => {
        return new Promise((resolve, reject) => {
            exec(`convert "${src}" "${dest}"`, (err) => {
                if (err) {
                    exec(`magick "${src}" "${dest}"`, (err2) => {
                        if (err2) reject(err2); else resolve();
                    });
                } else resolve();
            });
        });
    };

    try {
        if (fs.existsSync(tgaPath)) {
            await runAppend(tempPath, tgaPath);
        } else {
            await runConvert(tempPath, tgaPath);
        }
        
        // Re-generate PNG
        if (fs.existsSync(pngPath)) fs.unlinkSync(pngPath);
        await runConvert(tgaPath, pngPath);
        
        fs.unlinkSync(tempPath);

        const newFrameLine = `${nextIdx}: x=${nextX} y=0 w=${newW} h=${newH}`;
        const newContent = ssheetContent.trim() + '\n' + newFrameLine + '\n';

        const stmt = db.prepare('UPDATE assets_text SET content = ? WHERE project_id = ? AND type = "spritesheets" AND name = ?');
        stmt.run(newContent, projectId, name);

        sendReloadPacket();
        res.json({ status: "ok", frameIndex: nextIdx, x: nextX, y: 0, w: newW, h: newH });
    } catch (err) {
        if (fs.existsSync(tempPath)) fs.unlinkSync(tempPath);
        res.status(500).json({ error: "Failed to append frame" });
    }
});

module.exports = router;
