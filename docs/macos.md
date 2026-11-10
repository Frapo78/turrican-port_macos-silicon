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

An original **extended ADF** for Turrican II is required. On launch through
Finder, the app asks you to locate the ADF when it cannot find `T2.adf`
in the current working directory, `assets/`, or the configured data dir.
You may also launch the executable from Terminal with an explicit ADF path.

Game saves and states default to
`~/Library/Application Support/Turrican II/` on macOS.

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
