const path = require('path');
const crypto = require('crypto');
require('dotenv').config({ path: path.join(__dirname, '.env'), quiet: true });

const isProduction = process.env.NODE_ENV === 'production';

function int(name, fallback) {
  const raw = process.env[name];
  if (raw === undefined || raw === '') return fallback;
  const value = Number(raw);
  if (!Number.isInteger(value) || value < 0) throw new Error(`${name} must be a non-negative integer`);
  return value;
}

let jwtSecret = process.env.JWT_SECRET;
if (isProduction && (!jwtSecret || jwtSecret.length < 32)) {
  throw new Error('JWT_SECRET must be set to at least 32 characters in production');
}
if (!jwtSecret) {
  jwtSecret = crypto.randomBytes(32).toString('hex');
  console.warn('JWT_SECRET is not set: using a random one, so logins reset on restart.');
}

const dataDir = process.env.DATA_DIR ? path.resolve(process.env.DATA_DIR) : __dirname;
const frameworkRoot = path.resolve(__dirname, '..', '..');

module.exports = {
  isProduction,
  jwtSecret,
  port: int('PORT', 3001),
  host: process.env.HOST || (isProduction ? '127.0.0.1' : 'localhost'),
  trustProxy: int('TRUST_PROXY', 0),
  // Extra origins allowed to call the API. Same-origin needs no entry.
  corsOrigins: (process.env.CORS_ORIGINS || (isProduction ? '' : 'http://localhost:5173'))
    .split(',').map(s => s.trim()).filter(Boolean),
  dbPath: path.join(dataDir, 'database.sqlite'),
  uploadsDir: path.join(dataDir, 'uploads'),
  runtimeDir: path.join(dataDir, 'runtime'),
  frontendDir: path.join(__dirname, '..', 'frontend', 'dist'),
  frameworkRoot,
  engineBin: path.resolve(process.env.ENGINE_BIN || path.join(frameworkRoot, 'bin', 'framework')),
  // Project scripts are user-supplied Python, so hosting is opt-in and sandboxed.
  gameServersEnabled: process.env.GAME_SERVERS_ENABLED === 'true',
  // 'systemd': through the root helper in deploy/ with per-game limits. 'bwrap': direct, for development.
  gameLauncher: process.env.GAME_LAUNCHER || 'bwrap',
  gamePortBase: int('GAME_PORT_BASE', 7777),
  reloadPortBase: int('RELOAD_PORT_BASE', 9091),
  maxGameServers: int('MAX_GAME_SERVERS', 10),
  publicGameHost: process.env.PUBLIC_GAME_HOST || 'localhost',
};
