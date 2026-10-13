# Turrican II Disk Format

The game ships as an old-style extended ADF ("UAE--ADF") image. The
runtime and `tools/t2disk.py` both understand this format.

## Image layout

```
offset 0:   "UAE--ADF"  (8 bytes)
offset 8:   160 × {u16 sync, u16 length}   (640 bytes)
offset 648: track data, concatenated
```

- Tracks 0–1: AmigaDOS (sync = 0). Track 0 holds the bootblock; track 1
  is read later by MAIN with sync `$4489`.
- Tracks 2–159: custom MFM tracks. After the sync mark `$9521` the image
  stores 13 640 bytes per track (the final track in the supplied image is
  short by 248 bytes and is unused).

## Custom track encoding

Each custom track (after the sync) is:

| Offset | Size     | Content                                      |
|--------|----------|----------------------------------------------|
| 0      | 2 bytes  | pad word                                     |
| 2      | 13 600 B | 1700 pairs of (odd, even) longs → 6 800 data |
| 13 602 | 8 bytes  | checksum pair (odd, even)                    |

Decoding: for each pair `(odd, even)` the data long is
`(odd & 0x55555555) | ((even & 0x55555555) << 1)`.
The checksum is the EOR of all raw data longs, masked with `$55555555`.

The loader issues a DMA of 6 820 words (`DSKLEN = $9AA4`) with sync
`$9521` and `ADKCON = $1500`.

## Directory

The first 400 bytes of track 2 contain 25 directory entries:

```
struct DirEnt {
  char   name[4];   // e.g. "MAIN", "L1-1"
  uint16_t track;   // starting track
  uint16_t offset;  // byte offset inside that track's data
  uint32_t length;  // uncompressed length in bytes
  uint32_t flags;   // bit 0 = PP20 compressed, other bits loader-specific
};
```

Files occupy contiguous ranges of the 6 800-byte track payloads and may
span multiple tracks. When the high bit of the flags (or the loader
request) indicates packing, the payload is a PowerPacker "PP20" stream
that the stage-2 loader decompresses on the fly.

## Boot sequence

1. **Bootblock** (track 0). Allocates all available Fast then Chip memory
   into a chunk list, reads disk offset `$400` for `$800` bytes into
   `$60000`, and jumps there via `trap #0`.

2. **Stage 2** (`$60000`). Kills the OS, installs a minimal trackloader,
   loads the Rainbow Arts intro (RA-I) to `$30000` and runs it at
   `$3010A`, then loads the main game code (MAIN, PP20) to `$C0` and
   enters it in supervisor mode.

3. **MAIN**. Among other things, DMA-reads track 1 (sync `$4489`,
   768 words). The runtime synthesises valid AmigaDOS MFM for sector
   tracks via `Disk::encode_dos_track`.

## File list (from the reference image)

See `tools/t2disk.py list T2.adf`. Notable entries:

| Name | Approx. size | Notes                          |
|------|--------------|--------------------------------|
| RA-I | 16 KB        | Rainbow Arts intro (PP20)      |
| MAIN | 57 KB        | Main game code (PP20)          |
| L1-1 … L4-1 | varying | Level data                     |
| MUS0 … MUS4 | varying | Music                          |
| IDAT, IPxx  |           | Intro / title assets           |

The last custom track (159) has a checksum error in the reference image
and is not used by the game.
