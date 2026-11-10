// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/game.hpp"
#include "assets/adf_probe.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>
#include <iostream>
using namespace t2native;
int main() {
  Game g;
  const auto x0 = g.player().pos.x;
  for (int i = 0; i < 60; ++i) g.tick(Input{.move=1.f}, 1.f/60.f);
  assert(g.player().pos.x > x0 + 100.f);
  assert(g.ticks() == 60);
  g.tick(Input{.beam=true, .aim={0.f, -1.f}}, 1.f/60.f);
  assert(g.beam_active());
  assert(std::fabs(g.player().beam_angle + 1.5707963f) < .02f);
  g.set_widescreen(false);
  assert(g.view_width() == 720.f);
  g.set_widescreen(true);
  assert(g.view_width() == 960.f);
  assert(g.camera_x() >= 0.f);
  assert(is_known_reference_sha256("ce37990a07089a08d41291b9705c67da99534fc470b93f1ca49cf8bed0ced4b4"));
  assert(!is_known_reference_sha256("invalid"));
  // Synthetic standard ADF: inspect only metadata, without any copyrighted content.
  auto p = std::filesystem::temp_directory_path() / "t2native-synthetic-test.adf";
  {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.seekp(901120 - 1);
    out.put(0);
  }
  auto probe = probe_adf(p.string());
  assert(probe.structurally_supported && probe.format == AdfFormat::standard);
  std::filesystem::remove(p);
  // Synthetic old-format ADF with the last track missing 1 byte. The
  // reference disk has an analogous 248-byte short final track.
  {
    std::vector<unsigned char> header(648, 0);
    const char magic[] = "UAE--ADF";
    for (int j = 0; j < 8; ++j) header[j] = static_cast<unsigned char>(magic[j]);
    for (int i = 0; i < 160; ++i) {
      header[8 + i * 4] = i < 2 ? 0 : 0x95;
      header[9 + i * 4] = i < 2 ? 0 : 0x21;
      header[8 + i * 4 + 3] = 1;
    }
    std::ofstream out(p, std::ios::binary);
    out.write(reinterpret_cast<const char*>(header.data()), header.size());
    std::vector<unsigned char> payload(159, 0);
    out.write(reinterpret_cast<const char*>(payload.data()), payload.size());
  }
  probe = probe_adf(p.string());
  assert(probe.structurally_supported && probe.format == AdfFormat::uae_old);
  assert(probe.missing_tail_bytes == 1 && probe.sync_9521_tracks == 158);
  std::filesystem::remove(p);
  std::cout << "Native engine core and ADF probe tests passed\n";
}
