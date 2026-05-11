#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

echo "==> Pulling latest image..."
docker compose pull

echo "==> Restarting container..."
docker compose up -d

echo "==> Done. Container status:"
docker compose ps
