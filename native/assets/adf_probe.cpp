// SPDX-License-Identifier: GPL-3.0-or-later
#include "assets/adf_probe.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <vector>

namespace t2native {
namespace {
uint16_t be16(const uint8_t* p) { return uint16_t(p[0]) << 8 | p[1]; }
uint32_t be32(const uint8_t* p) {
  return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 |
         uint32_t(p[2]) << 8 | p[3];
}
}  // namespace
AdfProbe probe_adf(const std::string& path) {
  AdfProbe result;
  std::error_code ec;
  auto size = std::filesystem::file_size(path, ec);
  if (ec) { result.reason = "Cannot read disk image metadata"; return result; }
  result.bytes = size;
  std::ifstream file(path, std::ios::binary);
  if (!file) { result.reason = "Cannot open disk image"; return result; }
  std::array<uint8_t, 12> first{};
  file.read(reinterpret_cast<char*>(first.data()), first.size());
  const auto good = file.gcount();
  if (size == 901120) {
    result.format = AdfFormat::standard;
    result.tracks = 160;
    result.declared_bytes = size;
    result.structurally_supported = true;
    result.reason = "Standard 880 KiB ADF; original custom-track protection may be absent";
    return result;
  }
  if (good < 12) { result.reason = "Disk image too short"; return result; }
  const std::string magic(reinterpret_cast<char*>(first.data()), 8);
  if (magic == "UAE--ADF") {
    result.format = AdfFormat::uae_old;
    result.tracks = 160;
    constexpr size_t header = 8 + 160 * 4;
    if (size < header) { result.reason = "Truncated extended ADF track table"; return result; }
    file.clear(); file.seekg(8);
    std::vector<uint8_t> descriptors(160 * 4);
    if (!file.read(reinterpret_cast<char*>(descriptors.data()), descriptors.size())) {
      result.reason = "Unreadable extended ADF track table"; return result;
    }
    uint64_t expected = header;
    for (size_t i = 0; i < 160; ++i) {
      auto* d = descriptors.data() + 4 * i;
      uint16_t sync = be16(d), length = be16(d + 2);
      if (!length) { result.reason = "Zero-length track in table"; return result; }
      if (sync) ++result.raw_tracks;
      if (sync == 0x9521) ++result.sync_9521_tracks;
      expected += length;
    }
    result.declared_bytes = expected;
    if (size > expected) { result.reason = "Unexpected extra bytes after tracks"; return result; }
    if (expected - size > 256) { result.reason = "Extended ADF truncated by more than 256 bytes"; return result; }
    result.missing_tail_bytes = static_cast<unsigned>(expected - size);
    result.structurally_supported = true;
    result.reason = result.missing_tail_bytes ? "Last track is slightly truncated" : "Extended ADF track layout consistent";
    return result;
  }
  if (magic == "UAE-1ADF") {
    result.format = AdfFormat::uae_new;
    result.tracks = be16(first.data() + 10);
    if (result.tracks == 0 || result.tracks > 160 || size < 12 + 12ull * result.tracks) {
      result.reason = "Invalid new extended ADF track table"; return result;
    }
    file.clear(); file.seekg(12);
    std::vector<uint8_t> headers(result.tracks * 12);
    if (!file.read(reinterpret_cast<char*>(headers.data()), headers.size())) {
      result.reason = "Unreadable new extended ADF track table"; return result;
    }
    uint64_t expected = 12 + result.tracks * 12;
    for (unsigned i = 0; i < result.tracks; ++i) {
      const auto* h = headers.data() + i * 12;
      const uint16_t type = be16(h + 2);
      if (type > 1) { result.reason = "Unsupported track type"; return result; }
      if (type == 1) ++result.raw_tracks;
      expected += be32(h + 4);
    }
    result.declared_bytes = expected;
    if (expected != size) { result.reason = "New extended ADF track sizes inconsistent"; return result; }
    result.structurally_supported = true;
    result.reason = "New extended ADF track layout consistent";
    return result;
  }
  result.reason = "Unknown ADF format";
  return result;
}
bool is_known_reference_sha256(const std::string& hash) {
  // Exact identification of the user-supplied test image, not a license check.
  // SHA-256 is intentionally the only stored user-provided file fingerprint.
  return hash == "ce37990a07089a08d41291b9705c67da99534fc470b93f1ca49cf8bed0ced4b4";
}
}  // namespace t2native
