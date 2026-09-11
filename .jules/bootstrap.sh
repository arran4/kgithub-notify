#!/bin/bash
set -euo pipefail

ROOTFS_CACHE_DIR="${HOME}/.cache/kde-dev-rootfs"
MARKER_FILE="${ROOTFS_CACHE_DIR}/.ready"
TAR_FILE="kde-dev-rootfs-forky-amd64.tar.zst"
SHA_FILE="${TAR_FILE}.sha256"
DOWNLOAD_URL="https://github.com/arran4/kde-dev-rootfs/releases/latest/download/${TAR_FILE}"
SHA_URL="https://github.com/arran4/kde-dev-rootfs/releases/latest/download/${SHA_FILE}"

if [ -f "$MARKER_FILE" ]; then
    echo "Rootfs is already bootstrapped at $ROOTFS_CACHE_DIR."
    exit 0
fi

echo "Bootstrapping rootfs..."
mkdir -p "$ROOTFS_CACHE_DIR"
cd "$ROOTFS_CACHE_DIR"

echo "Downloading ${TAR_FILE}..."
curl --fail --location --retry 3 -o "$TAR_FILE" "$DOWNLOAD_URL"

echo "Downloading ${SHA_FILE}..."
curl --fail --location --retry 3 -o "$SHA_FILE" "$SHA_URL"

echo "Verifying checksum..."
sha256sum -c "$SHA_FILE"

echo "Extracting rootfs..."
# Use sudo to preserve numeric ownership for a proper chroot filesystem setup
# Exclude ./dev/* because unprivileged containers cannot mknod device files and run.sh mounts /dev anyway
sudo tar -I zstd --numeric-owner --exclude='./dev/*' -xf "$TAR_FILE"

# Make sure we clean up the downloaded archives
sudo rm "$TAR_FILE" "$SHA_FILE"

sudo chown $USER:$USER "$ROOTFS_CACHE_DIR"
touch "$MARKER_FILE"
echo "Rootfs successfully bootstrapped at $ROOTFS_CACHE_DIR."
