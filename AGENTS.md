# Turrican II Port

## Goal
Port the Amiga version of Turrican II (Factor 5, 1991) to modern C++ with
SDL2. Source material: `T2.adf` (in git, not redistributable).
Route: (1) run the original code on an emulated A500 runtime (done, from
~/projects/hostages/trunk), (2) static recompilation of the 68000 code
(tools/recomp.py from ~/projects/fa18-interceptor/trunk, to be adapted),
(3) native reimplementation with new features (gamepad support etc.).

## Disk format (tools/t2disk.py, docs to be written in docs/)
- Old-style extended ADF "UAE--ADF": 160 × {u16 sync, u16 len}, then the
  track data. Tracks 0-1 AmigaDOS (sync 0), tracks 2-159 raw MFM after
  sync $9521, 13640 bytes each (the last image track is cut short by 248
  bytes; unused).
- Custom track: 1 pad word, 1700 (odd, even) long pairs = 6800 bytes,
  then a checksum pair (EOR of the raw longs & $55555555).
- Directory: first 400 bytes of track 2, 25 × {name[4], u16 track,
  u16 offset, u32 length, u32 flags}. Files are contiguous across tracks;
  PP20 (PowerPacker) when the loader is asked to unpack.
- Bootblock: AvailMem/AllocMem all fast then chip memory into a chunk
  list (d0), reads disk $400+$800 to $60000, jumps there via trap #0.
- Stage 2 ($60000): kills the OS, trackloader (DSKLEN $9AA4 = 6820 words,
  sync $9521), loads RA-I (Rainbow Arts intro) to $30000 and runs $3010A,
  then MAIN (unpacked) to $C0 and jumps to $C0 in supervisor mode.
- MAIN also DMA-reads track 1 with sync $4489 (768 words); the runtime
  synthesizes AmigaDOS MFM for sector tracks (Disk::encode_dos_track).

## Known facts
- The game runs with DMACON = $07FF (blitter-nasty) and writes the next
  blit's registers right after BLTSIZE; the CPU is therefore stalled for
  the duration of each blit (Machine::blit).
- The display is produced per scanline, interleaved with the CPU
  (Machine::begin_line); the game double-buffers its copper list (COP1LC
  written every frame) and splits the title screen with COP2LC/COPJMP2.
- Title animation and level 1 use dual playfield, 6 planes.

## Recompiler design (next step)
Code arrives at run time at fixed addresses (stage 2 $60000, RA-I $30000,
MAIN $C0, maybe code in level files) and code is interleaved with
variables, so whole-image checksums do not work. Plan:
- build time: recomp.py takes the files decoded by t2disk.py with their
  load addresses plus coverage seeds (`--coverage`) from interpreter runs;
- runtime: a per-byte "is compiled code" mask; RAM writes (CPU, blitter,
  disk DMA) hitting a code byte mark its page dirty and force generated
  code to exit (deadline in Cpu instead of a local); the dispatcher
  re-verifies dirty pages against the expected bytes before entering a
  chunk, else interprets.
- `Bus::ram_written` is declared in cpu.hpp but never called yet.

## Tools
- `tools/t2disk.py list|extract|track ADF ...` — custom disk format, PP20
- `tools/disasm.py` — disassembly (m68k objdump from the nix shell)
- `tests/run_scenarios.sh` — scripted headless runs (scenarios/ empty)
- Script lines: `<frame> key <name|hex> [down|up|tap]`, `joy udlrf`,
  `shot file.ppm`, `poke addr val [b|w|l]`, `quit` (frame may be `+N`)
- Debug env: T2_BLITLOG, T2_AUDLOG=frame, T2_COPDUMP=frame (COP1+COP2
  lists), T2_WATCHB=addr, T2_WATCHPC=addr, T2_SPRDUMP
- Headless: `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./build/turrican2
  --fast --frames N --script s.txt T2.adf`
- FS-UAE is installed for reference comparisons; Kickstart ROMs are in
  ~/projects/fa18-interceptor/bios.

## Workflow
- See TODO.md. Commit directly (small, descriptive commits).
