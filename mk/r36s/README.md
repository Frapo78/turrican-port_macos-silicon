<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Turrican II for the R36S (ArkOS) and other PortMaster handhelds

Port of the Amiga version of Turrican II (Factor 5 / Rainbow Arts, 1991).
Native aarch64 build, linked against an ArkOS-era (Ubuntu 19.10) userland;
it uses the device's SDL2.

## Install

Copy `Turrican2.sh` and the `turrican2/` folder to `/roms/ports/`, or put
the zip into `ports/PortMaster/autoinstall/` and start PortMaster once.

## Disk image

The game disk is not included and may not be redistributed. Copy your own
Amiga disk image (extended ADF) into `ports/turrican2/` as `T2.adf`; any
other `*.adf` in that folder is used too.

## Controls

| Button       | Action                    |
|--------------|---------------------------|
| D-pad, stick | move                      |
| A            | jump                      |
| Y            | fire                      |
| X            | auto-fire                 |
| B            | Space (powerline)         |
| Select       | Esc                       |
| L1 / R1      | F1 / F2                   |
| Start        | debug menu                |
| Select+Start | quit                      |

The written disk and save states are kept in `turrican2/conf/`.
