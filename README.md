# Turrican II – Amiga port to C++/SDL2

Port of the Amiga version of **Turrican II: The Final Fight** (Factor 5 /
Rainbow Arts, 1991) to C++ and SDL2.

The game's original 68000 code runs on a small runtime that provides what
the code expects from an Amiga 500 (no Kickstart ROM needed): the parts of
exec/trackdisk.device the bootblock and intro use, emulated at library
level, and the custom chips the game programs directly (copper and
display, sprites, blitter, Paula, disk DMA, CIAs). The game boots straight
from the original disk image.

Plan: static recompilation of the 68000 code first (as in the FA-18
Interceptor port), then a native reimplementation with new features
(gamepad support, …).

## Requirements

The original disk image `T2.adf` (extended ADF, not included).

## Build

```bash
nix develop          # or: install cmake, a C++20 compiler and SDL2
cmake -B build
cmake --build build
./build/turrican2 T2.adf
```

## Controls

| Input | Function |
|-------|----------|
| Cursor keys / left stick / D-pad | joystick in port 2 (move / jump / duck) |
| Right Ctrl / gamepad **Y** | fire (hold = surround shot) |
| Gamepad **A** | jump (joystick up) |
| Gamepad **X** | auto-fire (pulsed fire taps) |
| Gamepad **B** / Space | powerline (secondary); down+B = gyroscope |
| Gamepad **Back** / Esc | quit to intro |
| Gamepad **Start** | open host debug menu (D-pad navigate, A ok, B cancel) |
| Ctrl | pause (fire to resume) |
| F11 / Alt-Enter | fullscreen |
| Pause | fast forward |

## License

GPLv3+ for the new code. The game itself is not included and not
redistributable; this is a reverse-engineering / preservation project.
