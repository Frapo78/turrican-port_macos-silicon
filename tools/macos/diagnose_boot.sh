#!/usr/bin/env bash
# Turrican II boot and disk diagnostics for macOS Apple Silicon.
# Does not upload or archive game data.
set -Eeuo pipefail

WORK="/Users/fra/APPS/TurricanII_silicon"
SRC="$WORK/source"
BUILDER="$WORK/TurricanII_dev_allinone.sh"
APP="$WORK/staging/turrican2.app"
BIN="$APP/Contents/MacOS/turrican2"
ADF="$HOME/Library/Application Support/Turrican II/T2.adf"
RUN_ID="bootdiag-$(date +%Y%m%d-%H%M%S)"
LOG_DIR="$WORK/logs/$RUN_ID"
ZIP="$WORK/$RUN_ID.zip"
LATEST="$WORK/bootdiag-latest.zip"
mkdir -p "$LOG_DIR"

# The existing local build wrapper can still be used for subsequent attempts.
# Avoid unconditionally pulling when testing uncommitted changes.
if [[ "$#" -eq 0 || "$1" != "--no-build" ]]; then
  if [[ ! -f "$BUILDER" ]]; then
    echo "Builder mancante: $BUILDER" >&2
    exit 1
  fi
  echo "Compilazione e aggiornamento fork..."
  if ! bash "$BUILDER" build > "$LOG_DIR/build.log" 2>&1; then
    echo "Errore di build. Dettagli: $LOG_DIR/build.log" >&2
    (cd "$WORK/logs" && /usr/bin/zip -q "$ZIP" "$RUN_ID/build.log")
    cp "$ZIP" "$LATEST"
    exit 1
  fi
fi

if [[ ! -x "$BIN" ]]; then
  echo "Eseguibile assente: $BIN" >&2
  exit 1
fi

{
  echo "Timestamp: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "Mac architecture: $(uname -m)"
  echo "Binary: $BIN"
  file "$BIN"
  echo "--- Git ---"
  if [[ -d "$SRC/.git" ]]; then
    git -C "$SRC" rev-parse --short HEAD || true
    git -C "$SRC" status --short || true
  fi
  echo "--- ADF metadata (not the disk contents) ---"
  if [[ -f "$ADF" ]]; then
    stat -f 'Imported disk size: %z bytes' "$ADF"
    shasum -a 256 "$ADF"
  else
    echo "No imported ADF found. The application may show its file picker."
  fi
  echo "--- Linked dependencies ---"
  otool -L "$BIN"
  echo "--- Code signature ---"
  codesign --verify --deep --strict "$APP" 2>&1 || true
} > "$LOG_DIR/environment.txt" 2>&1

# In-game screenshot is produced by the project's scripted frame recorder.
# No ADF or dump of emulated RAM is collected.
printf '90 shot %s\n300 shot %s\n600 shot %s\n' \
  "$LOG_DIR/frame-090.ppm" \
  "$LOG_DIR/frame-300.ppm" \
  "$LOG_DIR/frame-600.ppm" > "$LOG_DIR/scenario.txt"

echo "Eseguo 650 frame (~13 secondi): $(date)"
echo "Le schermate sono salvate automaticamente e il gioco si chiude al termine."
STATUS=0
(
  cd "$WORK"
  T2_BOOTDIAG=1 "$BIN" --verbose --frames 650 --script "$LOG_DIR/scenario.txt"
) > "$LOG_DIR/emulator.log" 2>&1 || STATUS=$?
echo "$STATUS" > "$LOG_DIR/exit-code.txt"

# Keep report small and relevant. The game ADF and saved disk are excluded.
(
  cd "$WORK/logs"
  /usr/bin/zip -qr "$ZIP" "$RUN_ID"
)
cp -f "$ZIP" "$LATEST"
echo
echo "Exit code: $STATUS"
echo "Diagnostica: $LATEST"
echo "Allega questo ZIP per il prossimo controllo. Non contiene il gioco."
