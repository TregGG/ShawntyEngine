const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const cors = require('cors');
require('dotenv').config();

const authRoutes = require('./routes/auth');
const projectRoutes = require('./routes/projects');
const assetRoutes = require('./routes/assets');
const setupEditorSocket = require('./socket/editor');

const app = express();
const server = http.createServer(app);

// Middleware
app.use(cors());
app.use(express.json());

// Routes
app.use('/api/auth', authRoutes);
app.use('/api/projects', projectRoutes);
app.use('/api/projects', assetRoutes);

// Setup Socket.io
const io = new Server(server, {
  cors: {
    origin: "*", // For development, allow all origins
    methods: ["GET", "POST"]
  }
});

setupEditorSocket(io);

const PORT = process.env.PORT || 3001;

server.listen(PORT, () => {
  console.log(`Backend server running on port ${PORT}`);
});
