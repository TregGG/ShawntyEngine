const db = require('../db');
const jwt = require('jsonwebtoken');
const { JWT_SECRET } = require('../middleware/auth');

module.exports = function(io) {
    io.on('connection', (socket) => {
        console.log(`Client connected: ${socket.id}`);

        socket.on('join_project', (projectId, token) => {
            try {
                // Determine if token is JWT (editor) or server_token (game client)
                const stmt = db.prepare('SELECT owner_id, server_token FROM projects WHERE id = ?');
                const project = stmt.get(projectId);
                
                if (!project) return socket.emit('error', { message: 'Project not found' });

                let authorized = false;
                if (token === project.server_token) {
                    authorized = true; // Game client
                } else {
                    try {
                        const user = jwt.verify(token, JWT_SECRET);
                        if (user.id === project.owner_id) authorized = true; // Editor client
                    } catch (e) {
                        // Invalid JWT
                    }
                }

                if (authorized) {
                    socket.join(projectId);
                    socket.emit('joined', { projectId, message: `Successfully joined project ${projectId}` });
                    console.log(`Client ${socket.id} joined project ${projectId}`);
                } else {
                    socket.emit('error', { message: 'Unauthorized' });
                }
            } catch (err) {
                console.error(err);
            }
        });

        socket.on('editor_update', (data) => {
            const { projectId, update } = data;
            
            if (update.action === 'save_scene') {
                try {
                    const stmt = db.prepare('UPDATE projects SET scene_data = ? WHERE id = ?');
                    stmt.run(JSON.stringify(update.scene), projectId);
                    console.log(`Saved scene for project ${projectId}`);
                } catch (err) {
                    console.error('Failed to save scene:', err);
                }
            }

            socket.to(projectId).emit('scene_update', update);
        });

        socket.on('disconnect', () => {
            console.log(`Client disconnected: ${socket.id}`);
        });
    });
};
