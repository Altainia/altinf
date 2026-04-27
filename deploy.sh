#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

echo "==> Pulling latest code..."
git pull

echo "==> Building Docker image..."
docker compose build

echo "==> Restarting container..."
docker compose up -d

echo "==> Done. Container status:"
docker compose ps
