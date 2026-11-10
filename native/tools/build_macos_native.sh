#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Build / test the independent native C++20 prototype on the owner's Mac.
set -Eeuo pipefail
WORK="/Users/fra/APPS/TurricanII_silicon"
SRC="$WORK/source"
BRANCH="feature/native-engine-m0"
BUILD="$WORK/build-native"
MODE="${1:-build}"
mkdir -p "$WORK" "$WORK/logs"
LOG="$WORK/logs/native-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$LOG"
[[ -d "$SRC/.git" ]] || { echo "Missing git checkout: $SRC" >&2; exit 1; }
if [[ "$MODE" == "build" || "$MODE" == "all" ]]; then
  if [[ "${2:-}" != "--local" ]]; then
    if [[ -n "$(git -C "$SRC" status --porcelain)" ]]; then
      echo "Local changes detected; not switching branch. Use '--local' to build current checkout." >&2
      exit 2
    fi
    git -C "$SRC" fetch origin "$BRANCH" > "$LOG/git.log" 2>&1
    git -C "$SRC" switch "$BRANCH"
    git -C "$SRC" pull --ff-only origin "$BRANCH" >> "$LOG/git.log" 2>&1
  fi
  cmake -S "$SRC" -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_PREFIX_PATH="$(brew --prefix)" \
    -DT2_NATIVE_WITH_SDL=ON -DT2_NATIVE_TESTS=ON > "$LOG/configure.log" 2>&1
  cmake --build "$BUILD" --target turrican2_native t2_native_tests --parallel 4 \
    > "$LOG/build.log" 2>&1
  ctest --test-dir "$BUILD" -R t2_native_core --output-on-failure > "$LOG/test.log" 2>&1
  APP="$BUILD/native/turrican2_native.app"
  test -x "$APP/Contents/MacOS/turrican2_native"
  lipo -verify_arch arm64 "$APP/Contents/MacOS/turrican2_native"
  echo "Native app built: $APP"
fi
APP="$BUILD/native/turrican2_native.app"
BIN="$APP/Contents/MacOS/turrican2_native"
if [[ "$MODE" == "test" || "$MODE" == "all" ]]; then
  echo "Launching native synthetic scene (no ADF required in test mode)."
  "$BIN" --demo > "$LOG/runtime.log" 2>&1 || true
fi
if [[ "$MODE" == "identify" ]]; then
  [[ $# -ge 2 ]] || { echo 'Usage: build_macos_native.sh identify /path/to/ADF' >&2; exit 2; }
  python3 "$SRC/native/tools/inspect_adf.py" "$2"
fi
if [[ "$MODE" == "build" || "$MODE" == "all" ]]; then
  echo "To test: bash $SRC/native/tools/build_macos_native.sh test"
  echo "The native prototype is NOT a port of the complete original game yet."
fi
printf '%s\n' "Diagnostics: $LOG"
