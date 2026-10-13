# MAIN: game flow and memory map

MAIN is loaded (PP20-unpacked) to `$C0` by stage 2 and entered there
(`bra $1246`). All addresses are absolute; MAIN ends at `$205A8`. World
files are loaded to `$20700`. Found by disassembly (`tools/disasm.py
T2.adf MAIN --coverage FILE`) and checked at runtime (RAM dumps with the
`dump` script command, `T2_WATCHB`/`T2_WATCHPC`). `src/t2/game.cpp` uses
this map; it checks the instruction words at its hook addresses first.

## Game flow

| Address | What |
|---------|------|
| `$1246` | entry: d0 = memory chunk list (`$1562`), d1 = track (`$62E`); `$1566` = memory in 512 KB units. Loads `INTR` + `MUS0` (title program) to `$20700` and runs it, then `bra $51BC` |
| `$51BC` | **new game**: world `$5CE2` = 0, stage `$5CE4` = 0, lives = 3, … |
| `$5230` | `bsr $630`: load the world (new game and next world) |
| `$630`  | load world `$5CE2`: names `L1-1/MUS1 … L5-1/MUS5, INTR/MUS0` (table `$6C8`); world file to `$20700`, music to `($2070C)` |
| `$6F8`  | load file by name (d0 = name, d1 = address, d2 = flags); directory copy (30 entries) at `$802` |
| `$56BE` | start stage → game loop |
| `$529A` | **per-frame game loop** (`$5304`: variant while `$139` < 0) |
| `$52F2` | `$1D6` set → `$5692` stage complete: stage + 1; if ≥ stage count (world file +`$14`) → world + 1 (`$571A`), after world 5 the ending (`$138E`), else `$5220` → `$5230` |
| `$5360` | life lost: lives = 0 → game over / continue (`$5406`), else `$74E6` lose a life, restart stage |
| `$573E` | new life: every weapon level − 1, bombs = 3, power line = 1 |
| `$12D0` | runs an INTR entry (d7) with a stage name from `$1352` (`1-1 1-2 / 2-1 2-2 / 3-1 3-2 3-3 / 4-1 4-2 / 5-1 5-2 / END`) |

Stages per world: 2, 2, 3, 2, 2.

## World file (at `$20700`)

| Offset | What |
|--------|------|
| `+$0C` | music address (`MUSx` is loaded there) |
| `+$14` | word: number of stages |
| `+$16` | stage descriptor pointers (long, per stage) |

Stage descriptor (52 bytes): `+$00` map, `+$08` player start (x, y),
`+$14` init routine (e.g. world 1 stage 2: `$2165E` loads `L1-2` to
`$43894`), `+$18` per-frame routine (→ `$198`; world 1 stage 1 `$215B2`
sets `$1D6` at the exit), `+$1C` time limit (3 decimal digits), `+$2C` music
subsong (→ `$5CE6`).

## Player state

Counters shown in the HUD are stored as decimal digits, one per byte.

| Address | Size | What |
|---------|------|------|
| `$15C`  | 2 digits | lives (`$74B4` +1 capped at 99, `$74E6` −1, `$7514` draw) |
| `$15E`  | 2 digits | diamonds (`$73E8` +1; at 100 → `$73BC` continues + 1) |
| `$160`  | byte | continues 0–5 (HUD dots; `$7388` d0 = 0 full / 1 empty, d1 = slot) |
| `$1C1`  | byte | power line available (Space / 2nd button, `$2D3A`); icon `$7372` with a0 = `$1E638` / `$1E658` |
| `$1D6`  | byte | stage complete (set by the level code) |
| `$1D7`  | byte | built-in cheat: no life/bomb loss, no score (`$74E6`, `$7462`, `$7704`) |
| `$EC`   | word | > 0: invulnerability countdown (pickup 5: 520); < 0: no damage and the timer stops |
| `$3CE4` | byte | weapon: 1 Multiple (`$3CE6`), 2 Laser (`$4E28`), 3 Rebound (`$4836`) |
| `$3D98` | word | Multiple level 0–2 |
| `$4F7A` | word | Laser level 0–4 |
| `$4ABC` | word | Rebound level 0–3 |
| `$4716` | word | 0–8, pickup 3 (used by the hold-fire beam at `$46EC`) |
| `$786B` | byte | energy 0–55 (`$76AE` damage d1, `$75B2` refill with bar animation `$15A`/`$15B`) |
| `$786C` | 3 digits | time (`$7542` −1 every 50 frames, `$7598` draw) |
| `$7870` | 2 digits | wheel-mode bombs (`$742A` +1, `$7462` −1, `$749C` draw; 3 per life) |
| `$7814` | 7 digits | score (`$7704` add; HUD shows a trailing 0) |
| `$5CE2` | word | world 0–4 |
| `$5CE4` | word | stage in the world |

Pickups (`$7D80`, d2 = type): 0 Multiple, 1 Laser, 2 Rebound (level + 1,
selects the weapon), 3 `$4716` + 1, 4 `$1AC`/`$1BE` (unknown),
5 invulnerability, 6 full energy, 7 bomb, 8 extra life.

## Input

| Address | What |
|---------|------|
| `$32FA` | read joystick (JOY1DAT via table `$338A`, fire `$BFE001` bit 7, 2nd button POTGOR bit 14) |
| `$DA`/`$DB`/`$DE` | fire pressed (edge) / held / hold counter; `$DC` 2nd button |
| `$249`  | last key (raw): Esc `$45` gives up (lives = 0, `$5B6C`), P `$19` / `$63` pause (`$5B18`), S `$21` music on/off (`$5B88`) |

## Other

- `$1F000`–: sound driver (`jsr $1F04C` d0 = sound effect, `$1F01C` music).
- HUD bitmap around `$1D400`–`$1E700`.
