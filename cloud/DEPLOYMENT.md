# Deploying to one Azure VM

**First deployment: work through [deploy/PRE_DEPLOY.md](../deploy/PRE_DEPLOY.md).**

The editor, database and every game server run on one Debian 12 VM.

```bash
az login
./deploy/create-vm.sh my-unique-name westeurope Standard_B2s   # creates the VM: billing starts
./deploy/deploy.sh azureuser@my-unique-name.westeurope.cloudapp.azure.com
```

Run `deploy.sh` again to update; this stops running games. Without local Azure CLI, run
`create-vm.sh` in Cloud Shell with `SSH_FROM=<your IP>/32` and your own SSH key.

| | Limit |
|---|---|
| One game | 50% of one CPU, 384 MB, 32 threads, 12 hours (`GAME_CPU`, `GAME_MEMORY_MB`) |
| All games together | 75% of the CPUs, all memory except 1 GB |
| Number of games | As many as fit in that memory, at most 20 (`MAX_GAME_SERVERS`) |

To change these, run `setup-vm.sh` on the VM with the variables set. Each game is a systemd
unit (`systemctl status 'shawnty-game-*'`, `systemd-cgtop`) under its own user, and cannot see
the database or secrets. A firewall rule lets games answer players but not open connections.
Games stop after five minutes without players and restart when the project is next opened.

Players enter `host:port/CODE` in the game client, or run `framework --connect host:port/CODE`.
The editor toolbar shows the address. The code can be replaced with `POST /api/projects/ID/join-code`.
Game traffic, including the code, is not encrypted. Player login will replace codes later.

The server sends players the project's data files, never scripts. Saving in the editor updates
connected players; a change with a script error is rejected and shown in the editor console.
To check a server without a window: `framework --probe host:port/CODE --seconds 5`.

Logs: `journalctl -u shawnty`. Back up `/var/lib/shawnty`. Health check: `/api/health`.

## Accounts

```bash
sudo -u shawnty env $(sudo cat /etc/shawnty/backend.env /etc/shawnty/secret.env | xargs) \
    sh -c 'cd /opt/shawnty/cloud/backend && node admin.js list-users'
```

Other commands: `reset-password EMAIL|--all` (prints a new password and signs the user out) and
`rotate-token PROJECT_ID|--all`.

## Moving an existing database

`database.sqlite` is no longer tracked by git. **Pulling that commit deletes the file
from any other checkout that still tracks it.** On a live host, before pulling:

```bash
sudo mkdir -p /var/lib/shawnty        # owned by the service user
cp -p cloud/backend/database.sqlite /var/lib/shawnty/
cp -rp cloud/backend/uploads /var/lib/shawnty/
```

## Development game servers

`GAME_SERVERS_ENABLED=true` with the default `GAME_LAUNCHER=bwrap` needs `bwrap` and `prlimit`.
That mode does **not** limit CPU or block outgoing connections. Don't use it on a public host.

## Tests

```bash
cd cloud/backend && npm test
./bin/framework --server --test    # from the repository root
```
