// Account and token maintenance. Run with the same environment as the server:
//   node admin.js list-users
//   node admin.js reset-password EMAIL|--all
//   node admin.js rotate-token PROJECT_ID|--all
const bcrypt = require('bcryptjs');
const crypto = require('crypto');
const db = require('./db');

const [command, target] = process.argv.slice(2);

function pick(table, key) {
  const rows = target === '--all'
    ? db.prepare(`SELECT * FROM ${table}`).all()
    : db.prepare(`SELECT * FROM ${table} WHERE ${key} = ?`).all(target ?? '');
  if (rows.length === 0) throw new Error(`Nothing matches "${target ?? ''}"`);
  return rows;
}

try {
  if (command === 'list-users') {
    for (const u of db.prepare('SELECT id, email, created_at FROM users').all()) console.log(u.id, u.email, u.created_at);
  } else if (command === 'reset-password') {
    for (const user of pick('users', 'email')) {
      const password = crypto.randomBytes(15).toString('base64url');
      // Bumping token_version signs the user out everywhere
      db.prepare('UPDATE users SET password_hash = ?, token_version = token_version + 1 WHERE id = ?')
        .run(bcrypt.hashSync(password, 12), user.id);
      console.log(`${user.email}\t${password}`);
    }
  } else if (command === 'rotate-token') {
    for (const project of pick('projects', 'id')) {
      db.prepare('UPDATE projects SET server_token = ? WHERE id = ?')
        .run('token_' + crypto.randomBytes(16).toString('hex'), project.id);
      console.log(`Rotated ${project.id}`);
    }
  } else {
    throw new Error('Usage: node admin.js list-users | reset-password EMAIL|--all | rotate-token PROJECT_ID|--all');
  }
} catch (err) {
  console.error(err.message);
  process.exitCode = 1;
} finally {
  db.close();
}
