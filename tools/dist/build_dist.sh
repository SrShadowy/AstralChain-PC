#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
IMAGE_NAME="astral-chain-pc-builder:ubuntu22.04"

cd "$ROOT_DIR"

echo "== Building Ubuntu 22.04 portable builder image =="
BUILD_CTX="$(mktemp -d)"
trap 'rm -rf "$BUILD_CTX"' EXIT
cp "$ROOT_DIR/requirements.txt" "$BUILD_CTX/requirements.txt"
docker build -t "$IMAGE_NAME" -f "$ROOT_DIR/tools/dist/Dockerfile.ubuntu" "$BUILD_CTX"

echo "== Running clean distribution build (.tar.gz + AppImage) =="
mkdir -p "$ROOT_DIR/dist"
docker run --rm \
    --user "$(id -u):$(id -g)" \
    -v "$ROOT_DIR:/workspace:ro" \
    -v "$ROOT_DIR/dist:/out:rw" \
    "$IMAGE_NAME" \
    /bin/bash /workspace/tools/dist/package.sh /workspace /out
