module.exports = function(io) {
    io.on('connection', (socket) => {
        console.log(`Client connected: ${socket.id}`);

        // A client (either Web Editor or Game Engine) joins a specific project room
        socket.on('join_project', (projectId, serverToken) => {
            console.log(`Client ${socket.id} joined project ${projectId}`);
            socket.join(projectId);
            socket.emit('joined', { projectId, message: `Successfully joined project ${projectId}` });
        });

        // The Web Editor sends an update (e.g., moved an object)
        socket.on('editor_update', (data) => {
            const { projectId, update } = data;
            console.log(`Received update for project ${projectId}:`, update);
            
            // Broadcast the update to all OTHER clients in the same project room
            // (Typically the Game Engine listening)
            socket.to(projectId).emit('scene_update', update);
        });

        socket.on('disconnect', () => {
            console.log(`Client disconnected: ${socket.id}`);
        });
    });
};
