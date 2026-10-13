#include "t2/game.hpp"

#include <algorithm>

namespace t2 {

namespace {
// Code addresses in MAIN (with the instruction words checked before use).
constexpr uint32_t LOOP_TOP = 0x529A;      // per-frame game loop: tst.b $139
constexpr uint32_t LOOP_TOP_ALT = 0x5304;  // loop variant while $139 < 0: bsr $5fb2
constexpr uint32_t LOAD_WORLD = 0x5230;    // bsr $630 (new game, next world)
constexpr uint32_t NEW_GAME = 0x51BC;      // clr.w $5ce2
// Game routines called by the host.
constexpr uint32_t ADD_LIFE = 0x74B4;
constexpr uint32_t DRAW_LIVES = 0x7514;
constexpr uint32_t REFILL_ENERGY = 0x75B2;
constexpr uint32_t DRAW_POWER_LINE = 0x7372;  // a0 = icon
constexpr uint32_t DRAW_BOMBS = 0x749C;
constexpr uint32_t DRAW_CONTINUE = 0x7388;    // d0 = 0 full / 1 empty, d1 = slot
constexpr uint32_t DRAW_TIME = 0x7598;
constexpr uint32_t ICON_POWER_LINE = 0x1E638, ICON_NO_POWER_LINE = 0x1E658;
// Return address of injected calls: never executed (unmapped), the hook
// there restores the registers.
constexpr uint32_t CALL_RETURN = 0xFFFFF0;

struct Sig { uint32_t addr; uint16_t w0, w1; };
constexpr Sig SIGS[] = {
    {LOOP_TOP, 0x4A38, 0x0139},
    {LOOP_TOP_ALT, 0x6100, 0x0CAC},
    {LOAD_WORLD, 0x6100, 0xB3FE},
    {NEW_GAME, 0x4278, 0x5CE2},
    {ADD_LIFE, 0x41F9, 0x0001},
    {REFILL_ENERGY, 0x41F9, 0x0001},
};
}  // namespace

const char* weapon_name(int w) {
  switch (w) {
    case 1: return "Multiple";
    case 2: return "Laser";
    case 3: return "Rebound";
    default: return "?";
  }
}

Game::Game(amiga::Machine& m) : m_(m) {
  m_.set_pc_hook(LOOP_TOP, [this] { frame_hook(); });
  m_.set_pc_hook(LOOP_TOP_ALT, [this] { frame_hook(); });
  m_.set_pc_hook(LOAD_WORLD, [this] { world_load_hook(); });
  m_.set_pc_hook(CALL_RETURN, [this] { return_hook(); });
}

bool Game::main_loaded() const {
  for (const Sig& s : SIGS)
    if (m_.r16(s.addr) != s.w0 || m_.r16(s.addr + 2) != s.w1) return false;
  return true;
}

bool Game::in_game() const {
  // The loop runs every frame; a gap means title, loading or ending.
  return last_frame_ && m_.frame_count() + 1 <= last_frame_ + 5 && main_loaded();
}

int Game::digits(uint32_t addr, int n) const {
  int v = 0;
  for (int i = 0; i < n; ++i) v = v * 10 + std::min<int>(m_.r8(addr + uint32_t(i)), 9);
  return v;
}

void Game::set_digits(uint32_t addr, int n, int value) {
  for (int i = n - 1; i >= 0; --i, value /= 10) m_.w8(addr + uint32_t(i), uint8_t(value % 10));
}

int Game::world() const { return m_.r16(var::WORLD); }
int Game::stage() const { return m_.r16(var::STAGE); }
int Game::lives() const { return digits(var::LIVES, 2); }
int Game::weapon() const { return m_.r8(var::WEAPON); }
int Game::weapon_level(int w) const {
  switch (w) {
    case 1: return m_.r16(var::LEVEL_MULTIPLE);
    case 2: return m_.r16(var::LEVEL_LASER);
    case 3: return m_.r16(var::LEVEL_REBOUND);
    default: return 0;
  }
}
bool Game::cheat_flag() const { return m_.r8(var::CHEAT) != 0; }

// ---- hooks -------------------------------------------------------------------

void Game::frame_hook() {
  if (in_call_ || !main_loaded()) return;
  if (calls_.empty()) {
    // Once per frame (the hook also runs again after each injected call).
    if (last_frame_ != m_.frame_count() + 1) {
      last_frame_ = m_.frame_count() + 1;
      per_frame_toggles();
    }
    while (!actions_.empty()) {
      Action a = std::move(actions_.front());
      actions_.pop_front();
      a();
    }
  }
  start_next_call();
}

bool Game::start_next_call() {
  if (calls_.empty()) return false;
  Call c = calls_.front();
  calls_.pop_front();
  auto& cpu = m_.cpu;
  saved_ = cpu.save();
  in_call_ = true;
  cpu.d[0] = c.d0;
  cpu.d[1] = c.d1;
  cpu.a[0] = c.a0;
  cpu.push32(CALL_RETURN);
  cpu.pc = c.addr;
  return true;
}

void Game::return_hook() {
  if (!in_call_) return;
  in_call_ = false;
  m_.cpu.restore(saved_);  // back to the loop top: the frame hook continues
}

void Game::world_load_hook() {
  if (warp_world_ < 0 || !main_loaded()) return;
  m_.w16(var::WORLD, uint16_t(warp_world_));
  m_.w16(var::STAGE, uint16_t(warp_stage_));
  warp_world_ = warp_stage_ = -1;
}

void Game::per_frame_toggles() {
  if (infinite_lives && lives() < 3) {
    set_digits(var::LIVES, 2, 3);
    call(DRAW_LIVES);
  }
  // Refill once the bar has finished animating ($15A) after a hit.
  if (infinite_energy && m_.r8(var::ENERGY) < 55 && !m_.r8(0x15A)) call(REFILL_ENERGY);
  if (god_mode) m_.w16(var::INVULNERABLE, 0xFFFF);
}

// ---- actions ------------------------------------------------------------------

void Game::add_life() { queue([this] { call(ADD_LIFE); }); }

void Game::set_lives(int n) {
  queue([this, n] {
    set_digits(var::LIVES, 2, std::clamp(n, 0, 99));
    call(DRAW_LIVES);
  });
}

void Game::full_energy() { queue([this] { call(REFILL_ENERGY); }); }

void Game::refill_power_line() {
  queue([this] {
    m_.w8(var::POWER_LINE, 1);
    // As at $577A: the "used" icon is shown in wheel mode ($FC < 0).
    call(DRAW_POWER_LINE, 0, 0, (m_.r8(0xFC) & 0x80) ? ICON_NO_POWER_LINE : ICON_POWER_LINE);
  });
}

void Game::set_bombs(int n) {
  queue([this, n] {
    set_digits(var::BOMBS, 2, std::clamp(n, 0, 99));
    call(DRAW_BOMBS);
  });
}

void Game::set_continues(int n) {
  queue([this, n] {
    int c = std::clamp(n, 0, 5);
    m_.w8(var::CONTINUES, uint8_t(c));
    for (int i = 0; i < 5; ++i) call(DRAW_CONTINUE, i < c ? 0 : 1, uint32_t(i));
  });
}

void Game::set_time(int n) {
  queue([this, n] {
    set_digits(var::TIME, 3, std::clamp(n, 0, 999));
    call(DRAW_TIME);
  });
}

void Game::select_weapon(int w, int level) {
  if (w < 1 || w > 3) return;
  queue([this, w, level] {
    static const uint32_t addr[4] = {0, var::LEVEL_MULTIPLE, var::LEVEL_LASER, var::LEVEL_REBOUND};
    m_.w16(addr[w], uint16_t(std::clamp(level, 0, MAX_LEVEL[w])));
    m_.w8(var::WEAPON, uint8_t(w));
  });
}

void Game::max_weapons() {
  queue([this] {
    m_.w16(var::LEVEL_MULTIPLE, uint16_t(MAX_LEVEL[1]));
    m_.w16(var::LEVEL_LASER, uint16_t(MAX_LEVEL[2]));
    m_.w16(var::LEVEL_REBOUND, uint16_t(MAX_LEVEL[3]));
    m_.w16(var::BEAM, 8);
  });
}

void Game::warp(int world, int stage) {
  if (world < 0 || world >= NUM_WORLDS) {
    warp_world_ = warp_stage_ = -1;
    return;
  }
  warp_world_ = world;
  warp_stage_ = std::clamp(stage, 0, STAGES[world] - 1);
  if (!in_game()) return;  // applied when the next game loads its world
  // In a game: finish the current stage as the last one of a world that is
  // not the last, so the game advances to the next world and reloads at
  // LOAD_WORLD, where the hook substitutes the target.
  queue([this] {
    int count = m_.r16(var::LEVEL_FILE + 0x14);
    m_.w16(var::WORLD, 0);
    m_.w16(var::STAGE, uint16_t(std::max(count, 1) - 1));
    m_.w8(var::STAGE_DONE, 0xFF);
  });
}

void Game::set_cheat_flag(bool on) { queue([this, on] { m_.w8(var::CHEAT, on ? 0xFF : 0); }); }

}  // namespace t2
