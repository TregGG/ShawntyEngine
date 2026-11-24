const fs = require('fs');
const path = require('path');
const db = require('../db');
const { uploadsDir } = require('../config');

const DIR = path.join(__dirname, '..', 'templates', 'starter');

const starterScene = () => fs.readFileSync(path.join(DIR, 'scene.json'), 'utf8');

// Gives a new project something to play: a floor, a player and a scripted platform.
function addStarterAssets(projectId) {
  const insert = db.prepare('INSERT INTO assets_text (project_id, type, name, content) VALUES (?, ?, ?, ?)');
  for (const file of fs.readdirSync(path.join(DIR, 'assets'))) {
    const [type, name] = file.split('__');
    insert.run(projectId, type, name, fs.readFileSync(path.join(DIR, 'assets', file), 'utf8'));
  }
  fs.cpSync(path.join(DIR, 'textures'), path.join(uploadsDir, projectId, 'textures'), { recursive: true });
}

module.exports = { starterScene, addStarterAssets };
