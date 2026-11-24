# Before the first Azure deployment

Status on 2026-11-24: **not deployed**; no VM exists. The scripts here have never been run on a
real VM. How to deploy and the resource limits: [cloud/DEPLOYMENT.md](../cloud/DEPLOYMENT.md).

Decision: one VM with systemd, no Docker. Docker would need root-equivalent socket access for the
backend and new firewall rules for games. Reconsider for several machines, AKS/Container Apps,
or CI-built images; launching is confined to `serverManager.js` and `shawnty-game`.

## 1. Build first (no VM needed)

- [ ] Release folders: build into `/opt/shawnty/releases/<commit>`, then switch a symlink. Today
      `setup-vm.sh` stops the site and builds in place, so a failed build leaves it down. Include
      rollback, and update the paths in `games.conf`, `backend.env` and `shawnty.service`.
- [ ] Nightly backup of SQLite (use `.backup` or `VACUUM INTO`, not a plain copy: it uses WAL) and
      uploads to Azure Blob Storage. Test a restore.
- [ ] Audit network packet parsing (`services/networkcontrol.cpp`, `test/testnetworkcontrol.cpp`)
      before opening the game ports to the internet.

## 2. Clean up old credentials

- [ ] Save, then delete `~/shawnty-new-passwords.txt`.
- [ ] Delete old hashes: `~/shawnty-pre-rewrite.bundle`, `cloud/backend/database.sqlite.bak`, and
      the `refs/original-dbpurge` refs (then expire the reflog and run `git gc --prune=now`).
- [ ] If the GitHub repo is or was public, ask GitHub Support to purge the old commits.
- [ ] If any other copy of the database exists, run `node admin.js reset-password --all` and
      `rotate-token --all` on it.

## 3. Create and deploy

- [ ] Install the Azure CLI and run `az login`.
- [ ] `./deploy/create-vm.sh DNS_NAME [REGION] [SIZE]` (billing starts).
- [ ] `./deploy/deploy.sh azureuser@HOST`, watching the output.
- [ ] To keep existing accounts and projects: stop the service, copy `database.sqlite` and
      `uploads/` to `/var/lib/shawnty`, `chown -R shawnty:shawnty` them, start it again.

## 4. Check on the VM

- [ ] HTTPS and `/api/health` work; log in, edit, save and reopen a project in a browser.
- [ ] Image upload works (Debian has ImageMagick 6, development used 7).
- [ ] Join from the game client with `host:port`; two projects get different ports.
- [ ] CPU and memory limits hold (`systemd-cgtop`); a memory hog is killed.
- [ ] A game script cannot connect to localhost, the internet or `169.254.169.254`.
- [ ] After a reboot, the firewall rules, backend and Caddy all come back.
- [ ] Stopping or restarting the backend leaves no games running.

## Later

- Player login in the client, so joining doesn't need an address. Until then anyone with it can join.
- Email verification and self-service password reset (now `admin.js` only).
