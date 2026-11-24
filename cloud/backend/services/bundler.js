const AdmZip = require('adm-zip');
const fs = require('fs');
const path = require('path');
const db = require('../db');
const { uploadsDir } = require('../config');
const { ASSET_TYPES, isSafeName, unwrapScene } = require('../lib/validate');

const EXTENSIONS = {
  objects: '.objasset',
  spritesheets: '.ssheet',
  animations: '.anim',
  scripts: '.py',
  prefabs: '.prefab'
};

// Returns Map<relative path, Buffer>, or null if the project doesn't exist.
function collectProjectFiles(projectId) {
  const project = db.prepare('SELECT id, name, scene_data FROM projects WHERE id = ?').get(projectId);
  if (!project) return null;

  const files = new Map();
  const addJson = (name, value, indent) => files.set(name, Buffer.from(JSON.stringify(value, null, indent), 'utf8'));

  // server_token is deliberately left out: scripts and players can read these files.
  addJson('manifest.json', { id: project.id, name: project.name, initialScene: 'scenes/main.scene' }, 2);

  let scene = null;
  try {
    scene = unwrapScene(JSON.parse(project.scene_data));
  } catch { /* handled below */ }
  addJson('scenes/main.scene', { version: 1, scene: scene || { entities: [], relationships: [] } }, 4);

  const assets = db.prepare('SELECT type, name, content FROM assets_text WHERE project_id = ?').all(projectId);
  for (const { type, name, content } of assets) {
    if (!ASSET_TYPES.includes(type) || !isSafeName(name)) {
      console.warn(`Skipping asset with unsafe type or name in project ${projectId}`);
      continue;
    }
    const ext = EXTENSIONS[type];
    files.set(`${type}/${name.endsWith(ext) ? name : name + ext}`, Buffer.from(content, 'utf8'));
  }

  const texturesDir = path.join(uploadsDir, projectId, 'textures');
  if (fs.existsSync(texturesDir)) {
    for (const entry of fs.readdirSync(texturesDir, { withFileTypes: true })) {
      if (entry.isFile() && isSafeName(entry.name) && /\.(png|tga)$/i.test(entry.name)) {
        files.set(`textures/${entry.name}`, fs.readFileSync(path.join(texturesDir, entry.name)));
      }
    }
  }
  return files;
}

function buildProjectBundle(projectId) {
  const files = collectProjectFiles(projectId);
  if (!files) return null;
  const zip = new AdmZip();
  for (const [name, data] of files) zip.addFile(name, data);
  return zip.toBuffer();
}

// Updates in place: a running sandbox has this directory bind-mounted.
function syncProjectToDisk(projectId, targetDir) {
  const files = collectProjectFiles(projectId);
  if (!files) return false;

  for (const dir of ['scenes', 'textures', ...ASSET_TYPES]) {
    const full = path.join(targetDir, dir);
    fs.mkdirSync(full, { recursive: true });
    for (const name of fs.readdirSync(full)) {
      if (!files.has(`${dir}/${name}`)) fs.rmSync(path.join(full, name), { recursive: true, force: true });
    }
  }
  for (const [name, data] of files) {
    const target = path.join(targetDir, name);
    const temp = `${target}.tmp`;
    fs.writeFileSync(temp, data);
    fs.renameSync(temp, target);
  }
  return true;
}

module.exports = { buildProjectBundle, syncProjectToDisk };
