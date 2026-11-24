const { test, before, after } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawn, spawnSync } = require('child_process');

const dataDir = fs.mkdtempSync(path.join(os.tmpdir(), 'shawnty-test-'));
const base = 20000 + (process.pid % 20000);
Object.assign(process.env, {
  DATA_DIR: dataDir, JWT_SECRET: 'test-secret-test-secret-test-secret!', NODE_ENV: 'test',
  GAME_SERVERS_ENABLED: 'true', GAME_PORT_BASE: String(base), RELOAD_PORT_BASE: String(base + 100)
});

const config = require('../config');
const { server, io } = require('../server');
const db = require('../db');
const serverManager = require('../services/serverManager');
const AdmZip = require('adm-zip');
const { io: connect } = require('socket.io-client');

const canHost = fs.existsSync(config.engineBin) &&
  spawnSync('bwrap', ['--ro-bind', '/', '/', '--unshare-pid', 'true']).status === 0;
const hasMagick = !spawnSync('magick', ['-version']).error;

let url, alice, bob, project;
const sockets = [];

async function api(method, route, { token, body } = {}) {
  const isForm = body instanceof FormData;
  const res = await fetch(url + '/api' + route, {
    method,
    headers: { ...(token && { Authorization: `Bearer ${token}` }), ...(body && !isForm && { 'Content-Type': 'application/json' }) },
    body: isForm ? body : body && JSON.stringify(body)
  });
  const buf = Buffer.from(await res.arrayBuffer());
  let json = null;
  try { json = JSON.parse(buf); } catch { /* binary */ }
  return { status: res.status, json, buf };
}

async function account(email) {
  const creds = { email, password: 'correct horse battery' };
  assert.equal((await api('POST', '/auth/register', { body: creds })).status, 200);
  return (await api('POST', '/auth/login', { body: creds })).json.token;
}

function join(token) {
  const socket = connect(url, { forceNew: true });
  sockets.push(socket);
  return new Promise(resolve => {
    socket.on('joined', info => resolve({ socket, info }));
    socket.on('error', error => resolve({ socket, error }));
    socket.on('connect', () => socket.emit('join_project', project.id, token));
  });
}
const update = (socket, u) => socket.timeout(3000).emitWithAck('editor_update', { projectId: project.id, update: u });
const scene = name => ({ entities: [{ editorId: 'e', name, components: { transform: { position: [0, 0], size: [1, 1], rotation: 0 } } }], relationships: [] });
const savedScene = async () => (await api('GET', `/projects/${project.id}/scene`, { token: alice })).json.scene;

// Captures stdout lines from the sandboxed game server
let gameLog = '';
serverManager.events.on('log', (id, e) => { gameLog += `${e.level}: ${e.message}\n`; });
const realWrite = process.stdout.write.bind(process.stdout);
process.stdout.write = (chunk, ...rest) => {
  if (String(chunk).startsWith('[game:')) { gameLog += chunk; return true; }
  return realWrite(chunk, ...rest);
};
async function waitForLog(text) {
  for (let i = 0; i < 100; i++) {
    if (gameLog.includes(text)) return;
    await new Promise(r => setTimeout(r, 100));
  }
  assert.fail(`"${text}" not logged:\n${gameLog}`);
}

before(async () => {
  await new Promise(r => server.listen(0, '127.0.0.1', r));
  url = `http://127.0.0.1:${server.address().port}`;
  alice = await account('alice@example.com');
  bob = await account('bob@example.com');
  project = (await api('POST', '/projects', { token: alice, body: { name: 'Demo' } })).json;
});

after(async () => {
  sockets.forEach(s => s.close());
  serverManager.stopAll();
  await new Promise(r => setTimeout(r, 300));
  await io.close();
  db.close();
  fs.rmSync(dataDir, { recursive: true, force: true });
});

test('registration rejects weak and malformed input', async () => {
  for (const body of [{}, { email: 'a@b.co', password: 'short' }, { email: 'nope', password: 'long enough password' }, { email: {}, password: [] }]) {
    assert.equal((await api('POST', '/auth/register', { body })).status, 400);
  }
});

test('projects are private to their owner', async () => {
  assert.equal((await api('GET', '/projects')).status, 401);
  assert.equal((await api('GET', `/projects/${project.id}/scene`, { token: bob })).status, 404);
  assert.equal((await api('GET', `/projects/${project.id}/objects`, { token: bob })).status, 404);
});

test('asset names, types and contents are validated; duplicates conflict', async () => {
  const post = (type, body) => api('POST', `/projects/${project.id}/${type}`, { token: alice, body });
  assert.equal((await post('scripts', { name: '../../evil' })).status, 400);
  assert.equal((await post('weird', { name: 'x' })).status, 404);
  assert.equal((await post('spritesheets', { name: 'big', cols: 1e9, rows: 1e9 })).status, 400);
  assert.equal((await post('objects', { name: 'x', spritesheet: 'a\nb:c' })).status, 400);
  assert.equal((await post('spritesheets', { name: 'sheet' })).status, 200);
  assert.equal((await post('spritesheets', { name: 'sheet' })).status, 409);
  const put = await api('PUT', `/projects/${project.id}/spritesheets/sheet`, { token: alice, body: { cols: 2, rows: 1 } });
  assert.equal(put.status, 200);
});

test('upload file names never reach a shell', { skip: !hasMagick }, async () => {
  const src = path.join(dataDir, 'src.png');
  spawnSync('magick', ['-size', '8x8', 'xc:red', src]);
  const marker = path.join(dataDir, 'pwned');
  const form = new FormData();
  form.append('file', new Blob([fs.readFileSync(src)]), '$(touch pwned)`id`.png');
  const up = await api('POST', `/projects/${project.id}/assets/upload`, { token: alice, body: form });
  assert.equal(up.status, 200);
  assert.ok(!fs.existsSync(marker) && !fs.existsSync('pwned'));
  assert.match(up.json.name, /^[A-Za-z0-9_-]+$/);
  const image = await api('GET', `/projects/${project.id}/textures/${up.json.name}/image`, { token: alice });
  assert.equal(image.status, 200);
  const tga = fs.readFileSync(path.join(dataDir, 'uploads', project.id, 'textures', up.json.filename));
  assert.equal(tga[2], 2); // uncompressed
  assert.equal(tga[16], 32);

  const bad = new FormData();
  bad.append('file', new Blob(['push graphic-context']), 'x.mvg');
  assert.equal((await api('POST', `/projects/${project.id}/assets/upload`, { token: alice, body: bad })).status, 400);

  const frame = new FormData();
  frame.append('file', new Blob([fs.readFileSync(src)]), 'f.png');
  const append = await api('POST', `/projects/${project.id}/spritesheets/sheet/append`, { token: alice, body: frame });
  assert.deepEqual([append.status, append.json.w, append.json.frameIndex], [200, 8, 2]);
});

test('only the owner can save, and scenes are not re-wrapped', async () => {
  const stranger = connect(url, { forceNew: true });
  sockets.push(stranger);
  await new Promise(r => stranger.on('connect', r));
  assert.equal((await update(stranger, { action: 'save_scene', scene: scene('hacked') })).ok, false);

  assert.ok((await join(bob)).error);
  const game = await join(project.serverToken);
  assert.ok(game.info);
  assert.equal((await update(game.socket, { action: 'save_scene', scene: scene('hacked') })).ok, false);

  const owner = await join(alice);
  assert.equal((await update(owner.socket, { action: 'save_scene', scene: { scene: { scene: scene('mine') } } })).ok, true);
  assert.equal((await savedScene()).entities[0].name, 'mine');
  assert.equal((await update(owner.socket, { action: 'save_scene', scene: { nope: 1 } })).ok, false);
});

test('bundle contains the project but not its token', async () => {
  const info = await api('GET', `/projects/by-token/${project.serverToken}/info`);
  assert.equal(info.json.server_token, undefined);
  const { buf } = await api('GET', `/projects/by-token/${project.serverToken}/bundle`);
  const zip = new AdmZip(buf);
  assert.ok(zip.getEntry('scenes/main.scene'));
  assert.ok(zip.getEntry('spritesheets/sheet.ssheet'));
  for (const e of zip.getEntries()) assert.ok(!e.getData().includes(project.serverToken), e.entryName);
  assert.equal((await api('GET', '/projects/by-token/wrong/bundle')).status, 404);
});

test('game server runs the project sandboxed, hot-reloads and stops', { skip: !canHost }, async () => {
  serverManager.stopServer(project.id);
  await new Promise(r => setTimeout(r, 500));
  gameLog = '';

  const secret = path.join(dataDir, 'database.sqlite');
  db.prepare('INSERT INTO assets_text (project_id, type, name, content) VALUES (?,?,?,?)').run(project.id, 'scripts', 'probe.py',
    `import os\nclass Probe:\n    def OnStart(self, entity, input):\n        print("SEES_DB", os.path.exists(${JSON.stringify(secret)}), "HOME", os.path.exists(${JSON.stringify(os.homedir())}), "ENV", "JWT_SECRET" in os.environ, flush=True)\n        try:\n            open("/project/x", "w"); print("WROTE", flush=True)\n        except OSError:\n            print("READONLY", flush=True)\n    def OnUpdate(self, entity, dt, input):\n        pass\n`);
  const s = scene('probe');
  s.entities[0].components.script = { path: 'test_compiled/scripts/probe.py', class: 'Probe' };
  db.prepare('UPDATE projects SET scene_data = ? WHERE id = ?').run(JSON.stringify(s), project.id);

  const owner = await join(alice);
  assert.equal(owner.info.server.port, base);
  await waitForLog('SEES_DB False HOME False ENV False');
  await waitForLog('READONLY');
  await waitForLog('info: Game server ready');

  s.entities.push({ ...scene('second').entities[0], editorId: 'e2' });
  assert.equal((await update(owner.socket, { action: 'save_scene', scene: s })).ok, true);
  await waitForLog('info: Change applied');

  serverManager.stopServer(project.id);
  for (let i = 0; i < 50 && serverManager.getServer(project.id); i++) await new Promise(r => setTimeout(r, 100));
  assert.equal(serverManager.getServer(project.id), null);
});

test('a new project is playable and scripts are visible to players', { skip: !canHost }, async () => {
  const fresh = (await api('POST', '/projects', { token: alice, body: { name: 'Fresh' } })).json;
  const socket = connect(url, { forceNew: true });
  sockets.push(socket);
  const info = await new Promise(resolve => {
    socket.on('joined', resolve);
    socket.on('connect', () => socket.emit('join_project', fresh.id, alice));
  });
  await new Promise(r => setTimeout(r, 1500));
  const address = info.address.replace(/^[^:]+/, '127.0.0.1');
  const run = seconds => new Promise(resolve => {
    const child = spawn(config.engineBin, ['--probe', address, '--seconds', String(seconds)], { env: { ...process.env, XDG_CACHE_HOME: path.join(dataDir, 'c' + Math.random()) } });
    let out = '';
    child.stdout.on('data', d => { out += d; });
    child.on('exit', () => resolve(out));
  });
  const first = run(9);
  await new Promise(r => setTimeout(r, 2000));
  const [a, b] = await Promise.all([first, run(4)]);

  // The platform (second entity in the scene) moves back and forth for both players
  for (const out of [a, b]) {
    const xs = [...out.matchAll(/entity: id=2 x=(-?[\d.]+)/g)].map(m => Number(m[1]));
    assert.ok(xs.length > 2 && Math.max(...xs) - Math.min(...xs) > 1, out.slice(-1500));
    assert.ok(xs.every(x => Math.abs(x) < 4.5), `platform out of range: ${xs}`);
    assert.ok(!/entity: id=1 /.test(out), 'the floor never moves, so it is never sent');
  }
  // The first player is told about the second, joining and leaving. The second already knows the first.
  assert.match(a, /spawn: id=\d+ kind=player/);
  assert.match(b, /spawn: id=\d+ kind=player/);
  assert.match(a, /destroy: id=\d+/);
  serverManager.stopServer(fresh.id);
});

test('players download the project, see changes live, and never get scripts', { skip: !canHost }, async () => {
  const cache = path.join(dataDir, 'cache');
  const setScript = src => db.prepare("UPDATE assets_text SET content = ? WHERE project_id = ? AND name = 'probe.py'").run(src, project.id);
  const good = 'class Probe:\n    def OnUpdate(self, entity, dt, input):\n        pass\n';
  setScript(good);
  const s = scene('v1');
  s.entities[0].components.script = { path: 'scripts/probe.py', class: 'Probe' };
  s.entities.push({ editorId: 'floor', name: 'Floor', components: {
    transform: { position: [0, -3], size: [500, 1], rotation: 0 }, collider: { autoBounds: true },
    rigidbody: { bodyType: 'Static' } } });
  db.prepare('UPDATE projects SET scene_data = ? WHERE id = ?').run(JSON.stringify(s), project.id);
  // Larger than one packet, one send window and one chunk; plus a list too long for one packet
  const big = require('crypto').randomBytes(3_000_000);
  fs.mkdirSync(path.join(dataDir, 'uploads', project.id, 'textures'), { recursive: true });
  fs.writeFileSync(path.join(dataDir, 'uploads', project.id, 'textures', 'big.tga'), big);
  for (let i = 0; i < 150; i++) {
    db.prepare('INSERT INTO assets_text (project_id, type, name, content) VALUES (?,?,?,?)').run(project.id, 'objects', `object_${i}`, `spritesheet:s${i}\nanimations:s${i}\n`);
  }
  gameLog = '';

  const owner = await join(alice);
  const received = [];
  owner.socket.on('game_log', e => received.push(e));
  const game = await join(project.serverToken);
  assert.equal(game.info.address, undefined); // the code is for the owner
  await waitForLog('info: Game server ready');
  const address = owner.info.address.replace(/^[^:]+/, '127.0.0.1');
  assert.match(address, /^127\.0\.0\.1:\d+\/[A-Z2-9]{8}$/);

  const probe = (addr, seconds, dir) => {
    const child = spawn(config.engineBin, ['--probe', addr, '--seconds', String(seconds)], { env: { ...process.env, XDG_CACHE_HOME: dir } });
    const p = { out: '', done: new Promise(r => child.on('exit', r)) };
    child.stdout.on('data', d => { p.out += String(d).split('\n').filter(l => !l.includes('][')).join('\n'); });
    p.until = async (text, n = 1) => {
      for (let i = 0; i < 80; i++) {
        if (p.out.split(text).length > n) return;
        await new Promise(r => setTimeout(r, 100));
      }
      assert.fail(`"${text}" x${n} not in:\n${p.out}\n--- game\n${gameLog.slice(-1500)}`);
    };
    return p;
  };

  const wrong = probe(address.replace(/\/.*/, '/WRONG222'), 4, cache);
  await wrong.done;
  assert.match(wrong.out, /Wrong join code/);
  assert.doesNotMatch(wrong.out, /content:/);

  const player = probe(address, 20, cache);
  await player.until('welcome: scene=scenes/main.scene');
  const root = /root=(\S+)/.exec(player.out)[1];
  assert.ok(fs.existsSync(path.join(root, 'scenes/main.scene')));
  assert.ok(fs.readFileSync(path.join(root, 'textures/big.tga')).equals(big));
  assert.ok(!fs.existsSync(path.join(root, 'scripts')) && !fs.existsSync(path.join(root, 'server.json')) && !fs.existsSync(path.join(root, 'manifest.json')));
  await player.until('position: x=', 3); // moving right

  // A saved change reaches the connected player, who keeps playing
  s.entities[0].name = 'v2';
  assert.equal((await update(owner.socket, { action: 'save_scene', scene: s })).ok, true);
  await player.until('content:', 2);
  await player.until('welcome:', 2);
  const second = player.out.split('content:')[2];
  assert.match(second, /downloaded=1 /); // only the changed file
  assert.match(fs.readFileSync(path.join(/root=(\S+)/.exec(second)[1], 'scenes/main.scene'), 'utf8'), /v2/);
  const xs = [...player.out.matchAll(/x=(-?[\d.]+)/g)].map(m => Number(m[1]));
  for (let i = 1; i < xs.length; i++) assert.ok(xs[i] > xs[i - 1] - 1, `player was moved back: ${xs}`);

  // Saves in quick succession: the last one always arrives
  for (let i = 0; i < 10; i++) {
    s.entities[0].name = `burst${i}`;
    update(owner.socket, { action: 'save_scene', scene: s });
  }
  const latest = () => {
    const roots = [...player.out.matchAll(/root=(\S+)/g)];
    try { return fs.readFileSync(path.join(roots.at(-1)[1], 'scenes/main.scene'), 'utf8'); } catch { return ''; }
  };
  for (let i = 0; i < 50 && !latest().includes('burst9'); i++) await new Promise(r => setTimeout(r, 100));
  assert.match(latest(), /burst9/);
  const versions = player.out.split('content:').length;

  // A broken script is reported and the running version stays
  await api('PUT', `/projects/${project.id}/scripts/probe.py`, { token: alice, body: { content: 'class Probe(:\n' } });
  await waitForLog('error: Change not applied');
  await new Promise(r => setTimeout(r, 300)); // reaches the editor's socket
  assert.ok(received.some(e => e.level === 'error' && /probe\.py/.test(e.message) && /SyntaxError/.test(e.message)), JSON.stringify(received));
  await new Promise(r => setTimeout(r, 500));
  assert.equal(player.out.split('content:').length, versions);

  // Runtime errors are reported once, not every frame
  await api('PUT', `/projects/${project.id}/scripts/probe.py`, { token: alice, body: { content: good.replace('pass', 'boom()') } });
  await waitForLog("NameError: name 'boom' is not defined");
  await new Promise(r => setTimeout(r, 500));
  const nameErrors = received.filter(e => /NameError/.test(e.message));
  assert.equal(nameErrors.length, 1, JSON.stringify(nameErrors));

  const rotated = await api('POST', `/projects/${project.id}/join-code`, { token: alice });
  await new Promise(r => setTimeout(r, 500));
  const stale = probe(address, 4, cache);
  await stale.done;
  assert.match(stale.out, /Wrong join code/);
  const fresh = probe(address.replace(/\/.*/, '/' + rotated.json.joinCode), 3, cache);
  await fresh.until('welcome:');
  assert.match(fresh.out, /downloaded=0 /); // everything cached
  await player.done;
  assert.ok(!fs.existsSync(root)); // temporary copies are cleaned up
});

test('resetting a password revokes sessions; rotating a token revokes the old one', async () => {
  const admin = (...args) => spawnSync(process.execPath, [path.join(__dirname, '..', 'admin.js'), ...args], { env: process.env, encoding: 'utf8' });
  const reset = admin('reset-password', 'bob@example.com');
  assert.equal(reset.status, 0, reset.stderr);
  assert.equal((await api('GET', '/projects', { token: bob })).status, 403);
  const password = reset.stdout.trim().split('\t')[1];
  const login = await api('POST', '/auth/login', { body: { email: 'bob@example.com', password } });
  assert.equal((await api('GET', '/projects', { token: login.json.token })).status, 200);

  assert.equal(admin('rotate-token', project.id).status, 0);
  assert.equal((await api('GET', `/projects/by-token/${project.serverToken}/info`)).status, 404);
  assert.notEqual(admin('reset-password', 'nobody@example.com').status, 0);
});

test('production refuses to start without a strong JWT_SECRET', () => {
  const run = spawnSync(process.execPath, ['-e', 'require("./config")'], {
    cwd: path.join(__dirname, '..'), env: { PATH: process.env.PATH, NODE_ENV: 'production', JWT_SECRET: 'short' }
  });
  assert.notEqual(run.status, 0);
  assert.match(String(run.stderr), /JWT_SECRET/);
});
