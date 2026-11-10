# macOS Apple Silicon port (work in progress)

This fork follows [Grumbel/turrican-port](https://github.com/Grumbel/turrican-port)
and keeps the upstream GPL-3.0-or-later license. The proprietary Turrican II
game data is **not included**.

## Build a native ARM64 macOS app

Install the Apple command line developer tools and Homebrew, then:

```sh
brew install cmake sdl2-compat
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_PREFIX_PATH="$(brew --prefix)"
cmake --build build --parallel
file build/turrican2.app/Contents/MacOS/turrican2
open build/turrican2.app
```

An original **extended ADF** for Turrican II is required; no copyrighted
game data is distributed in this repository.

### First launch: native disk import

On the first Finder launch, a standard macOS file-selection dialog asks for
your original `T2.adf`. It checks the **UAE--ADF** extended-ADF header, then
copies the file into the app-owned persistent location:

```text
~/Library/Application Support/Turrican II/T2.adf
```

On later launches the saved copy is loaded automatically. The app asks for
another file only if the copy is missing or invalid. Selecting *Cancel* leaves
the original file untouched and exits without starting the game.

**Why not write into `turrican2.app/Contents/Resources`?** The installed
`.app` bundle may be read-only and changing it after signing invalidates its
code signature. Application Support is the native macOS per-user app data
location and survives app updates. Data never enters Git or a release build.

For advanced command-line use, an explicitly passed ADF path bypasses the
first-launch importer. Game saves and states also live under
`~/Library/Application Support/Turrican II/` by default.

## Fullscreen and graphics

- The green macOS window button and **Control-Command-F** use a native macOS
  fullscreen Space (SDL2 fullscreen Spaces).
- **F11** and **Alt-Return** remain supported.
- High-DPI/Retina rendering is enabled.
- The **classic** framebuffer remains 320×256 with aspect-preserving scaling.
  Fullscreen on a widescreen display therefore shows side margins instead
  of distorting or cropping the original playfield.
- **Extended 16:9 gameplay is not implemented yet.** It requires camera,
  scrolling and playfield-renderer changes, not merely stretching the image.

## Controller roadmap

The current upstream SDL2 gamepad support is unchanged in this first step.
Planned follow-up work: configurable DualShock 4 bindings for all exposed
buttons; L2 to hold the surround beam, right stick for beam orientation
(with gameplay-specific validation); controller hotplug/disconnect; and an
in-game remapping menu. macOS may reserve the PS/Home button.

## Build artifact limitations

The GitHub Actions `turrican2-macos-arm64-dev` bundle is a **development
build** linked to Homebrew SDL2/SDL3 libraries. Install
`sdl2-compat` locally before running it. It is **not yet a portable,
signed or notarized release**. A standalone distributable bundle is a
later packaging step.

Gameplay is still experimental upstream, and the CI build does not
validate actual rendering or input without a user-provided ADF.
