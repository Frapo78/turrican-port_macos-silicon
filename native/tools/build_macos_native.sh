#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Local native-engine M0 build, packaging and diagnostics. Never bundles an ADF.
set -Eeuo pipefail
WORK="/Users/fra/APPS/TurricanII_silicon"
SRC="$WORK/source"
BRANCH="feature/native-engine-m0"
BUILD="$WORK/build-native"
STAGING="$WORK/staging-native"
SDL_VERSION="2.32.10"
SDL_PREFIX="$WORK/deps/SDL2-install-$SDL_VERSION"
SDL_SRC="$WORK/deps/SDL2-$SDL_VERSION"
MODE="${1:-build}"
ZIP="$WORK/Turrican-II-Native-M0-dev-arm64.zip"
mkdir -p "$WORK" "$WORK/logs" "$STAGING"
LOG="$WORK/logs/native-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$LOG"
APP="$STAGING/turrican2_native.app"
BIN="$APP/Contents/MacOS/turrican2_native"
report() { echo "[native M0] $*"; }
fail() { echo "[native M0 ERROR] $*" >&2; echo "Logs: $LOG" >&2; exit 1; }

[[ -d "$SRC/.git" ]] || fail "Missing git checkout: $SRC"
if [[ "$MODE" == "build" || "$MODE" == "all" ]]; then
  if [[ "${2:-}" != "--local" ]]; then
    if [[ -n "$(git -C "$SRC" status --porcelain)" ]]; then
      fail "Uncommitted changes detected. Use --local to build current checkout."
    fi
    report "Updating the native branch"
    git -C "$SRC" fetch origin "$BRANCH" > "$LOG/git.log" 2>&1
    git -C "$SRC" switch "$BRANCH" >> "$LOG/git.log" 2>&1
    git -C "$SRC" merge --ff-only "origin/$BRANCH" >> "$LOG/git.log" 2>&1
  fi
  report "Using locally compiled classic SDL2 $SDL_VERSION (not SDL3 compatibility)"
  SDL_CMAKE_FILE="$(find "$SDL_PREFIX" -type f -name SDL2Config.cmake -print -quit 2>/dev/null || true)"
  [[ -n "$SDL_CMAKE_FILE" ]] || fail "SDL2 locale mancante in $SDL_PREFIX. Esegui prima TurricanII_dev_allinone.sh build --local."
  SDL_CMAKE_DIR="$(dirname "$SDL_CMAKE_FILE")"
  SDL_LIB="$(find "$SDL_PREFIX" \( -type f -o -type l \) -name libSDL2-2.0.0.dylib -print -quit)"
  [[ -n "$SDL_LIB" ]] || fail "SDL2 dylib missing"
  if [[ -f "$BUILD/CMakeCache.txt" ]] && ! grep -Fq "SDL2_DIR:PATH=$SDL_CMAKE_DIR" "$BUILD/CMakeCache.txt"; then
    rm -rf "$BUILD" # only the native build cache; never user game data
  fi
  report "Compiling independent ARM64 native engine"
  cmake -S "$SRC" -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_PREFIX_PATH="$SDL_PREFIX" -DSDL2_DIR="$SDL_CMAKE_DIR" \
    -DT2_NATIVE_WITH_SDL=ON -DT2_NATIVE_TESTS=ON > "$LOG/configure.log" 2>&1 || fail "Configure failed"
  cmake --build "$BUILD" --target turrican2_native t2_native_tests --parallel "$(sysctl -n hw.ncpu)" \
    > "$LOG/build.log" 2>&1 || fail "Build failed"
  ctest --test-dir "$BUILD" -R t2_native_core --output-on-failure > "$LOG/test.log" 2>&1 || fail "Unit tests failed"
  BUNDLE="$BUILD/native/turrican2_native.app"
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
    ((count+=1))
  done < <(otool -L "$BIN" | awk '$1 ~ /libSDL2(-2\.0\.0)?\.dylib/ {print $1}')
  (( count > 0 )) || fail "SDL2 dylib dependency not located"
  cp "$SRC/LICENSES/GPL-3.0-or-later.txt" "$APP/Contents/Resources/GPL-3.0-or-later.txt"
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
  "$BIN" --help > "$LOG/smoke.log" 2>&1 || fail "Native startup smoke test failed"
  ditto -c -k --keepParent --sequesterRsrc "$APP" "$ZIP"
  shasum -a 256 "$ZIP" > "$ZIP.sha256"
  report "Development ZIP: $ZIP"
fi
if [[ "$MODE" == "test" || "$MODE" == "all" ]]; then
  [[ -x "$BIN" ]] || fail "Build with 'build' first"
  report "Launching native synthetic scene, without ADF. Close window to finish."
  "$BIN" --demo > "$LOG/runtime.log" 2>&1 || fail "Native runtime exited with error"
fi
if [[ "$MODE" == "identify" ]]; then
  [[ $# -ge 2 ]] || fail 'Usage: build_macos_native.sh identify /path/to/ADF'
  python3 "$SRC/native/tools/inspect_adf.py" "$2" --deep
fi
report "Logs: $LOG"
report "Native code only: original Turrican II gameplay is NOT ported in M0."
