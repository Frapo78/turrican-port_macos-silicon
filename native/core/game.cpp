// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/game.hpp"
#include <algorithm>
#include <cmath>

namespace t2native {
Game::Game() {
  // Entirely synthetic geometry. No original game's artwork, maps or code.
  world_.platforms = {{0, 484, 2700, 56}, {320, 395, 190, 22},
                      {680, 355, 235, 22}, {1160, 410, 240, 22},
                      {1540, 340, 220, 22}, {2000, 395, 245, 22}};
}
void Game::set_widescreen(bool on) {
  widescreen_ = on;
  view_width_ = on ? 960.f : 720.f;
  camera_x_ = std::clamp(player_.pos.x - view_width_ * 0.35f,
                         0.f, world_.width - view_width_);
}
void Game::physics_step(const Input& in, float dt) {
  // Placeholder physics: deterministic fixed-step simulation, no 68000 CPU.
  constexpr float speed = 230.f, gravity = 1350.f, jump_speed = -510.f;
  const float direction = std::clamp(in.move, -1.f, 1.f);
  player_.vel.x = direction * speed;
  if (std::fabs(direction) > .15f) player_.facing_right = direction > 0;
  if (in.jump && !jump_held_ && player_.grounded) {
    player_.vel.y = jump_speed;
    player_.grounded = false;
  }
  jump_held_ = in.jump;
  const float bottom = player_.pos.y + 52.f;
  player_.vel.y = std::min(player_.vel.y + gravity * dt, 900.f);
  player_.pos.x = std::clamp(player_.pos.x + player_.vel.x * dt,
                             0.f, world_.width - 32.f);
  const float next_bottom = bottom + player_.vel.y * dt;
  float landing_top = world_.height + 100.f;
  if (player_.vel.y >= 0) {
    for (const auto& r : world_.platforms) {
      if (player_.pos.x + 28.f > r.x && player_.pos.x + 4.f < r.x + r.w &&
          bottom <= r.y + 3.f && next_bottom >= r.y) {
        landing_top = std::min(landing_top, r.y);
      }
    }
  }
  if (landing_top <= world_.height) {
    player_.pos.y = landing_top - 52.f;
    player_.vel.y = 0;
    player_.grounded = true;
  } else {
    player_.pos.y += player_.vel.y * dt;
    player_.grounded = false;
  }
  beam_active_ = in.beam;
  if (beam_active_) {
    if (std::fabs(in.aim.x) + std::fabs(in.aim.y) > .3f)
      player_.beam_angle = std::atan2(in.aim.y, in.aim.x);
    else
      player_.beam_angle = player_.facing_right ? 0.f : 3.14159265f;
  }
  camera_x_ = std::clamp(player_.pos.x - view_width_ * .35f,
                         0.f, world_.width - view_width_);
}
void Game::tick(const Input& input, float dt) {
  // Clamp to avoid huge steps after window drag / sleep. App drives 60 Hz.
  if (dt <= 0) return;
  physics_step(input, std::min(dt, 1.f / 30.f));
  ++ticks_;
}
}  // namespace t2native
