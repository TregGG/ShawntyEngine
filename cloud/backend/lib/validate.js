const ASSET_TYPES = ['spritesheets', 'animations', 'objects', 'scripts', 'prefabs'];
const NAME = /^[A-Za-z0-9_][A-Za-z0-9_ .-]{0,99}$/;

// Names become file names, so anything that could escape a directory is rejected.
function isSafeName(name) {
  return typeof name === 'string' && NAME.test(name) && !name.includes('..');
}

// Values written into the engine's line-based asset formats.
function isSafeLine(value) {
  return typeof value === 'string' && value.length <= 200 && !/[\r\n]/.test(value);
}

// The editor has stored scenes both bare and (sometimes repeatedly) wrapped in {scene: ...}.
function unwrapScene(data) {
  let scene = data;
  while (scene && typeof scene === 'object' && !Array.isArray(scene.entities) && scene.scene) {
    scene = scene.scene;
  }
  return scene && typeof scene === 'object' && Array.isArray(scene.entities) ? scene : null;
}

module.exports = { ASSET_TYPES, isSafeName, isSafeLine, unwrapScene };
