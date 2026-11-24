const { DatabaseSync } = require('node:sqlite');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const { dbPath } = require('./config');

fs.mkdirSync(path.dirname(dbPath), { recursive: true });
const db = new DatabaseSync(dbPath);

db.exec(`
  PRAGMA journal_mode = WAL;
  PRAGMA busy_timeout = 5000;

  CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    email TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
  );

  CREATE TABLE IF NOT EXISTS projects (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    owner_id INTEGER NOT NULL,
    server_token TEXT NOT NULL,
    scene_data TEXT NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(owner_id) REFERENCES users(id)
  );

  CREATE TABLE IF NOT EXISTS assets_text (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    project_id TEXT NOT NULL,
    type TEXT NOT NULL,
    name TEXT NOT NULL,
    content TEXT NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(project_id) REFERENCES projects(id),
    UNIQUE(project_id, type, name)
  );

  CREATE INDEX IF NOT EXISTS idx_projects_server_token ON projects(server_token);
`);

if (!db.prepare('PRAGMA table_info(users)').all().some(c => c.name === 'token_version')) {
  db.exec('ALTER TABLE users ADD COLUMN token_version INTEGER NOT NULL DEFAULT 0');
}

if (!db.prepare('PRAGMA table_info(projects)').all().some(c => c.name === 'join_code')) {
  db.exec('ALTER TABLE projects ADD COLUMN join_code TEXT');
}

// Players type this to join. No 0/O or 1/I.
db.newJoinCode = () => {
  const alphabet = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';
  return Array.from(crypto.randomBytes(8), b => alphabet[b % 32]).join('');
};
for (const { id } of db.prepare('SELECT id FROM projects WHERE join_code IS NULL').all()) {
  db.prepare('UPDATE projects SET join_code = ? WHERE id = ?').run(db.newJoinCode(), id);
}

// SQLITE_CONSTRAINT_UNIQUE
db.isUniqueViolation = (err) => err && err.errcode === 2067;

module.exports = db;
