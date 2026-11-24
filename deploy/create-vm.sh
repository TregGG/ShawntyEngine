#!/bin/bash
# Creates the single Azure VM. Needs the Azure CLI, logged in (az login). This starts billing.
#   ./deploy/create-vm.sh my-unique-dns-name [region] [size]
set -euo pipefail
NAME=${1:?usage: create-vm.sh DNS_NAME [REGION] [SIZE]}
REGION=${2:-westeurope}
SIZE=${3:-Standard_B2s}
GROUP=shawnty
[[ $NAME =~ ^[a-z][a-z0-9-]{1,61}[a-z0-9]$ ]] || { echo "DNS name: lowercase letters, digits and hyphens" >&2; exit 1; }

# SSH is limited to the address you run this from. Set SSH_FROM when using Cloud Shell.
SSH_FROM=${SSH_FROM:-$(curl -fsS4 https://ifconfig.me)/32}

az group create -n $GROUP -l "$REGION" -o none
az network nsg create -g $GROUP -n shawnty-nsg -o none
rule() { az network nsg rule create -g $GROUP --nsg-name shawnty-nsg -o none --access Allow --direction Inbound "$@"; }
rule -n ssh   --priority 100 --protocol Tcp --source-address-prefixes "$SSH_FROM" --destination-port-ranges 22
rule -n web   --priority 110 --protocol Tcp --destination-port-ranges 80 443
rule -n games --priority 120 --protocol Udp --destination-port-ranges 7777-7796

az vm create -g $GROUP -n shawnty --image Debian:debian-12:12-gen2:latest --size "$SIZE" \
    --admin-username azureuser --generate-ssh-keys --nsg shawnty-nsg \
    --public-ip-sku Standard --public-ip-address-allocation static --public-ip-address-dns-name "$NAME" \
    --os-disk-size-gb 64 -o none
HOST=$(az network public-ip show -g $GROUP -n shawntyPublicIP --query dnsSettings.fqdn -o tsv)
echo "VM created. Deploy with: ./deploy/deploy.sh azureuser@$HOST"
echo "Delete everything with:  az group delete -n $GROUP"
