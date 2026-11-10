# Turrican II native-engine prototype (M0)

This is a **separate, original C++20 game-prototype target**, not a 68000
interpreter and **not yet a working native conversion of the original game**.
It links only `t2_native_core` and SDL2; no `m68k`, `amiga`, `t2` or `Disk`
library participates in the production loop. All scenery and player graphics
are generated placeholder shapes; **no copyright-protected game assets** are
included. Physics, level layouts, timing and weapons are not yet faithful.

## Implemented

- Fixed-step 60 Hz native player/world update, gravity, platforms, camera
  and a synthetic scrollable level (world extends beyond both view widths).
- 16:9 960×540 logical viewport and 4:3 720×540 alternate viewport (F9).
  The mode changes the visible world width; no image stretching.
- DualShock 4 via SDL2 GameController: left stick/D-pad movement, Cross jump,
  Square fire indicator, L2 directional beam, right stick aiming; hotplug.
  Keyboard: arrows/A/D, Space jump, left Shift beam, I/J/K/L aim.
  Mapping is provisional, not gameplay-complete.
- On macOS: native fullscreen Spaces (Control-Command-F), high-DPI window,
  SDL2 Metal renderer requested with software fallback.
- On first macOS launch: AppKit ADF selection and technical identification of
  one user-supplied known SHA-256; saves **manifest only** under
  `~/Library/Application Support/Turrican II Native/edition.json`.
  On later launches no ADF is opened. `--demo` bypasses onboarding in tests.
- Independent C++ core tests; asset-free CI. No original game content in CI.

## Build just the native core on any modern C++20 toolchain

```bash
cmake -S native -B build-native-core -DT2_NATIVE_WITH_SDL=OFF -DT2_NATIVE_TESTS=ON
cmake --build build-native-core --parallel
ctest --test-dir build-native-core --output-on-failure
```

## Build GUI on macOS Apple Silicon

Build the new root CMake target `turrican2_native`, separately from the old
`turrican2` emulator target. On the owner's Mac, after fetching the branch:

```bash
bash /Users/fra/APPS/TurricanII_silicon/source/native/tools/build_macos_native.sh build
bash /Users/fra/APPS/TurricanII_silicon/source/native/tools/build_macos_native.sh test
```

`test` opens synthetic gameplay without asking for a disk. To exercise the
one-time system picker, directly launch the `.app` without `--demo`. The
current development app may still use locally installed SDL2 libraries and
is not a redistributable, notarized release ZIP.

## Limits

Identifying a matching ADF is not proof of purchase; a copied file matches
the same digest. No native Turrican II levels or sprites are implemented.
No game disk/asset files are committed. Runtime gameplay uses only compiled
C++ code and synthetic geometry. The old emulator remains separate for
reference and diagnostics. See `docs/native_port_architecture.md`.

For a local MFM checksum and directory metadata analysis, use
`python3 native/tools/inspect_adf.py /path/to/TurricanII.adf --deep`.
This command never writes or uploads extracted game data.
