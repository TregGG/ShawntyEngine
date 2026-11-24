const express = require('express');
const router = express.Router();
const db = require('../db');
const { authenticateToken } = require('../middleware/auth');
const multer = require('multer');
const fs = require('fs');
const path = require('path');
const { execFile } = require('child_process');
const { promisify } = require('util');
const crypto = require('crypto');
const os = require('os');
const { uploadsDir } = require('../config');
const { ASSET_TYPES, isSafeName, isSafeLine } = require('../lib/validate');
const serverManager = require('../services/serverManager');

const run = promisify(execFile);
const MAX_FRAMES = 4096;
const IMAGE_FORMATS = { '.png': 'png', '.tga': 'tga', '.jpg': 'jpeg', '.jpeg': 'jpeg', '.gif': 'gif', '.webp': 'webp', '.bmp': 'bmp' };
const LIMITS = ['-limit', 'memory', '256MiB', '-limit', 'map', '256MiB', '-limit', 'disk', '0',
    '-limit', 'width', '8192', '-limit', 'height', '8192', '-limit', 'time', '20'];

// Never through a shell. ImageMagick 7 is `magick`; version 6 is `convert`/`identify`.
async function imageMagick(tool, args) {
    const options = { timeout: 30000 };
    try {
        return await run('magick', tool === 'convert' ? args : [tool, ...args], options);
    } catch (err) {
        if (err.code !== 'ENOENT') throw err;
        return run(tool, args, options);
    }
}

// Explicit formats, so file contents cannot select another ImageMagick coder.
// [0] takes only the first frame of animated inputs.
const inputImage = (file) => `${IMAGE_FORMATS[path.extname(file).toLowerCase()]}:${file}[0]`;
const outputImage = (file) => `${IMAGE_FORMATS[path.extname(file).toLowerCase()]}:${file}`;

// Temporary files keep readers from seeing half-written images.
async function writeImage(inputs, dest, extra = []) {
    const ext = path.extname(dest);
    const temp = path.join(os.tmpdir(), `shawnty_${crypto.randomUUID()}${ext}`);
    try {
        await imageMagick('convert', [...LIMITS, ...inputs.map(inputImage), ...extra, outputImage(temp)]);
        await fs.promises.copyFile(temp, dest);
    } finally {
        await fs.promises.rm(temp, { force: true });
    }
}

const texturesDirFor = (projectId) => path.join(uploadsDir, projectId, 'textures');

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

const upload = multer({
    storage: multer.diskStorage({
        destination: os.tmpdir(),
        filename: (req, file, cb) => cb(null, `shawnty_${crypto.randomUUID()}${path.extname(file.originalname).toLowerCase()}`)
    }),
    limits: { fileSize: 10 * 1024 * 1024, files: 1 },
    fileFilter: (req, file, cb) => {
        const ok = path.extname(file.originalname).toLowerCase() in IMAGE_FORMATS;
        cb(ok ? null : Object.assign(new Error('Unsupported image type'), { status: 400 }), ok);
    }
});

router.param('type', (req, res, next, type) => {
    if (!ASSET_TYPES.includes(type)) return res.status(404).json({ error: "Unknown asset type" });
    next();
});
router.param('name', (req, res, next, name) => {
    if (!isSafeName(name)) return res.status(400).json({ error: "Invalid name" });
    next();
});

function isValidClips(clips) {
    return Array.isArray(clips) && clips.length <= 256 && clips.every(c =>
        c && isSafeLine(c.name) && Array.isArray(c.frames) && c.frames.length <= MAX_FRAMES &&
        c.frames.every(f => f && [f.frame, f.duration].every(v => v === undefined || Number.isFinite(v))));
}

// Returns the sheet text, or null if the grid is invalid.
function gridSheet(body, name) {
    const { texture = name, width = 32, height = 32, cols = 1, rows = 1 } = body;
    const ints = [width, height, cols, rows];
    if (!isSafeName(texture) || !ints.every(v => Number.isInteger(v) && v > 0 && v <= 8192) || cols * rows > MAX_FRAMES) return null;
    const lines = [`texture:${texture}`];
    for (let i = 0; i < cols * rows; i++) {
        lines.push(`${i}: x=${(i % cols) * width} y=${Math.floor(i / cols) * height} w=${width} h=${height}`);
    }
    return lines.join('\n') + '\n';
}

// TEXTURES
router.post('/:projectId/assets/upload', checkProjectOwnership, upload.single('file'), async (req, res) => {
    if (!req.file) return res.status(400).json({ error: "No file uploaded" });

    const tempPath = req.file.path;
    try {
        // Spaces and symbols in uploaded names become underscores
        const name = path.parse(req.file.originalname).name
            .replace(/[^A-Za-z0-9_-]+/g, '_').replace(/^-+/, '').slice(0, 90);
        if (!isSafeName(name)) return res.status(400).json({ error: "Invalid file name" });

        const dir = texturesDirFor(req.params.projectId);
        fs.mkdirSync(dir, { recursive: true });
        // The engine reads only uncompressed, bottom-up 24/32-bit TGA
        await writeImage([tempPath], path.join(dir, `${name}.png`));
        await writeImage([tempPath], path.join(dir, `${name}.tga`), ['-compress', 'none', '-type', 'TrueColorAlpha']);
        serverManager.reloadServer(req.params.projectId);
        res.json({ status: "ok", name, filename: `${name}.tga` });
    } catch (err) {
        console.error('Image conversion failed:', err.message);
        res.status(422).json({ error: "Image conversion failed" });
    } finally {
        fs.rmSync(tempPath, { force: true });
    }
});

router.get('/:projectId/textures/:name/image', checkProjectOwnership, (req, res) => {
    const projectId = req.params.projectId;
    const name = req.params.name;
    const pngPath = path.join(texturesDirFor(projectId), `${name}.png`);
    
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
    const body = req.body || {};
    let name = body.name || 'new_asset';
    let content = body.content || '';
    if (!isSafeName(name)) return res.status(400).json({ error: "Invalid name" });
    if (typeof content !== 'string') return res.status(400).json({ error: "Invalid content" });

    if (type === 'spritesheets') {
        content = gridSheet(body, name);
        if (content === null) return res.status(400).json({ error: "Invalid spritesheet grid" });
    } else if (type === 'animations') {
        const clips = body.clips || [{ name: 'idle', frames: [{ frame: 0, duration: 1.0 }] }];
        if (!isValidClips(clips)) return res.status(400).json({ error: "Invalid clips" });
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
        if (!isSafeName(spritesheet) || !isSafeName(animations)) return res.status(400).json({ error: "Invalid name" });
        content = `spritesheet:${spritesheet}\nanimations:${animations}\n`;
    } else if (type === 'scripts') {
        if (!name.endsWith('.py')) name += '.py';
        const classname = name.replace('.py', '').replace(/_/g, ' ').replace(/\b\w/g, l => l.toUpperCase()).replace(/ /g, '');
        content = `"""\n${name} — Auto-generated script\n"""\nimport shawnty\n\nclass ${classname}:\n    def OnStart(self, entity, input):\n        pass\n\n    def OnUpdate(self, entity, dt, input):\n        pass\n`;
    }

    try {
        const stmt = db.prepare('INSERT INTO assets_text (project_id, type, name, content) VALUES (?, ?, ?, ?)');
        stmt.run(projectId, type, name, content);
        serverManager.reloadServer(req.params.projectId);
        res.json({ status: "ok", name });
    } catch (err) {
        if (db.isUniqueViolation(err)) {
            res.status(409).json({ error: "Asset already exists" });
        } else {
            res.status(500).json({ error: "Server error" });
        }
    }
});

router.put('/:projectId/:type/:name', checkProjectOwnership, (req, res) => {
    const { projectId, type, name } = req.params;
    const body = req.body || {};
    let content = body.content || '';

    if (typeof content !== 'string') return res.status(400).json({ error: "Invalid content" });
    if (type === 'spritesheets' && body.cols !== undefined) {
        content = gridSheet(body, name);
        if (content === null) return res.status(400).json({ error: "Invalid spritesheet grid" });
    } else if (type === 'animations' && body.clips) {
        if (!isValidClips(body.clips)) return res.status(400).json({ error: "Invalid clips" });
        const lines = [];
        body.clips.forEach(clip => {
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
        serverManager.reloadServer(req.params.projectId);
        res.json({ status: "ok", name });
    } catch (err) {
        res.status(500).json({ error: "Server error" });
    }
});

// Helper for appending spritesheet frames (like local editor)
router.post('/:projectId/spritesheets/:name/append', checkProjectOwnership, upload.single('file'), async (req, res) => {
    if (!req.file) return res.status(400).json({ error: "No file uploaded" });

    const { projectId, name } = req.params;
    const tempPath = req.file.path;

    try {
        const row = db.prepare("SELECT content FROM assets_text WHERE project_id = ? AND type = 'spritesheets' AND name = ?").get(projectId, name);
        if (!row) return res.status(404).json({ error: "Spritesheet not found" });
        const ssheetContent = row.content;

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

    if (!isSafeName(textureName)) return res.status(400).json({ error: "Invalid texture name" });

    const { stdout } = await imageMagick('identify', [...LIMITS, '-format', '%w %h', inputImage(tempPath)]);
    const [newW, newH] = stdout.trim().split(' ').map(Number);

    const dir = texturesDirFor(projectId);
    fs.mkdirSync(dir, { recursive: true });
    const tgaPath = path.join(dir, `${textureName}.tga`);
    const pngPath = path.join(dir, `${textureName}.png`);
    const tgaOptions = ['-compress', 'none', '-type', 'TrueColorAlpha'];

    const sources = fs.existsSync(tgaPath) ? [tgaPath, tempPath] : [tempPath];
    await writeImage(sources, tgaPath, sources.length > 1 ? ['+append', ...tgaOptions] : tgaOptions);
    await writeImage([tgaPath], pngPath);

    const newContent = `${ssheetContent.trim()}\n${nextIdx}: x=${nextX} y=0 w=${newW} h=${newH}\n`;
    db.prepare("UPDATE assets_text SET content = ? WHERE project_id = ? AND type = 'spritesheets' AND name = ?").run(newContent, projectId, name);

    serverManager.reloadServer(projectId);
    res.json({ status: "ok", frameIndex: nextIdx, x: nextX, y: 0, w: newW, h: newH });
    } catch (err) {
        console.error('Append frame failed:', err.message);
        res.status(500).json({ error: "Failed to append frame" });
    } finally {
        fs.rmSync(tempPath, { force: true });
    }
});

module.exports = router;
