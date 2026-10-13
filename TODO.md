# TODO

## Works
- Boot from T2.adf: bootblock, stage-2 trackloader, Rainbow Arts intro,
  Turrican logo, title screen with music and the attract animation
- Fire on the title → loading screen → level 1-1 playable display (HUD,
  backdrop, player); timer runs

## Open
- [ ] Static recompilation (see AGENTS.md "Recompiler design"): port
      tools/recomp.py + src/recomp from the FA-18 port, code-byte write
      tracking, per-page validation in the dispatcher
- [ ] Scenarios in tests/scenarios (title, level 1 start, play)
- [ ] Verify gameplay input (move/jump/fire) and later levels/disk loads
- [ ] First ~5 s of audio (RA intro) peak at a constant level: check for
      DC / a stuck channel
- [ ] Compare against FS-UAE: display window (game uses DIW $2C91, left
      edge), sprite priorities, timing of loading screens
- [x] docs/disk_format.md
- [x] docs/memory_map.md: game flow, world files, player state, input
- [x] Debug menu: level select/warp, weapon select, lives/energy/bombs/
      continues/time, infinite lives/energy, god mode (src/t2/game.cpp)
- [ ] Unknown: pickup 4 (`$1AC`, `$1BE` = 2), `$4716` (beam?), `$139`
      (alternate game loop, e.g. the ship stages), INTR entry points
- [ ] Infinite lives restores the count only when play resumes after the
      death sequence; consider skipping the decrement instead
- [ ] Native reimplementation; gamepad support
