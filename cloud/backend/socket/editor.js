const crypto = require('crypto');
const db = require('../db');
const { verifyToken } = require('../middleware/auth');
const { unwrapScene } = require('../lib/validate');
const serverManager = require('../services/serverManager');

function sameToken(a, b) {
    const ha = crypto.createHash('sha256').update(String(a)).digest();
    const hb = crypto.createHash('sha256').update(String(b)).digest();
    return crypto.timingSafeEqual(ha, hb);
}

module.exports = function(io) {
    io.on('connection', (socket) => {
        // projectId -> { editor: boolean, expires: seconds | null }
        const access = new Map();

        socket.on('join_project', (projectId, token) => {
            if (typeof projectId !== 'string' || typeof token !== 'string') {
                return socket.emit('error', { message: 'Unauthorized' });
            }
            try {
                const project = db.prepare('SELECT owner_id, server_token FROM projects WHERE id = ?').get(projectId);
                if (!project) return socket.emit('error', { message: 'Unauthorized' });

                let grant = null;
                if (sameToken(token, project.server_token)) {
                    grant = { editor: false, expires: null }; // Game client: may listen, not edit
                } else {
                    try {
                        const user = verifyToken(token);
                        if (user.id === project.owner_id) grant = { editor: true, expires: user.exp };
                    } catch { /* invalid JWT */ }
                }
                if (!grant) return socket.emit('error', { message: 'Unauthorized' });

                access.set(projectId, grant);
                socket.join(projectId);
                const server = serverManager.startServer(projectId);
                socket.emit('joined', { projectId, server, message: `Successfully joined project ${projectId}` });
            } catch (err) {
                console.error('join_project failed:', err);
                socket.emit('error', { message: 'Server error' });
            }
        });

        socket.on('editor_update', (data, ack) => {
            const reply = typeof ack === 'function' ? ack : () => {};
            const { projectId, update } = data || {};
            const grant = access.get(projectId);
            if (!grant || !grant.editor || grant.expires * 1000 < Date.now()) {
                return reply({ ok: false, error: 'Unauthorized' });
            }
            if (!update || typeof update !== 'object' || typeof update.action !== 'string') {
                return reply({ ok: false, error: 'Invalid update' });
            }

            if (update.action === 'save_scene') {
                const scene = unwrapScene(update.scene);
                if (!scene) return reply({ ok: false, error: 'Invalid scene' });
                try {
                    db.prepare('UPDATE projects SET scene_data = ? WHERE id = ?').run(JSON.stringify(scene), projectId);
                } catch (err) {
                    console.error('Failed to save scene:', err);
                    return reply({ ok: false, error: 'Failed to save scene' });
                }
                try {
                    serverManager.reloadServer(projectId);
                } catch (err) {
                    console.error('Game server reload failed:', err);
                }
            }

            socket.to(projectId).emit('scene_update', update);
            reply({ ok: true });
        });
    });
};
