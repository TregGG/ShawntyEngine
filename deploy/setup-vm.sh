#!/bin/bash
# Sets up (or updates) everything on one Debian 12 VM. Run as root from the uploaded source:
#   sudo DOMAIN=mygame.westeurope.cloudapp.azure.com ./deploy/setup-vm.sh
set -euo pipefail
[[ $EUID -eq 0 ]] || { echo "Run as root" >&2; exit 1; }
: "${DOMAIN:?Set DOMAIN to the public DNS name of this VM}"
[[ $DOMAIN =~ ^[A-Za-z0-9.-]+$ ]] || { echo "Bad DOMAIN" >&2; exit 1; }
SRC=$(cd "$(dirname "$0")/.." && pwd)
APP=/opt/shawnty
DATA=/var/lib/shawnty
NODE_MAJOR=24

# Limits for each game, and how many fit on this VM. Override through the environment.
GAME_CPU=${GAME_CPU:-50%}
GAME_MEMORY_MB=${GAME_MEMORY_MB:-384}
mem_mb=$(awk '/MemTotal/{print int($2/1024)}' /proc/meminfo)
cpus=$(nproc)
# Leave 1 GB of memory and a quarter of the CPU for the OS, editor and database
games_mem_mb=$(( mem_mb - 1024 ))
(( games_mem_mb >= GAME_MEMORY_MB )) || { echo "This VM is too small: needs at least $((1024 + GAME_MEMORY_MB)) MB" >&2; exit 1; }
games_cpu=$(( cpus * 75 ))
MAX_GAME_SERVERS=${MAX_GAME_SERVERS:-$(( games_mem_mb / GAME_MEMORY_MB ))}
(( MAX_GAME_SERVERS <= 20 )) || MAX_GAME_SERVERS=20   # matches the 20 ports opened in create-vm.sh

export DEBIAN_FRONTEND=noninteractive
apt-get update -q
apt-get install -qy build-essential pkg-config libglfw3-dev libgl1-mesa-dev libfreetype6-dev \
    python3-dev pybind11-dev python3-pybind11 imagemagick caddy nftables rsync curl xz-utils sudo openssl

case $(uname -m) in x86_64) arch=x64 ;; aarch64) arch=arm64 ;; *) echo "Unsupported CPU" >&2; exit 1 ;; esac
if ! node --version 2>/dev/null | grep -q "^v$NODE_MAJOR\."; then
    tmp=$(mktemp -d)
    url=https://nodejs.org/dist/latest-v$NODE_MAJOR.x
    curl -fsSL "$url/SHASUMS256.txt" -o "$tmp/sums"
    tarball=$(awk '{print $2}' "$tmp/sums" | grep "linux-$arch.tar.xz$")
    curl -fsSL "$url/$tarball" -o "$tmp/$tarball"
    (cd "$tmp" && grep " $tarball\$" sums | sha256sum -c -)
    tar -xJf "$tmp/$tarball" -C /usr/local --strip-components=1 --no-same-owner
    rm -rf "$tmp"
fi

id shawnty >/dev/null 2>&1 || useradd --system --home-dir $DATA --shell /usr/sbin/nologin shawnty
id shawnty-game >/dev/null 2>&1 || useradd --system --no-create-home --home-dir /nonexistent --shell /usr/sbin/nologin shawnty-game
install -d -o shawnty -g shawnty -m 0750 $DATA
install -d -m 0755 /etc/shawnty /var/empty /var/empty/shawnty

systemctl stop shawnty 2>/dev/null || true
install -d -m 0755 $APP
rsync -a --delete --chown=root:root --exclude .git --exclude node_modules --exclude /obj --exclude /bin --exclude /release \
    --exclude 'database.sqlite*' --exclude /cloud/backend/uploads --exclude /cloud/backend/runtime --exclude .env "$SRC/" $APP/
(cd $APP && make BUILD=release)
(cd $APP/cloud/frontend && npm ci && npm run build)
(cd $APP/cloud/backend && npm ci --omit=dev)
chmod -R go+rX $APP

# Settings. The secret is generated once and kept across updates.
if [[ ! -f /etc/shawnty/secret.env ]]; then
    (umask 077; echo "JWT_SECRET=$(openssl rand -hex 32)" > /etc/shawnty/secret.env)
fi
cat > /etc/shawnty/backend.env <<EOF
NODE_ENV=production
HOST=127.0.0.1
PORT=3001
TRUST_PROXY=1
DATA_DIR=$DATA
ENGINE_BIN=$APP/release/framework
GAME_SERVERS_ENABLED=true
GAME_LAUNCHER=systemd
PUBLIC_GAME_HOST=$DOMAIN
GAME_PORT_BASE=7777
RELOAD_PORT_BASE=9091
MAX_GAME_SERVERS=$MAX_GAME_SERVERS
EOF
cat > /etc/shawnty/games.conf <<EOF
DATA_DIR=$DATA
ENGINE_BIN=$APP/release/framework
GAME_PORT_BASE=7777
RELOAD_PORT_BASE=9091
MAX_GAME_SERVERS=$MAX_GAME_SERVERS
GAME_CPU=$GAME_CPU
GAME_MEMORY=${GAME_MEMORY_MB}M
EOF
install -o root -g root -m 0755 "$SRC/deploy/shawnty-game" /usr/local/sbin/shawnty-game
cat > /etc/sudoers.d/shawnty.new <<'EOF'
shawnty ALL=(root) NOPASSWD: /usr/local/sbin/shawnty-game
EOF
chmod 0440 /etc/sudoers.d/shawnty.new
visudo -cf /etc/sudoers.d/shawnty.new
mv /etc/sudoers.d/shawnty.new /etc/sudoers.d/shawnty

# Games may answer players but never open connections: not to localhost, the internet,
# or the Azure metadata service.
cat > /etc/nftables.conf <<EOF
#!/usr/sbin/nft -f
table inet shawnty
delete table inet shawnty
table inet shawnty {
    chain output {
        type filter hook output priority 0; policy accept;
        meta skuid $(id -u shawnty-game) ct state established,related accept
        meta skuid $(id -u shawnty-game) counter reject
    }
}
EOF
systemctl enable nftables
systemctl restart nftables

# Ceiling for all games together
cat > /etc/systemd/system/shawnty-games.slice <<EOF
[Unit]
Description=Shawnty game servers

[Slice]
CPUQuota=${games_cpu}%
CPUWeight=50
MemoryMax=${games_mem_mb}M
MemorySwapMax=0
TasksMax=1024
EOF
cat > /etc/systemd/system/shawnty.service <<EOF
[Unit]
Description=Shawnty cloud editor
After=network-online.target nftables.service
Wants=network-online.target

[Service]
User=shawnty
WorkingDirectory=$APP/cloud/backend
EnvironmentFile=/etc/shawnty/backend.env
EnvironmentFile=/etc/shawnty/secret.env
ExecStartPre=+-/usr/local/sbin/shawnty-game stop-all
ExecStart=/usr/local/bin/node server.js
ExecStopPost=+-/usr/local/sbin/shawnty-game stop-all
Restart=always
RestartSec=3
UMask=0022
CPUWeight=200
MemoryMax=900M
PrivateTmp=yes
ProtectHome=yes
ProtectSystem=full
# NoNewPrivileges must stay off: the backend starts games through sudo

[Install]
WantedBy=multi-user.target
EOF
cat > /etc/caddy/Caddyfile <<EOF
$DOMAIN {
    encode gzip
    reverse_proxy 127.0.0.1:3001
}
EOF
systemctl daemon-reload
systemctl enable shawnty caddy
systemctl restart shawnty caddy

for _ in $(seq 20); do curl -fsS http://127.0.0.1:3001/api/health && break || sleep 1; done
echo
curl -fsS http://127.0.0.1:3001/api/health >/dev/null
echo "Ready: https://$DOMAIN - up to $MAX_GAME_SERVERS games, each $GAME_CPU of a CPU and ${GAME_MEMORY_MB} MB; ${games_cpu}% CPU and ${games_mem_mb} MB in total"
