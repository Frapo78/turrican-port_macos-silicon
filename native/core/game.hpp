// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <vector>

namespace t2native {
struct Vec2 { float x = 0, y = 0; };
struct Rect { float x, y, w, h; };
struct Input {
  float move = 0;
  bool jump = false;
  bool fire = false;
  bool beam = false;
  Vec2 aim{0, 0};
};
struct Player {
  Vec2 pos{48, 370};
  Vec2 vel{};
  bool grounded = false;
  bool facing_right = true;
  float beam_angle = 0;
  uint32_t fire_held_frames = 0;
};
struct World {
  float width = 2700;
  float height = 540;
  std::vector<Rect> platforms;
};
class Game {
 public:
  Game();
  void tick(const Input& input, float dt);
  const Player& player() const { return player_; }
  const World& world() const { return world_; }
  float camera_x() const { return camera_x_; }
  float view_width() const { return view_width_; }
  float view_height() const { return 540.f; }
  float beam_length() const { return 175.f; }
  bool beam_active() const { return beam_active_; }
  // The 68000 reference keeps a held-fire counter ($DE) and switches to the
  // surround/beam weapon after a hold. The exact frame threshold is a native
  // calibration point; keeping it here makes that behavior independently
  // testable without an emulator dependency.
  static constexpr uint32_t kSurroundHoldFrames = 30;
  bool surround_active() const { return player_.fire_held_frames >= kSurroundHoldFrames; }
  void set_widescreen(bool enable);
  bool widescreen() const { return widescreen_; }
  uint64_t ticks() const { return ticks_; }
 private:
  void physics_step(const Input& input, float dt);
  World world_;
  Player player_;
  float camera_x_ = 0;
  float view_width_ = 960;
  bool widescreen_ = true;
  bool beam_active_ = false;
  bool jump_held_ = false;
  uint64_t ticks_ = 0;
};
}  // namespace t2native
