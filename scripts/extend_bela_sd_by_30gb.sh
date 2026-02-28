#!/usr/bin/env bash
set -euo pipefail

# Safely extend Bela SD root partition (mmcblk0p3) by +30,000,000,000 bytes (30 GB decimal)
# while keeping BOOT (p1) and swap (p2) untouched.
#
# Usage:
#   sudo ./scripts/extend_bela_sd_by_30gb.sh
#
# Notes:
# - The script aborts if current partition layout differs from expected values.
# - It creates a backup of partition metadata before changes.

DEVICE="/dev/mmcblk0"
P1="${DEVICE}p1"
P2="${DEVICE}p2"
P3="${DEVICE}p3"

# Expected current layout (from this machine before resizing)
EXPECTED_TOTAL_SECTORS=121536512
EXPECTED_P1_START=2048
EXPECTED_P1_SIZE=524288
EXPECTED_P2_START=526336
EXPECTED_P2_SIZE=2097152
EXPECTED_P3_START=2623488
EXPECTED_P3_SIZE=12646400
SECTOR_SIZE=512

# +30 GB (decimal)
ADD_BYTES=30000000000
ADD_SECTORS=$((ADD_BYTES / SECTOR_SIZE))
NEW_P3_SIZE=$((EXPECTED_P3_SIZE + ADD_SECTORS))
NEW_P3_END=$((EXPECTED_P3_START + NEW_P3_SIZE - 1))

if [[ "${EUID}" -ne 0 ]]; then
  echo "Run as root: sudo $0"
  exit 1
fi

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || { echo "Missing command: $1"; exit 1; }
}

need_cmd lsblk
need_cmd partprobe
need_cmd parted
need_cmd e2fsck
need_cmd resize2fs
need_cmd sfdisk
need_cmd findmnt
need_cmd mount
need_cmd umount
need_cmd swapoff

echo "Checking current partition layout..."
TOTAL_SECTORS="$(cat /sys/class/block/mmcblk0/size)"
P1_START="$(cat /sys/class/block/mmcblk0/mmcblk0p1/start)"
P1_SIZE="$(cat /sys/class/block/mmcblk0/mmcblk0p1/size)"
P2_START="$(cat /sys/class/block/mmcblk0/mmcblk0p2/start)"
P2_SIZE="$(cat /sys/class/block/mmcblk0/mmcblk0p2/size)"
P3_START="$(cat /sys/class/block/mmcblk0/mmcblk0p3/start)"
P3_SIZE="$(cat /sys/class/block/mmcblk0/mmcblk0p3/size)"

[[ "${TOTAL_SECTORS}" == "${EXPECTED_TOTAL_SECTORS}" ]] || { echo "Unexpected disk size sectors: ${TOTAL_SECTORS}"; exit 1; }
[[ "${P1_START}" == "${EXPECTED_P1_START}" ]] || { echo "Unexpected p1 start: ${P1_START}"; exit 1; }
[[ "${P1_SIZE}" == "${EXPECTED_P1_SIZE}" ]] || { echo "Unexpected p1 size: ${P1_SIZE}"; exit 1; }
[[ "${P2_START}" == "${EXPECTED_P2_START}" ]] || { echo "Unexpected p2 start: ${P2_START}"; exit 1; }
[[ "${P2_SIZE}" == "${EXPECTED_P2_SIZE}" ]] || { echo "Unexpected p2 size: ${P2_SIZE}"; exit 1; }
[[ "${P3_START}" == "${EXPECTED_P3_START}" ]] || { echo "Unexpected p3 start: ${P3_START}"; exit 1; }
[[ "${P3_SIZE}" == "${EXPECTED_P3_SIZE}" ]] || { echo "Unexpected p3 size: ${P3_SIZE}"; exit 1; }

if (( NEW_P3_END >= TOTAL_SECTORS )); then
  echo "Not enough free sectors to extend by 30 GB."
  exit 1
fi

echo "Target:"
echo "- Current p3 size sectors: ${P3_SIZE}"
echo "- Add sectors: ${ADD_SECTORS} (+30 GB decimal)"
echo "- New p3 size sectors: ${NEW_P3_SIZE}"
echo "- New p3 end sector: ${NEW_P3_END}"
echo

echo "Current mounts on ${DEVICE}:"
lsblk -o NAME,SIZE,FSTYPE,MOUNTPOINTS "${DEVICE}"

echo
echo "Unmounting SD card partitions (if mounted)..."
if findmnt -rn -S "${P3}" >/dev/null; then
  umount "${P3}"
fi
if findmnt -rn -S "${P1}" >/dev/null; then
  umount "${P1}"
fi

echo "Disabling swap on ${P2} if active..."
swapoff "${P2}" 2>/dev/null || true

BACKUP_DIR="$(pwd)/partition_backups"
mkdir -p "${BACKUP_DIR}"
STAMP="$(date +%Y%m%d_%H%M%S)"
SFDISK_BAK="${BACKUP_DIR}/mmcblk0_${STAMP}.sfdisk"
DD_BAK="${BACKUP_DIR}/mmcblk0_${STAMP}_first8MiB.bin"

echo "Backing up partition table to ${SFDISK_BAK}"
sfdisk -d "${DEVICE}" > "${SFDISK_BAK}"

echo "Backing up first 8 MiB to ${DD_BAK}"
dd if="${DEVICE}" of="${DD_BAK}" bs=1M count=8 status=progress
sync

echo "Running filesystem pre-check on ${P3}..."
e2fsck -f "${P3}"

echo "Resizing partition 3 end to sector ${NEW_P3_END}..."
parted -s "${DEVICE}" unit s resizepart 3 "${NEW_P3_END}s"
sync
partprobe "${DEVICE}"
sleep 1

echo "Growing ext4 filesystem to fill resized partition..."
resize2fs "${P3}"
sync

echo "Running filesystem post-check on ${P3}..."
e2fsck -f "${P3}"

echo
echo "Done. Final layout:"
lsblk -b -o NAME,SIZE,START,TYPE,FSTYPE,MOUNTPOINTS "${DEVICE}"
echo
echo "Backups:"
echo "- ${SFDISK_BAK}"
echo "- ${DD_BAK}"
