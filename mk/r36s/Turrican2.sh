#!/bin/bash
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Turrican II: PortMaster launcher for the R36S / ArkOS (and alike).

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi
source "$controlfolder/control.txt"
[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"
get_controls

GAMEDIR="/$directory/ports/turrican2"
cd "$GAMEDIR" || exit 1
> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

# The disk image is not redistributable: the user copies it into this
# folder. T2.adf is preferred, then any other *.adf.
adf=
for f in T2.adf *.adf *.ADF; do
  if [ -f "$f" ]; then adf="$f"; break; fi
done
if [ -z "$adf" ]; then
  echo "No Turrican II disk image found: copy T2.adf into $GAMEDIR"
  sleep 5
  exit 1
fi

# The written disk and save states stay with the port (not under ~/.local — SD card only).
mkdir -p "$GAMEDIR/conf"
# The pad's layout from PortMaster, for SDL's game controller API.
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

# gptokeyb: Select+Start quits.
$GPTOKEYB "turrican2" &
pm_platform_helper "$GAMEDIR/turrican2"
./turrican2 --scale 2 --save "$GAMEDIR/conf" "$adf"
pm_finish
