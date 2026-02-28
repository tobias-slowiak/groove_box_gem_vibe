#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET_BASE="${1:-$ROOT_DIR/Samples/instruments/vcsl_full}"
LOCAL_CURL="$ROOT_DIR/tools/local_bin/curl_pkg/usr/bin/curl"

ARCHIVES_DIR="$TARGET_BASE/archives"
EXTRACTED_DIR="$TARGET_BASE/extracted"
LICENSES_DIR="$TARGET_BASE/licenses"
MANIFEST_DIR="$TARGET_BASE/manifest"

VCSL_VERSION="1.2.2-RC"
VCSL_ARCHIVE_NAME="VCSL-${VCSL_VERSION}.zip"
VCSL_URL="https://www.dropbox.com/scl/fi/3ei7q85gculfcbefavery/VCSL-1.2.2-RC.zip?rlkey=wzyco4e9qzxl4nnka8tgvi0rp&dl=1"
VCSL_ARCHIVE_PATH="$ARCHIVES_DIR/$VCSL_ARCHIVE_NAME"
VCSL_EXTRACT_DIR="$EXTRACTED_DIR/VCSL-$VCSL_VERSION"

mkdir -p "$ARCHIVES_DIR" "$EXTRACTED_DIR" "$LICENSES_DIR" "$MANIFEST_DIR"

if command -v curl >/dev/null 2>&1; then
  CURL_BIN="$(command -v curl)"
elif [[ -x "$LOCAL_CURL" ]]; then
  CURL_BIN="$LOCAL_CURL"
else
  echo "Error: curl is required but not installed."
  echo "Hint: place a local curl binary at $LOCAL_CURL"
  exit 1
fi

if ! command -v unzip >/dev/null 2>&1; then
  echo "Error: unzip is required but not installed."
  exit 1
fi

echo "Downloading $VCSL_ARCHIVE_NAME..."
"$CURL_BIN" -fL -C - \
  --retry 8 \
  --retry-all-errors \
  --retry-delay 3 \
  --output "$VCSL_ARCHIVE_PATH" \
  "$VCSL_URL"

if file "$VCSL_ARCHIVE_PATH" | grep -qi "HTML"; then
  echo "Error: download result is HTML, not an archive."
  exit 1
fi

mkdir -p "$VCSL_EXTRACT_DIR"
echo "Extracting into $VCSL_EXTRACT_DIR..."
unzip -oq "$VCSL_ARCHIVE_PATH" -d "$VCSL_EXTRACT_DIR"

DOWNLOADED_AT_UTC="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
ARCHIVE_BYTES="$(stat -c%s "$VCSL_ARCHIVE_PATH")"
ARCHIVE_SHA256="$(sha256sum "$VCSL_ARCHIVE_PATH" | awk '{print $1}')"
TOP_LEVEL_DIRS="$(find "$VCSL_EXTRACT_DIR" -mindepth 1 -maxdepth 1 -type d | wc -l | tr -d ' ')"
WAV_COUNT="$(find "$VCSL_EXTRACT_DIR" -type f -name '*.wav' | wc -l | tr -d ' ')"

cat > "$LICENSES_DIR/VCSL_LICENSE_NOTES.txt" << 'EOF'
Source: Versilian Studios Chamber Orchestra Community Edition
Page: https://versilian-studios.com/vcsl/
License: CC0 1.0 (as stated by the source page)
Note: Keep original attribution/readme files from the archive together with samples.
EOF

cat > "$MANIFEST_DIR/vcsl_full_manifest.tsv" << EOF
downloaded_at_utc	source_name	version	license	archive_url	archive_file	archive_bytes	archive_sha256	extracted_path	top_level_dirs	wav_files
$DOWNLOADED_AT_UTC	VCSL	$VCSL_VERSION	CC0 1.0	$VCSL_URL	$VCSL_ARCHIVE_PATH	$ARCHIVE_BYTES	$ARCHIVE_SHA256	$VCSL_EXTRACT_DIR	$TOP_LEVEL_DIRS	$WAV_COUNT
EOF

echo
echo "Done."
echo "Archive:   $VCSL_ARCHIVE_PATH"
echo "Extracted: $VCSL_EXTRACT_DIR"
echo "Manifest:  $MANIFEST_DIR/vcsl_full_manifest.tsv"
echo "License:   $LICENSES_DIR/VCSL_LICENSE_NOTES.txt"
