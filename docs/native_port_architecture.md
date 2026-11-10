# Native Turrican II for macOS Apple Silicon — target architecture

**Status: design / not implemented.** This is the product goal for the
[Frapo78/turrican-port_macos-silicon](https://github.com/Frapo78/turrican-port_macos-silicon)
fork, not a claim about the current executable.

## Non-negotiable distinction

| Current prototype | Required production port |
| --- | --- |
| ARM64 *host* running \`m68k::Cpu\` interpreter and emulated custom chips | ARM64 *game logic* executing directly as native code |
| Game boot and runtime load data from user-supplied disk image | Once provisioned, gameplay does **not open or read the ADF** |
| 320×256 Amiga display simulation | Classic aspect-preserving display + extended 16:9 view |
| Paula / Amiga keyboard/joystick simulation | Native audio and remappable DualShock 4 input |

Keep the upstream interpreter as a **reference-only regression harness**, not
as a production dependency or as the default runtime in a release advertised
as native.

"Native macOS executable" is **not sufficient** if the game itself still
executes interpreted 68000 instructions.

## Data and rights model: first run only

An ADF can be **identified** technically by container type, size, hashes,
track data, and supported game-version signatures. It is not a legal
certificate of ownership: merely selecting or hashing a file cannot
independently establish legal possession or a license.

There are two viable deployment modes:

1. **Locally extracted proprietary assets (default when redistributable
   assets are unavailable).** At first launch, a native macOS file picker
   selects the user's ADF. An offline importer validates a *supported*
   edition, extracts required artwork, sounds, maps and metadata **on the
   user's own Mac**, and writes a versioned, validated local cache in
   \`~/Library/Application Support/Turrican II/\`. Subsequent gameplay reads
   only the cache; the original ADF can be moved or removed. The ADF is
   therefore a *one-time input*, not a per-frame game disk or emulator ROM.
   Only assets for which extraction/use is appropriate should be processed,
   and the app must not upload them.
2. **Authorized bundled assets.** If the project obtains redistribution
   rights for the original assets or uses legitimately redistributable
   replacements, ship those assets with the app. Only in this mode can the
   ADF serve exclusively as an optional technical check / identifier without
   also being the source for game data. The user-requested "ADF only as proof"
   is **not implementable with proprietary original visuals/audio in the
   bundle absent the rights to distribute them**.

Do not claim that an arbitrary valid ADF is definitely the right Turrican II
edition. Build an explicit, locally maintained list of supported editions
with hashes/version signatures, and a graceful, non-destructive rejection
when required assets cannot be extracted. No ADF or extracted proprietary
assets in git, CI artifacts, diagnostics or release ZIPs. Never log raw game
data or include it in bug reports.

Store only a technical manifest of locally provisioned assets (edition,
extractor version, cache format, file hashes) plus normal user settings.
Provide a re-import option. A newly updated app must migrate or regenerate
the cache without deleting the user's original disk.

### One-time lifecycle

\`\`\`text
First launch:
  NSOpenPanel -> identify supported Turrican II disk -> optional local
  asset extraction -> asset-cache manifest -> native title screen

Next launches:
  validate local manifest -> load cached assets -> native title screen
  (NO ADF reads, NO Amiga trackloader, NO 68000 interpreter)
\`\`\`

## Production module boundaries

- \`native/core/\`: deterministic \`GameState\`, \`World\`, player/enemy motion,
  collisions, weapons, powerups, game rules, stage transitions, save data.
  Do not depend on \`amiga::Machine\` or 68k register addresses.
- \`native/render/\`: Metal renderer or SDL's Metal-backed renderer; original
  pixel-art presentation, scalable viewport, extended 16:9 camera. Drawing
  new offscreen level regions requires real world/map data; merely scaling
  the old 320×256 buffer is not widescreen gameplay.
- \`native/audio/\`: native audio playback/mixing from locally provisioned
  samples/music, with no Paula register emulation during gameplay.
- \`native/input/\`: macOS game controller adapter, DualShock 4 mapping and
  profile persistence. L2 holds the directional beam; right stick aims,
  subject to faithful weapon mechanics and animation compatibility.
- \`native/assets/\`: offline ADF edition detector + one-time extractor,
  trusted manifest, cache loader, versioned resources. Extraction is
  **outside the runtime game loop**.
- \`native/platform/macos/\`: AppKit dialogs, fullscreen Spaces,
  application support, build/package/notarization integration.
- \`src/amiga/\` and \`src/m68k/\`: legacy reference executable only.

C++20 is the portable core language; selected macOS-specific Objective-C++
wrappers are reasonable for AppKit and GameController. GPU/native libraries
can be optimized incrementally without changing game-state semantics.

## Milestones and acceptance gates

1. **M0 — independent boot:** separate native executable/app target can
   launch a synthetic title screen and one controllable placeholder scene
   **without an ADF, 68k interpreter or emulator library linked into the
   native target**. Headless CI state-update smoke tests use synthetic data.
2. **M1 — one-time onboarding:** validate a supported ADF edition and locally
   prepare assets if necessary. On subsequent launches, the binary opens
   only its cache. Test by moving the original ADF after import.
3. **M2 — playable vertical slice:** reimplement player motion, camera,
   collisions, weapons (including beam), enemies, pickups, physics and HUD
   for one stage; compare behavior with reference emulator captures.
4. **M3 — native audiovisual pipeline:** Metal, audio and controller path,
   save games and smooth, pixel-respecting scaling with measured frame
   budgets on Apple Silicon.
5. **M4 — complete game and extended 16:9:** each stage/intro/boss/ending
   implemented and checked; no cropped/stretched 4:3 cheats.
6. **M5 — distribution:** self-contained \`.app\` and ZIP, signing and
   notarization path, no copyrighted game assets in the release.

**Never mark a milestone complete solely because the existing ARM64
interpreter-based build passes.** Native gameplay must not call
\`m68k::Cpu::step\`, \`amiga::Machine::run\`, \`Disk::load\`, or
\`Disk::raw_track\`; production tests should verify target separation.

## Implementation choices

A static 68000-to-C++ recompiler can help analyze or bridge code during
porting, but generated code still tied to Amiga memory-mapped hardware does
not meet the final architecture. The target is to reimplement game rules and
assets in independent native-domain structures, with feature parity checked
against the upstream runtime. Avoid promising performance or completeness
until there are benchmarks and functional tests.

## Scope of the existing macOS PR

\`feature/macos-arm64-app-fullscreen\` is an **interim emulator-based
prototype**, valuable for reference comparisons and diagnosing the supplied
disk edition. It must not be described as the completed native game port.
Work on the native engine should be tracked against the milestones above,
using reviewable commits/PRs. Preserve upstream GPL-3.0-or-later notices for
derived source code.
