#!/bin/bash
set -euo pipefail

if [ "$#" -eq 0 ]; then
    echo "Usage: $0 <command> [args...]"
    exit 1
fi

ROOTFS_DIR="${HOME}/.cache/kde-dev-rootfs"

if [ ! -f "${ROOTFS_DIR}/.ready" ]; then
    echo "Error: Rootfs not found or not ready. Please run .jules/bootstrap.sh first."
    exit 1
fi

WORKSPACE_DIR=$(pwd)

# Function to unmount and clean up
cleanup() {
    echo "Cleaning up mounts..."
    sudo umount "${ROOTFS_DIR}/workspace" || true
    sudo umount "${ROOTFS_DIR}/etc/resolv.conf" || true
    sudo umount "${ROOTFS_DIR}/dev/pts" || true
    sudo umount -R "${ROOTFS_DIR}/dev" || true
    sudo umount "${ROOTFS_DIR}/sys" || true
    sudo umount "${ROOTFS_DIR}/proc" || true
}

trap cleanup EXIT

# Ensure mount points exist in rootfs
sudo mkdir -p "${ROOTFS_DIR}/workspace"
sudo mkdir -p "${ROOTFS_DIR}/proc"
sudo mkdir -p "${ROOTFS_DIR}/sys"
sudo mkdir -p "${ROOTFS_DIR}/dev/pts"
sudo rm -f "${ROOTFS_DIR}/etc/resolv.conf"
sudo touch "${ROOTFS_DIR}/etc/resolv.conf"

# Test if mounting is permitted in this environment; if not, fall back to proot
if ! sudo mount -t proc /proc "${ROOTFS_DIR}/proc" 2>/dev/null; then
    echo "Direct mount not permitted in this environment, using proot wrapper..."
    exec proot -r "${ROOTFS_DIR}" -b /proc -b /dev -b /sys -b "${HOME}" -b "${WORKSPACE_DIR}:/workspace" /bin/bash -c 'cd /workspace; export QT_QPA_PLATFORM=offscreen; "$@"' -- "$@"
fi

echo "Mounting file systems..."
sudo mount -t sysfs /sys "${ROOTFS_DIR}/sys"
sudo mount --rbind /dev "${ROOTFS_DIR}/dev"
sudo mount --make-rslave "${ROOTFS_DIR}/dev"
sudo mount -t devpts /dev/pts "${ROOTFS_DIR}/dev/pts"

# Copy DNS configuration (using bind mount)
sudo mount --bind /etc/resolv.conf "${ROOTFS_DIR}/etc/resolv.conf"

# Mount the workspace
sudo mount --bind "${WORKSPACE_DIR}" "${ROOTFS_DIR}/workspace"

# Execute the command inside the chroot
echo "Executing command in chroot..."
sudo chroot "${ROOTFS_DIR}" /bin/bash -c 'cd /workspace; export QT_QPA_PLATFORM=offscreen; "$@"' -- "$@"
