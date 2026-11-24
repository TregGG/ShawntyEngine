const config = require('./config');
const express = require('express');
const fs = require('fs');
const http = require('http');
const path = require('path');
const { Server } = require('socket.io');
const cors = require('cors');
const helmet = require('helmet');

const db = require('./db');
const serverManager = require('./services/serverManager');

const app = express();
const server = http.createServer(app);

app.set('trust proxy', config.trustProxy);
app.use(helmet({
  contentSecurityPolicy: {
    directives: {
      imgSrc: ["'self'", 'data:', 'blob:'],
      connectSrc: ["'self'", 'ws:', 'wss:'],
      upgradeInsecureRequests: null // TLS is handled by the reverse proxy
    }
  }
}));
app.use(cors({ origin: config.corsOrigins }));
app.use(express.json({ limit: '2mb' }));

app.get('/api/health', (req, res) => {
  try {
    db.prepare('SELECT 1').get();
    res.json({ status: 'ok' });
  } catch {
    res.status(503).json({ status: 'error' });
  }
});
app.use('/api/auth', require('./routes/auth'));
app.use('/api/projects', require('./routes/projects'));
app.use('/api/projects', require('./routes/assets'));
app.use('/api', (req, res) => res.status(404).json({ error: 'Not found' }));

// Serve the built frontend when present, with SPA fallback
if (fs.existsSync(config.frontendDir)) {
  app.use(express.static(config.frontendDir));
  app.get('/{*splat}', (req, res) => res.sendFile(path.join(config.frontendDir, 'index.html')));
}

app.use((err, req, res, next) => {
  if (res.headersSent) return next(err);
  const status = err.status || (err.name === 'MulterError' ? 400 : 500);
  if (status >= 500) console.error(err);
  res.status(status).json({ error: status >= 500 ? 'Server error' : err.message });
});

const io = new Server(server, {
  cors: { origin: config.corsOrigins, methods: ['GET', 'POST'] },
  maxHttpBufferSize: 2e6
});
require('./socket/editor')(io);

function shutdown() {
  serverManager.stopAll();
  io.close(() => {
    db.close();
    process.exit(0);
  });
  setTimeout(() => process.exit(1), 10000).unref();
}

if (require.main === module) {
  server.listen(config.port, config.host, () => {
    console.log(`Backend listening on ${config.host}:${config.port}`);
  });
  process.on('SIGTERM', shutdown);
  process.on('SIGINT', shutdown);
}

module.exports = { app, server, io };
