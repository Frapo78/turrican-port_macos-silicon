// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <string>
namespace t2native {
enum class AdfFormat { invalid, standard, uae_old, uae_new };
struct AdfProbe {
  AdfFormat format = AdfFormat::invalid;
  uint64_t bytes = 0;
  uint64_t declared_bytes = 0;
  unsigned tracks = 0;
  unsigned raw_tracks = 0;
  unsigned sync_9521_tracks = 0;
  unsigned missing_tail_bytes = 0;
  bool structurally_supported = false;
  std::string reason;
};
AdfProbe probe_adf(const std::string& path);
bool is_known_reference_sha256(const std::string& lower_hex_sha256);
}  // namespace t2native
