// SPDX-License-Identifier: GPL-3.0-or-later
// Native C++20 prototype. This target must never link to the amiga/m68k libs.
#include "core/game.hpp"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef __APPLE__
bool native_macos_onboard();
#endif

using t2native::Game;
using t2native::Input;
namespace {
void box(SDL_Renderer* r, float x, float y, float w, float h,
         Uint8 red, Uint8 green, Uint8 blue) {
  SDL_SetRenderDrawColor(r, red, green, blue, 255);
  SDL_Rect rect{int(x), int(y), int(w), int(h)};
  SDL_RenderFillRect(r, &rect);
}
void draw(SDL_Renderer* renderer, const Game& game, bool pressed_fire) {
  const float cam = game.camera_x(), view = game.view_width();
  SDL_SetRenderDrawColor(renderer, 7, 13, 29, 255);
  SDL_RenderClear(renderer);
  // Synthetic scrolling stars and scenery: no extracted / redistributed assets.
  for (int i = 0; i < 70; ++i) {
    const float world_x = float((i * 179 + 79) % 3000);
    float x = world_x - cam * .25f;
    x = std::fmod(x + 3000.f, 3000.f);
    if (x < view) box(renderer, x, float((i * 97 + 37) % 310), 3, 3,
                      80, 135, 195);
  }
  for (int i = 0; i < 24; ++i) {
    const float x = i * 170.f - cam * .45f;
    box(renderer, x, 290.f + (i % 4) * 11.f, 125, 280, 20, 35, 65);
  }
  for (auto& platform : game.world().platforms) {
    const float sx = platform.x - cam;
    if (sx > view || sx + platform.w < 0) continue;
    box(renderer, sx, platform.y, platform.w, platform.h, 42, 90, 101);
    box(renderer, sx, platform.y, platform.w, 5, 105, 190, 180);
  }
  const auto& p = game.player();
  const float px = p.pos.x - cam;
  box(renderer, px, p.pos.y, 28, 52, 232, 187, 78);
  box(renderer, px + (p.facing_right ? 20 : 0), p.pos.y + 13,
      8, 9, 30, 46, 69);
  if (game.beam_active()) {
    float x = px + 14.f, y = p.pos.y + 19.f;
    const float angle = p.beam_angle;
    SDL_SetRenderDrawColor(renderer, 130, 245, 255, 255);
    for (int k = -1; k <= 1; ++k) {
      SDL_RenderDrawLine(renderer, int(x), int(y + k),
                         int(x + std::cos(angle) * game.beam_length()),
                         int(y + std::sin(angle) * game.beam_length() + k));
    }
  }
  if (pressed_fire) box(renderer, px + (p.facing_right ? 30 : -12),
                        p.pos.y + 18, 12, 8, 250, 230, 150);
  // Status bars / overlay are invented placeholders, not a recreation of HUD.
  box(renderer, 15, 15, 180, 12, 34, 50, 70);
  box(renderer, 17, 17, 140, 8, 142, 224, 150);
  box(renderer, 15, 37, game.widescreen() ? 110 : 78, 5, 94, 135, 215);
  SDL_RenderPresent(renderer);
}
SDL_GameController* first_controller() {
  for (int i = 0; i < SDL_NumJoysticks(); ++i)
    if (SDL_IsGameController(i)) return SDL_GameControllerOpen(i);
  return nullptr;
}
}  // namespace
int main(int argc, char** argv) {
  bool demo = false;
  int frames = -1;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--demo") demo = true;
    else if (a == "--frames" && i+1 < argc) frames = std::atoi(argv[++i]);
    else if (a == "--help") {
      std::puts("turrican2_native [--demo] [--frames N]\n"
                "  --demo skips the one-time ADF identifier for testing\n"
                "  F9 classic/16:9, Ctrl-Command-F fullscreen (macOS).\n"
                "  PS4: left stick move, Cross jump, L2 beam, right stick aim.\n"
                "  Native synthetic prototype only: original game not ported yet.");
      return 0;
    }
  }
#ifdef __APPLE__
  if (!demo && !native_macos_onboard()) return 1;
#endif
  SDL_SetHint(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, "1");
#ifdef __APPLE__
  // SDL2 uses its Metal backend where supported; software fallback remains.
  SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");
#endif
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Window* win = SDL_CreateWindow("Turrican II Native - placeholder prototype",
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720,
      SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  if (!win) { std::fprintf(stderr, "Window: %s\n", SDL_GetError()); SDL_Quit(); return 1; }
  SDL_Renderer* r = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!r) r = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
  if (!r) { std::fprintf(stderr, "Renderer: %s\n", SDL_GetError()); SDL_DestroyWindow(win); SDL_Quit(); return 1; }
  Game game;
  SDL_RenderSetLogicalSize(r, 960, 540);
  SDL_GameController* pad = first_controller();
  bool running = true;
  int shown = 0;
  Uint64 last = SDL_GetPerformanceCounter();
  const double frequency = double(SDL_GetPerformanceFrequency());
  double accumulator = 0.;
  while (running && (frames < 0 || shown < frames)) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) running = false;
      else if (e.type == SDL_CONTROLLERDEVICEADDED && !pad) pad = first_controller();
      else if (e.type == SDL_CONTROLLERDEVICEREMOVED && pad &&
        e.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad))) {
        SDL_GameControllerClose(pad); pad = first_controller();
      }
      else if (e.type == SDL_KEYDOWN && !e.key.repeat) {
        auto key = e.key.keysym.sym;
        if (key == SDLK_ESCAPE) running = false;
        if (key == SDLK_F9) {
          game.set_widescreen(!game.widescreen());
          SDL_RenderSetLogicalSize(r, int(game.view_width()), 540);
        }
        if (key == SDLK_f && (e.key.keysym.mod & KMOD_GUI) &&
            (e.key.keysym.mod & KMOD_CTRL)) {
          const bool full = SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP;
          SDL_SetWindowFullscreen(win, full ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
        }
      }
    }
    const Uint8* k = SDL_GetKeyboardState(nullptr);
    Input input{};
    input.move = float(int(k[SDL_SCANCODE_D] || k[SDL_SCANCODE_RIGHT]) -
                       int(k[SDL_SCANCODE_A] || k[SDL_SCANCODE_LEFT]));
    input.jump = k[SDL_SCANCODE_SPACE] || k[SDL_SCANCODE_UP];
    input.beam = k[SDL_SCANCODE_LSHIFT];
    input.fire = k[SDL_SCANCODE_LCTRL];
    input.aim.x = float(int(k[SDL_SCANCODE_L]) - int(k[SDL_SCANCODE_J]));
    input.aim.y = float(int(k[SDL_SCANCODE_K]) - int(k[SDL_SCANCODE_I]));
    if (pad) {
      const auto axis = [&](SDL_GameControllerAxis a) {
        return SDL_GameControllerGetAxis(pad, a) / 32767.f;
      };
      if (std::fabs(axis(SDL_CONTROLLER_AXIS_LEFTX)) > .2f)
        input.move = axis(SDL_CONTROLLER_AXIS_LEFTX);
      else if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) input.move = -1;
      else if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) input.move = 1;
      input.jump |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A);
      input.fire |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_X);
      input.beam |= axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT) > .25f;
      float ax = axis(SDL_CONTROLLER_AXIS_RIGHTX), ay = axis(SDL_CONTROLLER_AXIS_RIGHTY);
      if (std::hypot(ax, ay) > .25f) input.aim = {ax, ay};
    }
    Uint64 now = SDL_GetPerformanceCounter();
    accumulator += std::min(0.10, double(now - last) / frequency);
    last = now;
    int steps = 0;
    while (accumulator >= 1. / 60. && steps++ < 6) {
      game.tick(input, 1.f/60.f);
      accumulator -= 1./60.;
    }
    draw(r, game, input.fire);
    ++shown;
    SDL_Delay(1);
  }
  if (pad) SDL_GameControllerClose(pad);
  SDL_DestroyRenderer(r);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
