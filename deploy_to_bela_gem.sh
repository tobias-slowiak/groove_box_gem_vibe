#!/usr/bin/env bash
set -euo pipefail

# Bash mirror of deploy_to_bela_gem.ps1
# Usage: ./deploy_to_bela_gem.sh [--debug] [--rebuild] [--copy-all]

DEBUG=0
REBUILD=0
COPY_ALL=0
VERBOSE=0
BUILD_ONLY=0

for arg in "$@"; do
  case "$arg" in
    --debug) DEBUG=1 ;;
    --rebuild) REBUILD=1 ;;
    --copy-all) COPY_ALL=1 ;;
    --verbose) VERBOSE=1 ;;
    --build-only) BUILD_ONLY=1 ;;
    *) echo "Unknown argument: $arg" >&2; exit 1 ;;
  esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "Starting deployment to Bela..."

BELA_IP="192.168.6.2"
PROJECT="instrumentFromPC"
REMOTE_FOLDER="/root/Bela/projects/${PROJECT}"
MAKE_DIR="/root/Bela"
TARGET="run"

LOCAL_RENDER="${SCRIPT_DIR}/render.cpp"
LOCAL_INCLUDE="${SCRIPT_DIR}/include"
LOCAL_SRC="${SCRIPT_DIR}/src"
LOCAL_U8G2="${SCRIPT_DIR}/u8g2"
STATE_FILE="${SCRIPT_DIR}/.deploy_state.txt"

REMOTE_USER_HOST="root@${BELA_IP}"

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

ssh_run() {
  local cmd="$1"
  ssh "${SSH_OPTS[@]}" "$REMOTE_USER_HOST" "$cmd" </dev/null
}

ssh_run_tty() {
  local cmd="$1"
  ssh -tt "${SSH_OPTS[@]}" "$REMOTE_USER_HOST" "$cmd" </dev/null
}

sync_bela_clock() {
  local now_utc
  now_utc="$(date -u +"%Y-%m-%d %H:%M:%S")"
  echo "[bela] Syncing clock to host UTC: ${now_utc}"
  ssh_run "date -u -s '${now_utc}'"
}

require_tool ssh
require_tool scp
require_tool sha256sum

[[ -f "$LOCAL_RENDER" ]] || { echo "Missing file: $LOCAL_RENDER" >&2; exit 1; }
[[ -d "$LOCAL_INCLUDE" ]] || { echo "Missing directory: $LOCAL_INCLUDE" >&2; exit 1; }
[[ -d "$LOCAL_SRC" ]] || { echo "Missing directory: $LOCAL_SRC" >&2; exit 1; }
[[ -d "$LOCAL_U8G2" ]] || { echo "Missing directory: $LOCAL_U8G2" >&2; exit 1; }

declare -A CREATED_REMOTE_DIRS=()

ensure_remote_dir() {
  local dir="$1"
  [[ -z "$dir" ]] && return
  if [[ -z "${CREATED_REMOTE_DIRS[$dir]+x}" ]]; then
    ssh_run "mkdir -p '$dir'"
    CREATED_REMOTE_DIRS["$dir"]=1
  fi
}

remote_dir_for_path() {
  local path="$1"
  echo "${path%/*}"
}

remove_remote_artifacts() {
  local key="$1"
  [[ -z "$key" ]] && return

  local without_ext="${key%.*}"
  local patterns=()

  if [[ "$key" == "render.cpp" ]]; then
    patterns+=("build/render.cpp.*")
    if [[ "$without_ext" != "$key" ]]; then
      patterns+=("build/${without_ext}.*")
    fi
  elif [[ "$key" == src/* ]]; then
    patterns+=("build/${key}.*")
    if [[ "$without_ext" != "$key" ]]; then
      patterns+=("build/${without_ext}.*")
    fi
  fi

  if [[ "${#patterns[@]}" -eq 0 ]]; then
    return
  fi

  for pattern in "${patterns[@]}"; do
    ssh_run "cd '$REMOTE_FOLDER' && rm -f $pattern"
  done
}

echo "Reload systemctl"
ssh_run "systemctl daemon-reload"

sync_bela_clock

ssh_run "mkdir -p '$REMOTE_FOLDER' '$REMOTE_FOLDER/include' '$REMOTE_FOLDER/src' '$REMOTE_FOLDER/u8g2'"

if [[ "$REBUILD" -eq 1 ]]; then
  read -r -p "are you sure you want to rebuild (y/N)? " confirm1
  confirm1="${confirm1,,}"
  if [[ "$confirm1" != "y" ]]; then
    echo "Rebuild cancelled."
    exit 0
  fi
  read -r -p "are you really sure (y/N)? " confirm2
  confirm2="${confirm2,,}"
  if [[ "$confirm2" != "y" ]]; then
    echo "Rebuild cancelled."
    exit 0
  fi
  echo "[bela] Removing remote build directory..."
  ssh_run "rm -rf /root/Bela/projects/instrumentFromPC/build/src"
fi

if [[ "$COPY_ALL" -eq 1 ]]; then
  echo "[bela] Copying all files (forced)."
fi

declare -A OLD_STATE=()
if [[ -f "$STATE_FILE" ]]; then
  while IFS='|' read -r key hash; do
    [[ -n "$key" ]] && OLD_STATE["$key"]="$hash"
  done < "$STATE_FILE"
fi

declare -A NEW_STATE=()
declare -a COPIED=()
PROCESSED=0
TOTAL=0

count_files() {
  local dir="$1"
  local count
  count="$(find "$dir" -type f | wc -l | awk '{print $1}')"
  echo "$count"
}

TOTAL=$((1 + $(count_files "$LOCAL_INCLUDE") + $(count_files "$LOCAL_SRC") + $(count_files "$LOCAL_U8G2")))
echo "[local] Total files to scan: ${TOTAL}"

add_entry() {
  local key="$1"
  local local_path="$2"
  local remote_path="$3"

  PROCESSED=$((PROCESSED + 1))

  local hash
  hash="$(sha256sum "$local_path" | awk '{print $1}')"
  NEW_STATE["$key"]="$hash"

  if [[ "$COPY_ALL" -eq 1 || -z "${OLD_STATE[$key]+x}" || "${OLD_STATE[$key]}" != "$hash" ]]; then
    local remote_dir
    remote_dir="$(remote_dir_for_path "$remote_path")"
    ensure_remote_dir "$remote_dir"
    if [[ "$VERBOSE" -eq 1 ]]; then
      echo "copying $key (${PROCESSED}/${TOTAL})"
    elif (( PROCESSED % 25 == 0 )); then
      echo "progress: ${PROCESSED}/${TOTAL}"
    fi
    scp -q "${SCP_OPTS[@]}" "$local_path" "${REMOTE_USER_HOST}:${remote_path}" </dev/null
    COPIED+=("$key")
    remove_remote_artifacts "$key"
    if [[ "$VERBOSE" -eq 1 ]]; then
      echo "copied $(basename "$key")"
    fi
  else
    if (( PROCESSED % 25 == 0 )); then
      echo "progress: ${PROCESSED}/${TOTAL}"
    fi
  fi
}

echo "[local] Scanning render.cpp"
add_entry "render.cpp" "$LOCAL_RENDER" "$REMOTE_FOLDER/render.cpp"

echo "[local] Scanning include/"
while IFS= read -r -d '' file; do
  rel="${file#$LOCAL_INCLUDE/}"
  key="include/${rel}"
  add_entry "$key" "$file" "$REMOTE_FOLDER/include/${rel}"
done < <(find "$LOCAL_INCLUDE" -type f -print0)

echo "[local] Scanning src/"
while IFS= read -r -d '' file; do
  rel="${file#$LOCAL_SRC/}"
  key="src/${rel}"
  add_entry "$key" "$file" "$REMOTE_FOLDER/src/${rel}"
done < <(find "$LOCAL_SRC" -type f -print0)

echo "[local] Scanning u8g2/"
while IFS= read -r -d '' file; do
  rel="${file#$LOCAL_U8G2/}"
  key="u8g2/${rel}"
  add_entry "$key" "$file" "$REMOTE_FOLDER/u8g2/${rel}"
done < <(find "$LOCAL_U8G2" -type f -print0)

echo "[local] Scan/copy pass complete."

declare -a REMOVED=()
for key in "${!OLD_STATE[@]}"; do
  if [[ -z "${NEW_STATE[$key]+x}" ]]; then
    ssh_run "rm -f '$REMOTE_FOLDER/$key'"
    REMOVED+=("$key")
  fi
done

{
  for key in "${!NEW_STATE[@]}"; do
    echo "${key}|${NEW_STATE[$key]}"
  done
} > "$STATE_FILE"

echo "[bela] Copied ${#COPIED[@]} file(s); removed ${#REMOVED[@]}."

REMOTE_U8G2="${REMOTE_FOLDER}/u8g2"
CFLAGS="-I${REMOTE_U8G2}/csrc"
CPPFLAGS="-I${REMOTE_U8G2}/csrc"
if [[ "$DEBUG" -eq 1 ]]; then
  CFLAGS+=" -DDEBUG_BUILD -UNDEBUG"
  CPPFLAGS+=" -DDEBUG_BUILD -UNDEBUG"
fi

if [[ "$BUILD_ONLY" -eq 1 ]]; then
  TARGET=""
fi

MAKE_CMD="cd '${MAKE_DIR}' && env CFLAGS='${CFLAGS}' CPPFLAGS='${CPPFLAGS}' make PROJECT='${PROJECT}' ${TARGET}"
if [[ "$DEBUG" -eq 1 ]]; then
  MAKE_CMD+=" debug"
fi

if [[ "$BUILD_ONLY" -eq 1 ]]; then
  echo "[bela] Starting build (no run)..."
else
  echo "[bela] Starting build/run..."
fi
ssh_run_tty "$MAKE_CMD"

echo "[bela] Done."
