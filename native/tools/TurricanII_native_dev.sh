#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Local native-engine M0 build, packaging and diagnostics. Never bundles an ADF.
set -Eeuo pipefail
WORK="${T2_WORKDIR:-$HOME/APPS/TurricanII_silicon}"
SRC="$WORK/source"
BRANCH="feature/native-engine-m0-continuation"
BUILD="$WORK/build-native-dev"
STAGING="$WORK/staging-native-dev"
SDL_VERSION="2.32.10"
SDL_PREFIX="$WORK/deps/SDL2-install-$SDL_VERSION"
SDL_SRC="$WORK/deps/SDL2-$SDL_VERSION"
MODE="${1:-all}"
ZIP="$WORK/Turrican-II-Native-dev-arm64.zip"
mkdir -p "$WORK" "$WORK/logs" "$STAGING"
LOG="$WORK/logs/native-$(date +%Y%m%d-%H%M%S)-${BASHPID}"
mkdir -p "$LOG/screenshots" "$LOG/crashes"
touch "$LOG/started.marker"
APP="$STAGING/turrican2_native.app"
BIN="$APP/Contents/MacOS/turrican2_native"
report() { echo "[Turrican Native] $*"; }
cat_help() {
  cat <<'HELP'
Turrican II native ARM64 (C++20) — script di sviluppo permanente

  bash TurricanII_native_dev.sh all       Build + test headless + demo interattiva
  bash TurricanII_native_dev.sh build     Compila/crea .app e ZIP autonomo
  bash TurricanII_native_dev.sh test      Test senza ADF (240 frame) + screenshot BMP
  bash TurricanII_native_dev.sh run       Demo giocabile e runtime.log (F12 screenshot)
  bash TurricanII_native_dev.sh run --with-adf  Selettore ADF, solo identificazione
  bash TurricanII_native_dev.sh run --frames 600  Demo limitata a 600 frame
  bash TurricanII_native_dev.sh diagnose Raccoglie log e dipendenze senza build
  bash TurricanII_native_dev.sh update   Fast-forward del branch attualmente aperto

Le modifiche locali a Git non vengono toccate; nessun cambio di branch automatico.
Output in ~/APPS/TurricanII_silicon/staging-native-dev/turrican2_native.app
ZIP: Turrican-II-Native-dev-arm64.zip
Diagnostica: native-diagnostics-latest.zip
HELP
}
if [[ "$MODE" == help || "$MODE" == -h || "$MODE" == --help ]]; then cat_help; exit 0; fi
case "$MODE" in all|build|test|run|diagnose|update) ;; *) echo "Modalità sconosciuta: $MODE" >&2; exit 2;; esac
shift || :
WITH_ADF=0; RUN_FRAMES=0; CAPTURE_EVERY=120
while (($#)); do
  case "$1" in
    --with-adf) WITH_ADF=1; shift ;;
    --frames) [[ $# -ge 2 && "$2" =~ ^[1-9][0-9]*$ ]] || { echo "Invalid --frames" >&2; exit 2; }; RUN_FRAMES="$2"; shift 2 ;;
    --capture-every) [[ $# -ge 2 && "$2" =~ ^[1-9][0-9]*$ ]] || { echo "Invalid --capture-every" >&2; exit 2; }; CAPTURE_EVERY="$2"; shift 2 ;;
    --local) shift ;; # Deprecated; default is local and never switches branches.
    *) echo "Opzione sconosciuta: $1" >&2; exit 2;;
  esac
done
[[ "$(uname -s)" == Darwin && "$(uname -m)" == arm64 ]] || { echo "Richiesto macOS Apple Silicon" >&2; exit 2; }

# All commands produce an inspectable report, including a failed build.
exec > >(tee -a "$LOG/terminal.log") 2>&1
finish() {
  local status="$1"; trap - ERR EXIT
  printf 'Exit code: %s\nMode: %s\nGit: ' "$status" "$MODE" > "$LOG/SUMMARY.txt"
  git -C "$SRC" rev-parse --short HEAD >> "$LOG/SUMMARY.txt" 2>/dev/null || :
  {
    uname -a; sw_vers 2>/dev/null || :
    if [[ -d "$SRC/.git" ]]; then git -C "$SRC" status --short || :; fi
    if [[ -x "$BIN" ]]; then file "$BIN"; otool -L "$BIN"; codesign --verify --deep --strict "$APP" || :; fi
  } > "$LOG/system.txt" 2>&1
  if [[ -d "$HOME/Library/Logs/DiagnosticReports" ]]; then
    find "$HOME/Library/Logs/DiagnosticReports" -maxdepth 1 -type f \
      \( -iname '*turrican2_native*.ips' -o -iname '*turrican2_native*.crash' \) \
      -newer "$LOG/started.marker" -exec cp -n '{}' "$LOG/crashes/" \; 2>/dev/null || :
  fi
  (cd "$WORK/logs" && ditto -c -k --keepParent "$(basename "$LOG")" "$WORK/native-diagnostics-latest.zip") || :
  report "Esito=$status; log: $LOG"
  report "ZIP diagnostico: $WORK/native-diagnostics-latest.zip"
  exit "$status"
}
trap 'finish "$?"' EXIT
trap 'echo "[Turrican Native ERROR] riga $LINENO: $BASH_COMMAND" >&2' ERR
logged() {
  local name="$1" rc; shift
  report "$name..."
  if "$@" > "$LOG/$name.log" 2>&1; then
    report "$name: OK"
  else
    rc="$?"; report "ERRORE $name (exit $rc). Estratto:"
    tail -n 35 "$LOG/$name.log" >&2 || :
    return "$rc"
  fi
}

fail() { echo "[native M0 ERROR] $*" >&2; echo "Logs: $LOG" >&2; exit 1; }

[[ -d "$SRC/.git" ]] || fail "Missing git checkout: $SRC"
if [[ "$MODE" == "build" || "$MODE" == "all" ]]; then
  if true; then
    # Build the current local checkout, even if there are edits. Never switch
    # branch, reset a working tree, or overwrite Codex's in-progress work.
    report "Checkout locale: $(git -C "$SRC" rev-parse --short HEAD), branch $(git -C "$SRC" branch --show-current)"
    if [[ -n "$(git -C "$SRC" status --porcelain)" ]]; then report "Modifiche locali presenti: NON vengono sovrascritte."; fi
  fi
  report "Using locally compiled classic SDL2 $SDL_VERSION (not SDL3 compatibility)"
  SDL_CMAKE_FILE="$(find "$SDL_PREFIX" -type f -name SDL2Config.cmake -print -quit 2>/dev/null || true)"
  if [[ -z "$SDL_CMAKE_FILE" ]]; then
    report "SDL2 classica assente: compilo SDL2 $SDL_VERSION da sorgenti ufficiali."
    for cmd in curl shasum tar cmake; do command -v "$cmd" >/dev/null || fail "Manca $cmd"; done
    SDL_ARCHIVE="$WORK/deps/SDL2-$SDL_VERSION.tar.gz"
    SDL_SHA="5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165"
    if [[ ! -f "$SDL_ARCHIVE" ]]; then
      logged SDL2-download curl -fL --retry 3 "https://www.libsdl.org/release/SDL2-$SDL_VERSION.tar.gz" -o "$SDL_ARCHIVE.part"
      mv "$SDL_ARCHIVE.part" "$SDL_ARCHIVE"
    fi
    [[ "$(shasum -a 256 "$SDL_ARCHIVE" | awk '{print $1}')" == "$SDL_SHA" ]] || fail "Checksum SDL2 non valida: $SDL_ARCHIVE"
    [[ -d "$SDL_SRC" ]] || logged SDL2-extract tar -xzf "$SDL_ARCHIVE" -C "$WORK/deps"
    logged SDL2-config cmake -S "$SDL_SRC" -B "$WORK/deps/SDL2-build-$SDL_VERSION" \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
      -DCMAKE_INSTALL_PREFIX="$SDL_PREFIX" -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TESTS=OFF
    logged SDL2-build cmake --build "$WORK/deps/SDL2-build-$SDL_VERSION" --parallel "$(sysctl -n hw.ncpu)"
    logged SDL2-install cmake --install "$WORK/deps/SDL2-build-$SDL_VERSION"
    SDL_CMAKE_FILE="$(find "$SDL_PREFIX" -type f -name SDL2Config.cmake -print -quit 2>/dev/null || true)"
  fi
  [[ -n "$SDL_CMAKE_FILE" ]] || fail "SDL2Config.cmake mancante dopo installazione."
  SDL_CMAKE_DIR="$(dirname "$SDL_CMAKE_FILE")"
  SDL_LIB="$(find "$SDL_PREFIX" \( -type f -o -type l \) -name libSDL2-2.0.0.dylib -print -quit)"
  [[ -n "$SDL_LIB" ]] || fail "SDL2 dylib missing"
  # build-native-dev is dedicated to this script: preserve old build-native/.
  report "Compiling independent ARM64 native engine"
  logged configure cmake -S "$SRC/native" -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_PREFIX_PATH="$SDL_PREFIX" -DSDL2_DIR="$SDL_CMAKE_DIR" \
    -DT2_NATIVE_WITH_SDL=ON -DT2_NATIVE_TESTS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
  logged compile cmake --build "$BUILD" --target turrican2_native t2_native_tests --parallel "$(sysctl -n hw.ncpu)"
  logged unit-tests ctest --test-dir "$BUILD" --output-on-failure
  BUNDLE="$BUILD/turrican2_native.app"
  test -x "$BUNDLE/Contents/MacOS/turrican2_native" || fail "App target missing"
  lipo "$BUNDLE/Contents/MacOS/turrican2_native" -verify_arch arm64 || fail "Not arm64"
  rm -rf "$APP" # only our own staged dev app
  ditto "$BUNDLE" "$APP"
  mkdir -p "$APP/Contents/Frameworks" "$APP/Contents/Resources"
  BUNDLED="$APP/Contents/Frameworks/libSDL2-2.0.0.dylib"
  cp -L "$SDL_LIB" "$BUNDLED"
  chmod u+w "$BUNDLED"
  install_name_tool -id '@rpath/libSDL2-2.0.0.dylib' "$BUNDLED"
  count=0
  while IFS= read -r lib; do
    [[ -n "$lib" ]] || continue
    install_name_tool -change "$lib" '@executable_path/../Frameworks/libSDL2-2.0.0.dylib' "$BIN"
    count=$((count + 1))
  done < <(otool -L "$BIN" | awk '$1 ~ /libSDL2(-2\.0\.0)?\.dylib/ {print $1}')
  (( count > 0 )) || fail "SDL2 dylib dependency not located"
  if [[ -f "$SRC/LICENSES/GPL-3.0-or-later.txt" ]]; then cp "$SRC/LICENSES/GPL-3.0-or-later.txt" "$APP/Contents/Resources/GPL-3.0-or-later.txt"; fi
  if [[ -f "$SDL_SRC/LICENSE.txt" ]]; then
    cp "$SDL_SRC/LICENSE.txt" "$APP/Contents/Resources/SDL2-LICENSE.txt"
  fi
  # Refuse external non-Apple runtime dependencies, including SDL3.
  for target in "$BIN" "$BUNDLED"; do
    while IFS= read -r line; do
      [[ "$line" == *"(compatibility version"* ]] || continue
      path="${line%% (*}"; path="${path#"${path%%[![:space:]]*}"}"
      case "$path" in
        /System/Library/*|/usr/lib/*|@executable_path/../Frameworks/libSDL2-2.0.0.dylib|@rpath/libSDL2-2.0.0.dylib) ;;
        *) fail "Unbundled dependency: $path" ;;
      esac
    done < <(otool -L "$target" | tail -n +2)
  done
  if otool -L "$BIN" "$BUNDLED" | grep -E 'SDL3|/opt/homebrew|/usr/local'; then
    fail "Unexpected SDL3 or Homebrew runtime dependency"
  fi
  lipo "$BUNDLED" -verify_arch arm64 || fail "SDL2 dylib not ARM64"
  if find "$APP" -type f \( -iname '*.adf' -o -iname '*.ipf' \) | grep -q .; then
    fail "Refusing to package original game data"
  fi
  codesign --force --sign - "$BUNDLED"
  codesign --force --sign - "$APP"
  codesign --verify --deep --strict "$APP" > "$LOG/signature.log" 2>&1 || fail "Invalid code signature"
  logged help-check "$BIN" --help
  ditto -c -k --keepParent --sequesterRsrc "$APP" "$ZIP"
  shasum -a 256 "$ZIP" > "$ZIP.sha256"
  report "Development ZIP: $ZIP"
fi
ensure_app() {
  if [[ ! -x "$BIN" ]]; then
    report 'App assente: eseguo prima la build locale.'
    bash "$SRC/native/tools/TurricanII_native_dev.sh" build
  fi
}
if [[ "$MODE" == update ]]; then
  [[ -z "$(git -C "$SRC" status --porcelain)" ]] || fail "Modifiche locali: non aggiorno."
  CUR="$(git -C "$SRC" branch --show-current)"
  [[ -n "$CUR" ]] || fail 'Detached HEAD, niente aggiornamento.'
  logged fetch git -C "$SRC" fetch origin "$CUR"
  logged fast-forward git -C "$SRC" merge --ff-only "origin/$CUR"
fi
if [[ "$MODE" == test || "$MODE" == all ]]; then
  ensure_app
  report "Test automatico headless: 240 frame, screenshot e telemetria."
  logged headless-smoke env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    "$BIN" --demo --debug --frames 240 --capture-dir "$LOG/screenshots" --capture-every "$CAPTURE_EVERY"
fi
if [[ "$MODE" == run || "$MODE" == all ]]; then
  ensure_app
  args=(--debug --capture-dir "$LOG/screenshots")
  if (( WITH_ADF == 0 )); then args=(--demo "${args[@]}"); fi
  (( RUN_FRAMES == 0 )) || args+=(--frames "$RUN_FRAMES" --capture-every "$CAPTURE_EVERY")
  report 'Avvio demo interattiva; Esc chiude, F9 cambia formato, F12 screenshot.'
  logged runtime "$BIN" "${args[@]}"
fi
report "Script versionato sul branch corrente. L'originale Turrican II non è ancora portato."
