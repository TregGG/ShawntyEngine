#!/bin/bash
# Uploads this checkout to the VM and installs or updates it.
#   ./deploy/deploy.sh azureuser@my-name.westeurope.cloudapp.azure.com
set -euo pipefail
TARGET=${1:?usage: deploy.sh USER@HOST}
cd "$(dirname "$0")/.."
[[ -f external/glm/glm/glm.hpp && -f external/enet/host.c ]] || git submodule update --init
ssh "$TARGET" 'command -v rsync >/dev/null || (sudo apt-get update -q && sudo apt-get install -qy rsync); mkdir -p shawnty-src'
rsync -az --delete --exclude .git --exclude node_modules --exclude dist --exclude venv --exclude /obj --exclude /bin \
    --exclude /release --exclude '*.log' --exclude logs.txt --exclude .env --exclude 'database.sqlite*' \
    --exclude /cloud/backend/uploads --exclude /cloud/backend/runtime ./ "$TARGET:shawnty-src/"
ssh -t "$TARGET" "sudo DOMAIN=${TARGET#*@} bash shawnty-src/deploy/setup-vm.sh"
