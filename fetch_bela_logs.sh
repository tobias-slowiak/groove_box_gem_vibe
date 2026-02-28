#!/usr/bin/env bash
set -euo pipefail

# Copy logs from Bela to local logs folder.
# Existing local files are backed up before overwrite.
#
# Usage:
#   ./fetch_bela_logs.sh [remote_user_host] [remote_log_dir] [local_log_dir] [local_backup_dir]
# Example:
#   ./fetch_bela_logs.sh root@192.168.6.2 /opt/Bela/logs/instrument ./logs/bela/instrument ./logs/bela/instrument_backups

REMOTE_USER_HOST="${1:-root@192.168.6.2}"
REMOTE_LOG_DIR="${2:-/opt/Bela/logs/instrument}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOCAL_LOG_DIR="${3:-${SCRIPT_DIR}/logs/bela/instrument}"
LOCAL_BACKUP_DIR="${4:-${SCRIPT_DIR}/logs/bela/instrument_backups}"
BACKUP_STAMP="$(date +"%Y-%m-%d_%H-%M-%S")"

require_tool() {
  local name="$1"
  if ! command -v "$name" >/dev/null 2>&1; then
    echo "Required command '$name' not found." >&2
    exit 1
  fi
}

SSH_OPTS=(
  -o StrictHostKeyChecking=accept-new
  -o BatchMode=yes
  -o PasswordAuthentication=no
  -o PreferredAuthentications=publickey
  -o LogLevel=ERROR
)

SCP_OPTS=(
  -o StrictHostKeyChecking=accept-new
  -o BatchMode=yes
  -o PasswordAuthentication=no
  -o PreferredAuthentications=publickey
  -o LogLevel=ERROR
  -B
)

require_tool ssh
require_tool scp
require_tool cp
require_tool find
require_tool date

mkdir -p "$LOCAL_LOG_DIR"
mkdir -p "$LOCAL_BACKUP_DIR"

echo "Remote: ${REMOTE_USER_HOST}:${REMOTE_LOG_DIR}"
echo "Local:  ${LOCAL_LOG_DIR}"
echo "Backup: ${LOCAL_BACKUP_DIR}"

REMOTE_LIST=""
if ! REMOTE_LIST="$(
  ssh "${SSH_OPTS[@]}" "$REMOTE_USER_HOST" "cd '$REMOTE_LOG_DIR' && find . -type f -print" </dev/null
)"; then
  echo "Failed to read remote logs from ${REMOTE_USER_HOST}:${REMOTE_LOG_DIR}" >&2
  exit 1
fi

mapfile -t REMOTE_FILES <<<"$REMOTE_LIST"

if [[ "${#REMOTE_FILES[@]}" -eq 0 ]]; then
  echo "No log files found on remote."
  exit 0
fi

BACKUP_COUNT=0
COPIED_COUNT=0

for rel in "${REMOTE_FILES[@]}"; do
  rel="${rel#./}"
  [[ -z "$rel" ]] && continue

  local_path="${LOCAL_LOG_DIR}/${rel}"
  local_dir="$(dirname "$local_path")"
  mkdir -p "$local_dir"

  if [[ -f "$local_path" ]]; then
    backup_path="${LOCAL_BACKUP_DIR}/${rel}.${BACKUP_STAMP}.bak"
    mkdir -p "$(dirname "$backup_path")"
    cp -p "$local_path" "$backup_path"
    BACKUP_COUNT=$((BACKUP_COUNT + 1))
    echo "Backup: ${backup_path}"
  fi

  scp -q "${SCP_OPTS[@]}" "${REMOTE_USER_HOST}:${REMOTE_LOG_DIR}/${rel}" "$local_path" </dev/null
  COPIED_COUNT=$((COPIED_COUNT + 1))
  echo "Copied: ${rel}"
done

echo "Done. copied=${COPIED_COUNT}, backups=${BACKUP_COUNT}"
