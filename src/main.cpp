// Turrican II — SDL2 front end.
#include "amiga/machine.hpp"

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

// Scripted input for automated runs:  <frame> <command> [args]
//   key <SDL key name> [down|up|tap]   joy <subset of "udlrf">
//   mouse <dx> <dy>   lmb|rmb <0|1>   shot <file.ppm>   quit
//   poke <hexaddr> <hexvalue> [b|w|l]   (force a game variable)
struct ScriptEvent {
  int frame;
  std::string cmd;
  std::vector<std::string> args;
};

std::vector<ScriptEvent> load_script(const std::string& path) {
  std::vector<ScriptEvent> ev;
  std::ifstream in(path);
  std::string line;
  int last = 0;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream ss(line);
    std::string f;
    ScriptEvent e;
    ss >> f >> e.cmd;
    // "+N" is relative to the previous event
    e.frame = f[0] == '+' ? last + std::atoi(f.c_str() + 1) : std::atoi(f.c_str());
    last = e.frame;
    std::string a;
    while (ss >> a) e.args.push_back(a);
    ev.push_back(e);
  }
  return ev;
}

int amiga_rawkey(SDL_Scancode sc) {
  switch (sc) {
    case SDL_SCANCODE_GRAVE: return 0x00;
    case SDL_SCANCODE_1: return 0x01; case SDL_SCANCODE_2: return 0x02;
    case SDL_SCANCODE_3: return 0x03; case SDL_SCANCODE_4: return 0x04;
    case SDL_SCANCODE_5: return 0x05; case SDL_SCANCODE_6: return 0x06;
    case SDL_SCANCODE_7: return 0x07; case SDL_SCANCODE_8: return 0x08;
    case SDL_SCANCODE_9: return 0x09; case SDL_SCANCODE_0: return 0x0A;
    case SDL_SCANCODE_MINUS: return 0x0B; case SDL_SCANCODE_EQUALS: return 0x0C;
    case SDL_SCANCODE_BACKSLASH: return 0x0D;
    case SDL_SCANCODE_Q: return 0x10; case SDL_SCANCODE_W: return 0x11;
    case SDL_SCANCODE_E: return 0x12; case SDL_SCANCODE_R: return 0x13;
    case SDL_SCANCODE_T: return 0x14; case SDL_SCANCODE_Y: return 0x15;
    case SDL_SCANCODE_U: return 0x16; case SDL_SCANCODE_I: return 0x17;
    case SDL_SCANCODE_O: return 0x18; case SDL_SCANCODE_P: return 0x19;
    case SDL_SCANCODE_LEFTBRACKET: return 0x1A; case SDL_SCANCODE_RIGHTBRACKET: return 0x1B;
    case SDL_SCANCODE_A: return 0x20; case SDL_SCANCODE_S: return 0x21;
    case SDL_SCANCODE_D: return 0x22; case SDL_SCANCODE_F: return 0x23;
    case SDL_SCANCODE_G: return 0x24; case SDL_SCANCODE_H: return 0x25;
    case SDL_SCANCODE_J: return 0x26; case SDL_SCANCODE_K: return 0x27;
    case SDL_SCANCODE_L: return 0x28; case SDL_SCANCODE_SEMICOLON: return 0x29;
    case SDL_SCANCODE_APOSTROPHE: return 0x2A;
    case SDL_SCANCODE_NONUSBACKSLASH: return 0x30;
    case SDL_SCANCODE_Z: return 0x31; case SDL_SCANCODE_X: return 0x32;
    case SDL_SCANCODE_C: return 0x33; case SDL_SCANCODE_V: return 0x34;
    case SDL_SCANCODE_B: return 0x35; case SDL_SCANCODE_N: return 0x36;
    case SDL_SCANCODE_M: return 0x37; case SDL_SCANCODE_COMMA: return 0x38;
    case SDL_SCANCODE_PERIOD: return 0x39; case SDL_SCANCODE_SLASH: return 0x3A;
    case SDL_SCANCODE_SPACE: return 0x40; case SDL_SCANCODE_BACKSPACE: return 0x41;
    case SDL_SCANCODE_TAB: return 0x42; case SDL_SCANCODE_KP_ENTER: return 0x43;
    case SDL_SCANCODE_RETURN: return 0x44; case SDL_SCANCODE_ESCAPE: return 0x45;
    case SDL_SCANCODE_DELETE: return 0x46; case SDL_SCANCODE_KP_MINUS: return 0x4A;
    case SDL_SCANCODE_UP: return 0x4C; case SDL_SCANCODE_DOWN: return 0x4D;
    case SDL_SCANCODE_RIGHT: return 0x4E; case SDL_SCANCODE_LEFT: return 0x4F;
    case SDL_SCANCODE_F1: return 0x50; case SDL_SCANCODE_F2: return 0x51;
    case SDL_SCANCODE_F3: return 0x52; case SDL_SCANCODE_F4: return 0x53;
    case SDL_SCANCODE_F5: return 0x54; case SDL_SCANCODE_F6: return 0x55;
    case SDL_SCANCODE_F7: return 0x56; case SDL_SCANCODE_F8: return 0x57;
    case SDL_SCANCODE_F9: return 0x58; case SDL_SCANCODE_F10: return 0x59;
    case SDL_SCANCODE_KP_0: return 0x0F; case SDL_SCANCODE_KP_1: return 0x1D;
    case SDL_SCANCODE_KP_2: return 0x1E; case SDL_SCANCODE_KP_3: return 0x1F;
    case SDL_SCANCODE_KP_4: return 0x2D; case SDL_SCANCODE_KP_5: return 0x2E;
    case SDL_SCANCODE_KP_6: return 0x2F; case SDL_SCANCODE_KP_7: return 0x3D;
    case SDL_SCANCODE_KP_8: return 0x3E; case SDL_SCANCODE_KP_9: return 0x3F;
    case SDL_SCANCODE_KP_PERIOD: return 0x3C; case SDL_SCANCODE_KP_DIVIDE: return 0x5C;
    case SDL_SCANCODE_KP_MULTIPLY: return 0x5D; case SDL_SCANCODE_KP_PLUS: return 0x5E;
    case SDL_SCANCODE_HOME: return 0x5F;  // HELP
    case SDL_SCANCODE_LSHIFT: return 0x60; case SDL_SCANCODE_RSHIFT: return 0x61;
    case SDL_SCANCODE_CAPSLOCK: return 0x62; case SDL_SCANCODE_LCTRL: return 0x63;
    case SDL_SCANCODE_LALT: return 0x64; case SDL_SCANCODE_RALT: return 0x65;
    case SDL_SCANCODE_LGUI: return 0x66; case SDL_SCANCODE_RGUI: return 0x67;
    default: return -1;
  }
}

// ---------------------------------------------------------------------------
// Host-side debug menu (Start button).  Fully separate from the emulated
// game code — only uses Machine::boot / key_event / w8 for actions that are
// safe with the current reverse-engineering state.
// ---------------------------------------------------------------------------
struct DebugMenu {
  bool open = false;
  int sel = 0;
  int cooldown = 0;          // frames to ignore D-pad after a move
  bool always_autofire = false;
  bool frozen = false;       // stop advancing the machine
  std::string status;        // one-shot feedback line
  int status_ttl = 0;

  // Candidates from MAIN binary scan (move.b #3,abs.w).  Experimental.
  static constexpr uint32_t CAND_LIVES_A = 0x25c2;
  static constexpr uint32_t CAND_LIVES_B = 0x3ce4;

  enum Item : int {
    I_RESUME = 0,
    I_RESTART,
    I_KILL,
    I_MAX_LIVES,
    I_MAX_POWER,
    I_TOGGLE_AUTOFIRE,
    I_TOGGLE_FREEZE,
    I_COUNT
  };

  static const char* label(int i) {
    switch (i) {
      case I_RESUME:         return "Resume";
      case I_RESTART:        return "Restart game";
      case I_KILL:           return "Kill player (Esc)";
      case I_MAX_LIVES:      return "Max lives (exp.)";
      case I_MAX_POWER:      return "Max powerlines (exp.)";
      case I_TOGGLE_AUTOFIRE:return "Toggle always auto-fire";
      case I_TOGGLE_FREEZE:  return "Toggle freeze emulation";
      default:               return "?";
    }
  }

  void set_status(const char* s) { status = s; status_ttl = 90; }

  void open_menu() {
    open = true;
    sel = 0;
    cooldown = 8;
  }

  void close_menu() { open = false; }

  // Called every frame while open; consumes D-pad / A / B / Start.
  // Returns true if the event was eaten (do not pass to game).
  bool navigate(bool up, bool down, bool a, bool b, bool start,
                amiga::Machine* m, const std::string& adf, const std::string& save) {
    if (cooldown > 0) --cooldown;
    if (start && cooldown == 0) { close_menu(); return true; }
    if (b && cooldown == 0) { close_menu(); return true; }
    if (cooldown == 0) {
      if (up)   { sel = (sel + I_COUNT - 1) % I_COUNT; cooldown = 8; }
      if (down) { sel = (sel + 1) % I_COUNT; cooldown = 8; }
      if (a) {
        cooldown = 12;
        apply(sel, m, adf, save);
      }
    }
    return true;
  }

  void apply(int item, amiga::Machine* m, const std::string& adf, const std::string& save) {
    switch (item) {
      case I_RESUME:
        close_menu();
        break;
      case I_RESTART:
        if (m->boot(adf, save)) set_status("Restarted");
        else set_status("Boot failed");
        close_menu();
        break;
      case I_KILL:
        // Game uses Esc as suicide / lose a life.
        m->key_event(0x45, true);
        m->key_event(0x45, false);
        set_status("Kill (Esc)");
        close_menu();
        break;
      case I_MAX_LIVES:
        // Experimental: poke both candidate addresses found in MAIN.
        m->w8(CAND_LIVES_A, 9);
        m->w8(CAND_LIVES_B, 9);
        set_status("Poked lives candidates = 9");
        break;
      case I_MAX_POWER:
        // Powerline count is not yet mapped; leave a stub status.
        set_status("Powerlines: address unknown");
        break;
      case I_TOGGLE_AUTOFIRE:
        always_autofire = !always_autofire;
        set_status(always_autofire ? "Always auto-fire ON" : "Always auto-fire OFF");
        break;
      case I_TOGGLE_FREEZE:
        frozen = !frozen;
        set_status(frozen ? "Emulation frozen" : "Emulation running");
        break;
      default:
        break;
    }
  }

  // Tiny 5×7 font for the overlay (ASCII 32–90 subset, 1 bpp rows).
  // Drawn into the 32-bit ARGB framebuffer.
  static void draw_char(uint32_t* fb, int x, int y, char c, uint32_t col) {
    static const uint8_t font[64][7] = {
      // space through Z (indices 0..58); only a useful subset filled
      {0,0,0,0,0,0,0}, // ' '
      {0x04,0x04,0x04,0x04,0x00,0x04,0x00}, // !
      {0x0A,0x0A,0x00,0x00,0x00,0x00,0x00}, // "
      {0x0A,0x1F,0x0A,0x1F,0x0A,0x00,0x00}, // #
      {0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04}, // $
      {0x18,0x19,0x02,0x04,0x08,0x13,0x03}, // %
      {0x08,0x14,0x08,0x15,0x12,0x0D,0x00}, // &
      {0x04,0x04,0x00,0x00,0x00,0x00,0x00}, // '
      {0x02,0x04,0x04,0x04,0x04,0x02,0x00}, // (
      {0x08,0x04,0x04,0x04,0x04,0x08,0x00}, // )
      {0x00,0x0A,0x04,0x1F,0x04,0x0A,0x00}, // *
      {0x00,0x04,0x04,0x1F,0x04,0x04,0x00}, // +
      {0x00,0x00,0x00,0x00,0x04,0x04,0x08}, // ,
      {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}, // -
      {0x00,0x00,0x00,0x00,0x00,0x04,0x00}, // .
      {0x01,0x02,0x04,0x08,0x10,0x00,0x00}, // /
      {0x0E,0x11,0x13,0x15,0x19,0x0E,0x00}, // 0
      {0x04,0x0C,0x04,0x04,0x04,0x0E,0x00}, // 1
      {0x0E,0x11,0x01,0x06,0x08,0x1F,0x00}, // 2
      {0x1F,0x02,0x04,0x01,0x11,0x0E,0x00}, // 3
      {0x02,0x06,0x0A,0x12,0x1F,0x02,0x00}, // 4
      {0x1F,0x10,0x1E,0x01,0x11,0x0E,0x00}, // 5
      {0x06,0x08,0x1E,0x11,0x11,0x0E,0x00}, // 6
      {0x1F,0x01,0x02,0x04,0x08,0x08,0x00}, // 7
      {0x0E,0x11,0x0E,0x11,0x11,0x0E,0x00}, // 8
      {0x0E,0x11,0x11,0x0F,0x01,0x0C,0x00}, // 9
      {0x00,0x04,0x00,0x00,0x04,0x00,0x00}, // :
      {0x00,0x04,0x00,0x00,0x04,0x04,0x08}, // ;
      {0x02,0x04,0x08,0x04,0x02,0x00,0x00}, // <
      {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00}, // =
      {0x08,0x04,0x02,0x04,0x08,0x00,0x00}, // >
      {0x0E,0x11,0x01,0x06,0x00,0x04,0x00}, // ?
      {0x0E,0x11,0x17,0x15,0x17,0x08,0x06}, // @
      {0x0E,0x11,0x11,0x1F,0x11,0x11,0x00}, // A
      {0x1E,0x11,0x1E,0x11,0x11,0x1E,0x00}, // B
      {0x0E,0x11,0x10,0x10,0x11,0x0E,0x00}, // C
      {0x1E,0x11,0x11,0x11,0x11,0x1E,0x00}, // D
      {0x1F,0x10,0x1E,0x10,0x10,0x1F,0x00}, // E
      {0x1F,0x10,0x1E,0x10,0x10,0x10,0x00}, // F
      {0x0E,0x11,0x10,0x13,0x11,0x0F,0x00}, // G
      {0x11,0x11,0x1F,0x11,0x11,0x11,0x00}, // H
      {0x0E,0x04,0x04,0x04,0x04,0x0E,0x00}, // I
      {0x01,0x01,0x01,0x01,0x11,0x0E,0x00}, // J
      {0x11,0x12,0x1C,0x12,0x11,0x11,0x00}, // K
      {0x10,0x10,0x10,0x10,0x10,0x1F,0x00}, // L
      {0x11,0x1B,0x15,0x11,0x11,0x11,0x00}, // M
      {0x11,0x19,0x15,0x13,0x11,0x11,0x00}, // N
      {0x0E,0x11,0x11,0x11,0x11,0x0E,0x00}, // O
      {0x1E,0x11,0x11,0x1E,0x10,0x10,0x00}, // P
      {0x0E,0x11,0x11,0x15,0x12,0x0D,0x00}, // Q
      {0x1E,0x11,0x11,0x1E,0x12,0x11,0x00}, // R
      {0x0E,0x11,0x08,0x04,0x11,0x0E,0x00}, // S
      {0x1F,0x04,0x04,0x04,0x04,0x04,0x00}, // T
      {0x11,0x11,0x11,0x11,0x11,0x0E,0x00}, // U
      {0x11,0x11,0x11,0x11,0x0A,0x04,0x00}, // V
      {0x11,0x11,0x11,0x15,0x1B,0x11,0x00}, // W
      {0x11,0x11,0x0A,0x04,0x0A,0x11,0x00}, // X
      {0x11,0x11,0x0A,0x04,0x04,0x04,0x00}, // Y
      {0x1F,0x01,0x02,0x04,0x08,0x1F,0x00}, // Z
    };
    if (c < 32 || c > 90) c = '?';
    const uint8_t* g = font[c - 32];
    for (int row = 0; row < 7; ++row) {
      uint8_t bits = g[row];
      for (int colb = 0; colb < 5; ++colb) {
        if (bits & (0x10 >> colb)) {
          int px = x + colb, py = y + row;
          if (px >= 0 && px < amiga::FB_W && py >= 0 && py < amiga::VIEW_H)
            fb[py * amiga::FB_W + px] = col;
        }
      }
    }
  }

  static void draw_text(uint32_t* fb, int x, int y, const char* s, uint32_t col) {
    for (; *s; ++s, x += 6) draw_char(fb, x, y, (*s >= 'a' && *s <= 'z') ? *s - 32 : *s, col);
  }

  void draw(uint32_t* fb) {
    if (!open && status_ttl <= 0) return;
    if (status_ttl > 0) {
      --status_ttl;
      // status bar at bottom
      for (int y = amiga::VIEW_H - 12; y < amiga::VIEW_H; ++y)
        for (int x = 0; x < amiga::FB_W; ++x)
          fb[y * amiga::FB_W + x] = 0xC0000000 | (fb[y * amiga::FB_W + x] & 0x00FFFFFF);
      draw_text(fb, 4, amiga::VIEW_H - 10, status.c_str(), 0xFFFFFFFFu);
    }
    if (!open) return;
    // Dim the playfield.
    for (int i = 0; i < amiga::FB_W * amiga::VIEW_H; ++i)
      fb[i] = (fb[i] >> 2) & 0x3F3F3F3Fu;
    const int box_x = 40, box_y = 40, box_w = 240, box_h = 20 + I_COUNT * 12 + 16;
    for (int y = box_y; y < box_y + box_h && y < amiga::VIEW_H; ++y)
      for (int x = box_x; x < box_x + box_w && x < amiga::FB_W; ++x)
        fb[y * amiga::FB_W + x] = 0xFF1A1A2Eu;
    // border
    for (int x = box_x; x < box_x + box_w; ++x) {
      if (box_y >= 0 && box_y < amiga::VIEW_H) fb[box_y * amiga::FB_W + x] = 0xFFAAAAFFu;
      int yb = box_y + box_h - 1;
      if (yb >= 0 && yb < amiga::VIEW_H) fb[yb * amiga::FB_W + x] = 0xFFAAAAFFu;
    }
    for (int y = box_y; y < box_y + box_h; ++y) {
      if (y < 0 || y >= amiga::VIEW_H) continue;
      if (box_x >= 0 && box_x < amiga::FB_W) fb[y * amiga::FB_W + box_x] = 0xFFAAAAFFu;
      int xb = box_x + box_w - 1;
      if (xb >= 0 && xb < amiga::FB_W) fb[y * amiga::FB_W + xb] = 0xFFAAAAFFu;
    }
    draw_text(fb, box_x + 8, box_y + 6, "DEBUG MENU", 0xFFFFFF55u);
    for (int i = 0; i < I_COUNT; ++i) {
      uint32_t col = (i == sel) ? 0xFFFFFF00u : 0xFFCCCCCCu;
      char line[64];
      std::snprintf(line, sizeof(line), "%c %s", (i == sel) ? '>' : ' ', label(i));
      draw_text(fb, box_x + 8, box_y + 20 + i * 12, line, col);
    }
    draw_text(fb, box_x + 8, box_y + box_h - 12, "A=OK  B/Start=Back", 0xFF8888AAu);
  }
};

class SdlHost : public amiga::Host {
public:
  SDL_Window* win = nullptr;
  SDL_Renderer* ren = nullptr;
  SDL_Texture* tex = nullptr;
  SDL_AudioDeviceID audio_dev = 0;
  amiga::Machine* machine = nullptr;
  SDL_GameController* pad = nullptr;
  bool mouse_grab = false;
  bool fast = false;
  bool quit = false;
  uint64_t next_frame = 0;
  int screenshot_frame = -1;
  std::string screenshot_path;
  int frame_no = 0;
  int quit_after = -1;
  std::vector<ScriptEvent> script;
  size_t script_pos = 0;
  std::vector<std::pair<int, int>> pending_up;  // frame, rawkey
  bool sjoy[5] = {};  // scripted joystick
  bool kjoy[5] = {};  // keyboard joystick
  bool pad_held_key[128] = {};
  int autofire_phase_ = 0;  // frame counter for X-button auto-fire pulse
  DebugMenu dbg_;
  std::string adf_path_;
  std::string save_path_;

  bool init(int scale) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
      fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
      return false;
    }
    // Window height ≈ 15/16 of the line count so 320×256 lores sits near 4:3;
    // logical size stays 320×256 and SDL letterboxes inside the window.
    win = SDL_CreateWindow("Turrican II", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           amiga::FB_W * scale, amiga::VIEW_H * scale * 15 / 16,
                           SDL_WINDOW_RESIZABLE);
    if (!win) return false;
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren) ren = SDL_CreateRenderer(win, -1, 0);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_RenderSetLogicalSize(ren, amiga::FB_W, amiga::VIEW_H);
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                            amiga::FB_W, amiga::VIEW_H);
    SDL_AudioSpec want{}, have{};
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    audio_dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (audio_dev) SDL_PauseAudioDevice(audio_dev, 0);
    for (int i = 0; i < SDL_NumJoysticks(); ++i)
      if (SDL_IsGameController(i)) { pad = SDL_GameControllerOpen(i); break; }
    next_frame = SDL_GetPerformanceCounter();
    return true;
  }

  void set_grab(bool g) {
    mouse_grab = g;
    SDL_SetRelativeMouseMode(g ? SDL_TRUE : SDL_FALSE);
  }

  void handle_events() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_QUIT: quit = true; break;
        case SDL_KEYDOWN:
        case SDL_KEYUP: {
          bool down = e.type == SDL_KEYDOWN;
          SDL_Scancode sc = e.key.keysym.scancode;
          if (sc == SDL_SCANCODE_F12 && down) { set_grab(!mouse_grab); break; }
          if ((sc == SDL_SCANCODE_F11 ||
               (sc == SDL_SCANCODE_RETURN && (e.key.keysym.mod & KMOD_ALT))) && down) {
            Uint32 fl = SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP;
            SDL_SetWindowFullscreen(win, fl ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
            break;
          }
          if (sc == SDL_SCANCODE_PAUSE && down) { fast = !fast; break; }
          if (sc == SDL_SCANCODE_F4 && down && (e.key.keysym.mod & KMOD_ALT)) { quit = true; break; }
          if (e.key.repeat) break;
          int rk = amiga_rawkey(sc);
          if (rk >= 0) machine->key_event(uint8_t(rk), down);
          // Cursor keys and right Ctrl also work as joystick in port 2.
          switch (sc) {
            case SDL_SCANCODE_UP: kjoy[0] = down; break;
            case SDL_SCANCODE_DOWN: kjoy[1] = down; break;
            case SDL_SCANCODE_LEFT: kjoy[2] = down; break;
            case SDL_SCANCODE_RIGHT: kjoy[3] = down; break;
            case SDL_SCANCODE_RCTRL: kjoy[4] = down; break;
            default: break;
          }
          break;
        }
        case SDL_MOUSEMOTION:
          if (mouse_grab) machine->mouse_move(e.motion.xrel, e.motion.yrel);
          break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
          bool down = e.type == SDL_MOUSEBUTTONDOWN;
          // Mouse grab is opt-in (--grab or F12); do not steal the pointer on click.
          if (e.button.button == SDL_BUTTON_LEFT) machine->mouse_button(0, down);
          if (e.button.button == SDL_BUTTON_RIGHT) machine->mouse_button(1, down);
          break;
        }
        case SDL_CONTROLLERDEVICEADDED:
          if (!pad) pad = SDL_GameControllerOpen(e.cdevice.which);
          break;
      }
    }
    run_script();
    // Joystick in port 2 from script, keyboard and game controller.
    bool up = sjoy[0] || kjoy[0], down = sjoy[1] || kjoy[1], left = sjoy[2] || kjoy[2];
    bool right = sjoy[3] || kjoy[3], fire = sjoy[4] || kjoy[4];
    bool pad_a = false, pad_b = false, pad_start = false;
    if (pad) poll_gamepad(up, down, left, right, fire, pad_a, pad_b, pad_start);

    // Start opens / closes the host-side debug menu (not passed to the game).
    static bool start_was = false;
    if (pad_start && !start_was) {
      if (dbg_.open) dbg_.close_menu();
      else dbg_.open_menu();
    }
    start_was = pad_start;

    if (dbg_.open) {
      dbg_.navigate(up, down, pad_a, pad_b, pad_start, machine, adf_path_, save_path_);
      // While the menu is open, neutralise game input.
      machine->joy_state(false, false, false, false, false);
      return;
    }

    if (dbg_.always_autofire) {
      autofire_phase_ = (autofire_phase_ + 1) % 8;
      if (autofire_phase_ < 3) fire = true;
    }
    machine->joy_state(up, down, left, right, fire);
  }

  void poll_gamepad(bool& up, bool& down, bool& left, bool& right, bool& fire,
                    bool& out_a, bool& out_b, bool& out_start) {
    auto btn = [&](SDL_GameControllerButton b) { return SDL_GameControllerGetButton(pad, b) != 0; };
    constexpr int dead = 12000;
    int lx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
    int ly = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
    // Analog stick + D-pad → digital joystick directions.
    up |= ly < -dead || btn(SDL_CONTROLLER_BUTTON_DPAD_UP);
    down |= ly > dead || btn(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    left |= lx < -dead || btn(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    right |= lx > dead || btn(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    out_a = btn(SDL_CONTROLLER_BUTTON_A);
    out_b = btn(SDL_CONTROLLER_BUTTON_B);
    out_start = btn(SDL_CONTROLLER_BUTTON_START);
    // A = jump (joystick up). Does not affect the fire line.
    if (out_a && !dbg_.open) up = true;
    // Y = regular fire (short press / hold = surround as the game already does).
    if (btn(SDL_CONTROLLER_BUTTON_Y)) fire = true;
    // X = auto-fire: pulse the fire button (~12 Hz).
    if (btn(SDL_CONTROLLER_BUTTON_X)) {
      autofire_phase_ = (autofire_phase_ + 1) % 8;
      if (autofire_phase_ < 3) fire = true;
    } else if (!dbg_.always_autofire) {
      autofire_phase_ = 0;
    }
    // Buttons that synthesize Amiga keyboard events (held while pressed).
    // Start is reserved for the debug menu and is not mapped to a key.
    static const struct { SDL_GameControllerButton b; int key; } map[] = {
        {SDL_CONTROLLER_BUTTON_B, 0x40},              // Space (powerline / 2nd fire)
        {SDL_CONTROLLER_BUTTON_BACK, 0x45},           // Esc
        {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 0x50},   // F1
        {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 0x51},  // F2
    };
    for (auto& m : map) {
      bool d = btn(m.b);
      if (d != pad_held_key[m.key]) {
        pad_held_key[m.key] = d;
        if (!dbg_.open) machine->key_event(uint8_t(m.key), d);
      }
    }
  }

  // Host-only workaround: the game programs a shorter DIW than 256 lines, so
  // the emulated framebuffer is top-aligned with black below the HUD.  Shift
  // the non-black band to the vertical centre of the 256-line buffer.  Bounds
  // are sticky (hysteresis) to avoid jitter when a row flickers.
  void center_active_region(uint32_t* fb) {
    auto row_mostly_black = [&](int y) {
      const uint32_t* row = fb + y * amiga::FB_W;
      int lit = 0;
      for (int x = 0; x < amiga::FB_W; x += 2) {
        // Ignore very dark pixels (border / clear colour).
        if ((row[x] & 0x00F8F8F8u) != 0) ++lit;
      }
      return lit < 6;
    };
    int top = 0, bot = amiga::VIEW_H - 1;
    while (top < bot && row_mostly_black(top)) ++top;
    while (bot > top && row_mostly_black(bot)) --bot;
    // Require a plausible active height; otherwise leave the frame alone
    // (title screens / loading may be mostly black).
    int h = bot - top + 1;
    if (h < 64 || h >= amiga::VIEW_H) return;

    // Sticky bounds: only move if the new band differs by more than 2 lines.
    if (center_top_ < 0 || (top - center_top_ > 2) || (center_top_ - top > 2) ||
        (bot - center_bot_ > 2) || (center_bot_ - bot > 2)) {
      center_top_ = top;
      center_bot_ = bot;
    } else {
      top = center_top_;
      bot = center_bot_;
      h = bot - top + 1;
    }
    int new_top = (amiga::VIEW_H - h) / 2;
    if (new_top == top) return;

    static std::vector<uint32_t> tmp;
    tmp.assign(amiga::FB_W * amiga::VIEW_H, 0u);
    std::copy(fb + top * amiga::FB_W, fb + (bot + 1) * amiga::FB_W,
              tmp.begin() + new_top * amiga::FB_W);
    std::copy(tmp.begin(), tmp.end(), fb);
  }

  int center_top_ = -1, center_bot_ = -1;

  void frame(const uint32_t* fb) override {
    ++frame_no;
    // Mutable copy so the host-side debug overlay can draw without touching
    // the machine's view buffer.
    static std::vector<uint32_t> overlay;
    overlay.assign(fb, fb + amiga::FB_W * amiga::VIEW_H);
    center_active_region(overlay.data());
    dbg_.draw(overlay.data());

    SDL_UpdateTexture(tex, nullptr, overlay.data(), amiga::FB_W * 4);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, nullptr, nullptr);
    SDL_RenderPresent(ren);
    if (frame_no == screenshot_frame) save_screenshot(overlay.data());
    if (!pending_shot.empty()) {
      std::swap(pending_shot, screenshot_path);
      save_screenshot(overlay.data());
      std::swap(pending_shot, screenshot_path);
      pending_shot.clear();
    }
    if (quit_after >= 0 && frame_no >= quit_after) quit = true;
    handle_events();
    // Freeze: keep presenting the last frame and servicing the menu until
    // the user unfreezes (host-only; does not alter Machine state).
    while (dbg_.frozen && !quit) {
      SDL_Delay(16);
      handle_events();
      dbg_.draw(overlay.data());
      SDL_UpdateTexture(tex, nullptr, overlay.data(), amiga::FB_W * 4);
      SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
      SDL_RenderClear(ren);
      SDL_RenderCopy(ren, tex, nullptr, nullptr);
      SDL_RenderPresent(ren);
    }
    if (quit) throw amiga::QuitRequested{};
    // Pace to the PAL frame rate unless fast-forwarding.
    uint64_t freq = SDL_GetPerformanceFrequency();
    next_frame += uint64_t(double(freq) / amiga::FRAME_HZ);
    uint64_t now = SDL_GetPerformanceCounter();
    if (fast || int64_t(next_frame - now) < -int64_t(freq / 10)) next_frame = now;
    // Signed arithmetic: the counter may pass next_frame between reads.
    while (!fast) {
      int64_t left = int64_t(next_frame - SDL_GetPerformanceCounter());
      if (left <= 0) break;
      int64_t left_us = left * 1000000 / int64_t(freq);
      if (left_us > 2000) SDL_Delay(uint32_t(left_us / 1000 - 1));
    }
  }

  void run_script() {
    for (size_t i = 0; i < pending_up.size();) {
      if (pending_up[i].first <= frame_no) {
        machine->key_event(uint8_t(pending_up[i].second), false);
        pending_up.erase(pending_up.begin() + long(i));
      } else {
        ++i;
      }
    }
    while (script_pos < script.size() && script[script_pos].frame <= frame_no) {
      const ScriptEvent& e = script[script_pos++];
      if (e.cmd == "key" && !e.args.empty()) {
        static const std::pair<const char*, const char*> alias[] = {
            {"lshift", "Left Shift"}, {"rshift", "Right Shift"}, {"ctrl", "Left Ctrl"},
            {"lalt", "Left Alt"}, {"ralt", "Right Alt"}, {"equals", "="}, {"minus", "-"},
            {"comma", ","}, {"period", "."}, {"enter", "Keypad Enter"}};
        std::string name = e.args[0];
        for (auto& [a, b] : alias) if (name == a) name = b;
        int rk = amiga_rawkey(SDL_GetScancodeFromName(name.c_str()));
        if (rk < 0) rk = int(std::strtol(name.c_str(), nullptr, 16));
        // Keys are held long enough for the game's once-per-frame polling.
        std::string mode = e.args.size() > 1 ? e.args[1] : "tap";
        if (mode == "down") machine->key_event(uint8_t(rk), true);
        else if (mode == "up") machine->key_event(uint8_t(rk), false);
        else { machine->key_event(uint8_t(rk), true); pending_up.push_back({frame_no + 4, rk}); }
      } else if (e.cmd == "joy") {
        std::string st = e.args.empty() ? "" : e.args[0];
        const char* names = "udlrf";
        for (int k = 0; k < 5; ++k) sjoy[k] = st.find(names[k]) != std::string::npos;
      } else if (e.cmd == "mouse" && e.args.size() >= 2) {
        machine->mouse_move(std::atoi(e.args[0].c_str()), std::atoi(e.args[1].c_str()));
      } else if ((e.cmd == "lmb" || e.cmd == "rmb") && !e.args.empty()) {
        machine->mouse_button(e.cmd == "lmb" ? 0 : 1, e.args[0] != "0");
      } else if (e.cmd == "poke" && e.args.size() >= 2) {
        // poke <hexaddr> <hexvalue> [b|w|l]: force a game variable (test
        // scenarios use it to reach states that are hard to play into)
        uint32_t a = uint32_t(std::strtoul(e.args[0].c_str(), nullptr, 16));
        uint32_t v = uint32_t(std::strtoul(e.args[1].c_str(), nullptr, 16));
        char sz = e.args.size() > 2 ? e.args[2][0] : 'b';
        if (sz == 'l') machine->w32(a, v);
        else if (sz == 'w') machine->w16(a, uint16_t(v));
        else machine->w8(a, uint8_t(v));
      } else if (e.cmd == "shot" && !e.args.empty()) {
        pending_shot = e.args[0];
      } else if (e.cmd == "quit") {
        quit = true;
      }
    }
  }

  std::string pending_shot;

  void save_screenshot(const uint32_t* fb) {
    FILE* f = fopen(screenshot_path.c_str(), "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", amiga::FB_W, amiga::VIEW_H);
    for (int i = 0; i < amiga::FB_W * amiga::VIEW_H; ++i) {
      uint8_t px[3] = {uint8_t(fb[i] >> 16), uint8_t(fb[i] >> 8), uint8_t(fb[i])};
      fwrite(px, 1, 3, f);
    }
    fclose(f);
  }

  FILE* wav = nullptr;
  uint32_t wav_bytes = 0;

  void open_wav(const std::string& path) {
    wav = fopen(path.c_str(), "wb");
    if (wav) { uint8_t hdr[44] = {}; fwrite(hdr, 1, 44, wav); }
  }

  void close_wav() {
    if (!wav) return;
    auto le32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) fputc(int(v >> (8 * i)) & 0xFF, wav); };
    auto le16 = [&](uint16_t v) { fputc(v & 0xFF, wav); fputc(v >> 8, wav); };
    fseek(wav, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, wav); le32(36 + wav_bytes); fwrite("WAVEfmt ", 1, 8, wav);
    le32(16); le16(1); le16(2); le32(48000); le32(48000 * 4); le16(4); le16(16);
    fwrite("data", 1, 4, wav); le32(wav_bytes);
    fclose(wav);
    wav = nullptr;
  }

  void audio(const int16_t* samples, size_t frames) override {
    if (wav) { fwrite(samples, 4, frames, wav); wav_bytes += uint32_t(frames * 4); }
    if (!audio_dev || fast) return;
    // Keep latency bounded.
    if (SDL_GetQueuedAudioSize(audio_dev) > 48000 * 4 / 5) return;
    SDL_QueueAudio(audio_dev, samples, uint32_t(frames * 4));
  }
};

void usage() {
  fprintf(stderr,
          "usage: turrican2 [options] [disk.adf]\n"
          "  disk.adf         the original game disk (extended ADF)\n"
          "                   (default: T2.adf in the current directory,\n"
          "                   assets/ or the install data dir)\n"
          "  --scale N        window scale (default 3)\n"
          "  --save DIR       directory for the written disk\n"
          "                   (default: $XDG_DATA_HOME/turrican2)\n"
          "  --speed X        CPU speed multiplier (default 1 = A500)\n"
          "  --verbose        log OS calls\n"
          "  --trace          trace every instruction (very slow)\n"
          "  --screenshot N FILE  save frame N as PPM\n"
          "  --frames N       quit after N frames\n"
          "  --fast           run unthrottled\n"

          "  --wav FILE       record audio\n"
          "  --coverage FILE  write executed code addresses\n"
          "  --ram-hash N     log a RAM hash every N frames\n"
          "  --script FILE    scripted input (see source)\n"
          "  --grab           capture the mouse at start (F12 toggles)\n"
          "keys: cursor keys/right Ctrl = joystick, F11/Alt-Enter fullscreen,\n"
          "      Pause fast-forward; all other keys go to the game.\n"
          "pad:  left stick/D-pad = move, A = jump, Y = fire, X = auto-fire,\n"
          "      B = Space (powerline), Back = Esc, Start = debug menu.\n");
}

}  // namespace

int main(int argc, char** argv) {
  std::string adf_path, save_dir;
  int scale = 3;
  double speed = 1.0;
  bool verbose = false, trace = false, fast = false;
  int shot_frame = -1, quit_after = -1;
  std::string shot_path, script_path, wav_path, coverage_path;
  int ram_hash = 0;
  bool grab_mouse = false;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--scale" && i + 1 < argc) scale = std::atoi(argv[++i]);
    else if (a == "--save" && i + 1 < argc) save_dir = argv[++i];
    else if (a == "--speed" && i + 1 < argc) speed = std::atof(argv[++i]);
    else if (a == "--verbose") verbose = true;
    else if (a == "--trace") trace = true;
    else if (a == "--screenshot" && i + 2 < argc) { shot_frame = std::atoi(argv[++i]); shot_path = argv[++i]; }
    else if (a == "--frames" && i + 1 < argc) quit_after = std::atoi(argv[++i]);
    else if (a == "--fast") fast = true;
    else if (a == "--wav" && i + 1 < argc) wav_path = argv[++i];
    else if (a == "--coverage" && i + 1 < argc) coverage_path = argv[++i];
    else if (a == "--ram-hash" && i + 1 < argc) ram_hash = std::atoi(argv[++i]);
    else if (a == "--script" && i + 1 < argc) script_path = argv[++i];
    else if (a == "--grab") grab_mouse = true;
    else if (a == "-h" || a == "--help") { usage(); return 0; }
    else adf_path = a;
  }
  if (adf_path.empty()) {
    for (std::string cand : {std::string("."), std::string("assets"), std::string(T2_DATADIR)}) {
      std::string p = cand + "/T2.adf";
      if (std::filesystem::exists(p)) { adf_path = p; break; }
    }
  }
  if (adf_path.empty()) {
    fprintf(stderr, "Cannot find the game disk (T2.adf).\n");
    usage();
    return 1;
  }
  if (save_dir.empty()) {
    const char* xdg = getenv("XDG_DATA_HOME");
    const char* home = getenv("HOME");
    if (xdg && *xdg) save_dir = std::string(xdg) + "/turrican2";
    else if (home) save_dir = std::string(home) + "/.local/share/turrican2";
    else save_dir = ".";
  }

  amiga::Machine machine;
  machine.verbose = verbose;
  machine.trace = trace;
  machine.set_cpu_speed(speed);
  machine.coverage_path = coverage_path;
  machine.ram_hash_every = ram_hash;
  if (!machine.boot(adf_path, save_dir + "/disk.adf")) return 1;

  SdlHost host;
  host.machine = &machine;
  host.adf_path_ = adf_path;
  host.save_path_ = save_dir + "/disk.adf";
  host.screenshot_frame = shot_frame;
  host.screenshot_path = shot_path;
  host.quit_after = quit_after;
  host.fast = fast;
  if (!script_path.empty()) host.script = load_script(script_path);
  if (!host.init(scale)) return 1;
  if (grab_mouse) host.set_grab(true);
  machine.set_host(&host);
  if (!wav_path.empty()) host.open_wav(wav_path);
  machine.run();
  host.close_wav();
  SDL_Quit();
  return 0;
}
