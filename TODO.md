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
- [ ] docs/disk_format.md, docs on memory map of MAIN
- [ ] Native reimplementation; gamepad support
