const { spawn, spawnSync } = require('child_process');
const path = require('path');
const fs = require('fs');
const dgram = require('dgram');
const config = require('../config');
const { EventEmitter } = require('events');
const db = require('../db');
const { syncProjectToDisk } = require('./bundler');

// 'log' (projectId, { level, message }) and 'status' (projectId)
const events = new EventEmitter();
const EVENT_PREFIX = '@@shawnty ';
const MAX_LINES_PER_SECOND = 20;

// projectId -> { child, slot, port, reloadPort }
const active = new Map();
let available = null;
const useSystemd = config.gameLauncher === 'systemd';
const HELPER = ['-n', '/usr/local/sbin/shawnty-game'];

function isAvailable() {
  if (!config.gameServersEnabled) return false;
  if (available === null) {
    const hasEngine = fs.existsSync(config.engineBin);
    const probe = useSystemd
      ? spawnSync('sudo', ['-n', '-l', HELPER[1]], { stdio: 'ignore' })
      : spawnSync('bwrap', ['--ro-bind', '/', '/', '--unshare-pid', 'true'], { stdio: 'ignore' });
    const hasSandbox = !probe.error && probe.status === 0;
    if (!hasEngine) console.error(`Game servers disabled: engine not found at ${config.engineBin}`);
    if (!hasSandbox) console.error('Game servers disabled: the sandbox launcher is missing or not permitted');
    available = hasEngine && hasSandbox;
  }
  return available;
}

// Scripts are untrusted. They see the system libraries, the engine and their own
// project, all read-only, with no environment and bounded memory and file sizes.
// The network namespace is shared because the server has to accept players.
function sandboxCommand(projectDir, port, reloadPort) {
  const args = [
    '--as=2147483648', '--nofile=256', '--fsize=16777216', '--core=0', '--',
    'bwrap', '--die-with-parent', '--new-session', '--clearenv',
    '--unshare-user', '--unshare-pid', '--unshare-ipc', '--unshare-uts', '--unshare-cgroup-try',
    '--cap-drop', 'ALL',
    '--ro-bind', '/usr', '/usr'
  ];
  for (const dir of ['/bin', '/sbin', '/lib', '/lib64']) {
    if (!fs.existsSync(dir)) continue;
    if (fs.lstatSync(dir).isSymbolicLink()) args.push('--symlink', fs.readlinkSync(dir), dir);
    else args.push('--ro-bind', dir, dir);
  }
  args.push(
    '--ro-bind-try', '/etc/ld.so.cache', '/etc/ld.so.cache',
    '--proc', '/proc', '--dev', '/dev',
    '--size', '16777216', '--tmpfs', '/tmp',
    '--ro-bind', config.engineBin, '/engine/framework',
    '--ro-bind', projectDir, '/project',
    '--remount-ro', '/',
    '--chdir', '/tmp',
    '--setenv', 'PATH', '/usr/bin',
    '--setenv', 'HOME', '/tmp',
    '--setenv', 'PYTHONDONTWRITEBYTECODE', '1',
    '--setenv', 'PYTHONUNBUFFERED', '1',
    '/engine/framework', '--server', '--dev-events', '--project-dir', '/project',
    '--port', String(port), '--reload-port', String(reloadPort)
  );
  return ['prlimit', args];
}

function runtimeDirFor(projectId) {
  return path.join(config.runtimeDir, path.basename(projectId));
}

function freeSlot() {
  const used = new Set([...active.values()].map(s => s.slot));
  for (let i = 0; i < config.maxGameServers; i++) if (!used.has(i)) return i;
  return -1;
}

function getServer(projectId) {
  const entry = active.get(projectId);
  return entry ? { host: config.publicGameHost, port: entry.port } : null;
}

// What a player types. Contains the join code, so only for the project's owner.
function getJoinAddress(projectId) {
  const entry = active.get(projectId);
  if (!entry) return null;
  const { join_code: code } = db.prepare('SELECT join_code FROM projects WHERE id = ?').get(projectId);
  return `${config.publicGameHost}:${entry.port}/${code}`;
}

// Forwards script output and engine events to the project's editors. Output is written by
// untrusted scripts, so lines are capped in length and rate.
function watchOutput(projectId, stream, defaultLevel) {
  let pending = '', windowStart = 0, counts = {};
  // Separate budgets, so engine logs or a chatty script can't crowd out error reports
  const allow = (kind, limit) => (counts[kind] = (counts[kind] || 0) + 1) <= limit;
  stream.setEncoding('utf8');
  stream.on('data', chunk => {
    const lines = (pending + chunk).split('\n');
    pending = lines.pop().slice(-8192);
    for (const raw of lines) {
      // Engine log lines start with a colored [HH:MM:SS]; they go to the server log only
      // eslint-disable-next-line no-control-regex
      const line = raw.replace(/\x1b\[[0-9;]*m/g, '').replace(/[\x00-\x08\x0b-\x1f]/g, '').slice(0, 8192);
      if (!line.trim()) continue;
      const now = Date.now();
      if (now - windowStart > 1000) { windowStart = now; counts = {}; }
      if (/^\[\d\d:\d\d:\d\d\]\[/.test(line)) {
        if (allow('engine', 200)) process.stdout.write(`[game:${projectId}] ${line}\n`);
        continue;
      }
      if (!allow(line.startsWith(EVENT_PREFIX) ? 'event' : 'print', MAX_LINES_PER_SECOND)) continue;
      let event = { level: defaultLevel, message: line };
      if (line.startsWith(EVENT_PREFIX)) {
        try {
          const { level, message } = JSON.parse(line.slice(EVENT_PREFIX.length));
          event = { level: ['info', 'error'].includes(level) ? level : 'log', message: String(message) };
        } catch { continue; }
      }
      events.emit('log', projectId, event);
    }
  });
}

function startServer(projectId) {
  if (!isAvailable()) return null;
  if (active.has(projectId)) return getServer(projectId);

  const slot = freeSlot();
  if (slot < 0) {
    console.warn(`No free game server slot for project ${projectId}`);
    return null;
  }

  const projectDir = runtimeDirFor(projectId);
  if (!syncProjectToDisk(projectId, projectDir)) return null;

  const entry = { slot, port: config.gamePortBase + slot, reloadPort: config.reloadPortBase + slot };
  const [command, args] = useSystemd
    ? ['sudo', [...HELPER, 'start', String(slot), projectId]]
    : sandboxCommand(projectDir, entry.port, entry.reloadPort);
  const child = spawn(command, args, { cwd: '/', env: { PATH: process.env.PATH }, stdio: ['ignore', 'pipe', 'pipe'] });
  entry.child = child;
  active.set(projectId, entry);

  watchOutput(projectId, child.stdout, 'log');
  watchOutput(projectId, child.stderr, 'error');
  const forget = () => {
    if (active.get(projectId) !== entry) return;
    active.delete(projectId);
    events.emit('status', projectId);
  };
  child.on('error', err => { console.error(`Game server for ${projectId} failed to start:`, err.message); forget(); });
  child.on('exit', (code, signal) => { console.log(`Game server for ${projectId} exited (${signal || code})`); forget(); });

  events.emit('status', projectId);
  console.log(`Game server for ${projectId} started on port ${entry.port}`);
  return getServer(projectId);
}

function reloadServer(projectId) {
  const entry = active.get(projectId);
  if (!entry) return; // the next start syncs from the database anyway
  if (!syncProjectToDisk(projectId, runtimeDirFor(projectId))) return;

  const socket = dgram.createSocket('udp4');
  socket.on('error', err => { console.error('Reload packet failed:', err.message); socket.close(); });
  socket.send('reload', entry.reloadPort, '127.0.0.1', () => socket.close());
}

// The slot (and its ports) is released when the process has actually exited.
function stopServer(projectId) {
  const entry = active.get(projectId);
  if (!entry) return;
  // The systemd launcher runs as root, so it has to stop the unit for us
  if (useSystemd) spawn('sudo', [...HELPER, 'stop', String(entry.slot)], { stdio: 'ignore' }).on('error', () => {});
  else entry.child.kill('SIGKILL');
}

function stopAll() {
  for (const projectId of active.keys()) stopServer(projectId);
}

module.exports = { startServer, reloadServer, stopServer, stopAll, getServer, getJoinAddress, events };
