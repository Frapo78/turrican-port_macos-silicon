// Turrican II game knowledge on top of the emulated machine: the memory
// map of MAIN (see docs/memory_map.md) and host-side actions built on it
// (cheats, weapon and level select for the debug menu).
//
// Actions that need the game's own routines (HUD redraws, refilling the
// energy bar) are run at the top of the game's per-frame loop: the host
// saves the registers, calls the routine with a return address that traps
// back to the host, and restores the registers afterwards. Level select
// overrides world/stage where a new game (or the next world) loads its
// world file.
#pragma once

#include "amiga/machine.hpp"

#include <cstdint>
#include <deque>
#include <functional>
#include <string>

namespace t2 {

// MAIN (loaded at $C0) variables.
namespace var {
constexpr uint32_t LIVES = 0x15C;        // 2 bytes: tens, ones (decimal digits)
constexpr uint32_t DIAMONDS = 0x15E;     // 2 bytes: tens, ones; every 100 → +1 continue
constexpr uint32_t CONTINUES = 0x160;    // byte, 0..5 (HUD dots)
constexpr uint32_t POWER_LINE = 0x1C1;   // byte, 1 per life (Space / 2nd button)
constexpr uint32_t CHEAT = 0x1D7;        // byte, built-in cheat: no life/bomb loss, no score
constexpr uint32_t STAGE_DONE = 0x1D6;   // byte, set by the level code at the stage exit
constexpr uint32_t INVULNERABLE = 0xEC;  // word: >0 countdown, <0 frozen (no damage, no timer)
constexpr uint32_t WEAPON = 0x3CE4;      // byte: 1 Multiple, 2 Laser, 3 Rebound
constexpr uint32_t LEVEL_MULTIPLE = 0x3D98;  // word, 0..2
constexpr uint32_t LEVEL_LASER = 0x4F7A;     // word, 0..4
constexpr uint32_t LEVEL_REBOUND = 0x4ABC;   // word, 0..3
constexpr uint32_t BEAM = 0x4716;        // word, 0..8 (pickup 3; lightning beam)
constexpr uint32_t ENERGY = 0x786B;      // byte, 0..55
constexpr uint32_t TIME = 0x786C;        // 3 bytes: decimal digits
constexpr uint32_t BOMBS = 0x7870;       // 2 bytes: tens, ones (wheel mode, 3 per life)
constexpr uint32_t SCORE = 0x7814;       // 7 bytes: decimal digits
constexpr uint32_t WORLD = 0x5CE2;       // word, 0..4
constexpr uint32_t STAGE = 0x5CE4;       // word, stage within the world
constexpr uint32_t LEVEL_FILE = 0x20700; // world file (header +$14: stage count)
}  // namespace var

constexpr int NUM_WORLDS = 5;
// Stages per world, as in MAIN's stage name table at $1352.
constexpr int STAGES[NUM_WORLDS] = {2, 2, 3, 2, 2};
constexpr int MAX_LEVEL[4] = {0, 2, 4, 3};  // per weapon 1..3
const char* weapon_name(int w);

class Game {
public:
  explicit Game(amiga::Machine& m);

  // True while MAIN is in memory and the game loop is running (not the
  // title/intro program).
  bool in_game() const;
  bool main_loaded() const;

  // ---- actions (queued, run at the next game frame) ----------------------
  void add_life();
  void set_lives(int n);              // 0..99
  void full_energy();
  void refill_power_line();
  void set_bombs(int n);              // 0..99
  void set_continues(int n);          // 0..5
  void set_time(int n);               // 0..999
  void select_weapon(int w, int level);  // w 1..3, level clamped
  void max_weapons();
  // Start the next new game at world/stage (0-based); or, in a game, warp
  // there now (through the game's own stage-complete path).
  void warp(int world, int stage);

  // ---- toggles (applied every game frame) --------------------------------
  bool infinite_lives = false;
  bool infinite_energy = false;
  bool god_mode = false;              // also stops the timer
  void set_cheat_flag(bool on);
  bool cheat_flag() const;

  // Pending level select for the next world load, -1 if none.
  int pending_world() const { return warp_world_; }
  int pending_stage() const { return warp_stage_; }

  int world() const;
  int stage() const;
  int lives() const;
  int weapon() const;
  int weapon_level(int w) const;

private:
  using Action = std::function<void()>;
  struct Call { uint32_t addr; uint32_t d0, d1, a0; };

  void frame_hook();       // top of the game's per-frame loop
  void world_load_hook();  // new game / next world: about to load the world
  void return_hook();      // an injected call returned
  void queue(Action a) { actions_.push_back(std::move(a)); }
  // Call a game routine from the frame hook (registers are preserved).
  void call(uint32_t addr, uint32_t d0 = 0, uint32_t d1 = 0, uint32_t a0 = 0) {
    calls_.push_back({addr, d0, d1, a0});
  }
  bool start_next_call();
  void per_frame_toggles();

  int digits(uint32_t addr, int n) const;
  void set_digits(uint32_t addr, int n, int value);

  amiga::Machine& m_;
  std::deque<Action> actions_;
  std::deque<Call> calls_;
  bool in_call_ = false;
  m68k::Cpu::Regs saved_{};
  uint64_t last_frame_ = 0;  // frame of the last game loop iteration + 1
  int warp_world_ = -1, warp_stage_ = -1;
};

}  // namespace t2
