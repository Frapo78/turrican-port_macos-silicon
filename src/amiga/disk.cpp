#include "amiga/disk.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <filesystem>

namespace amiga {

namespace {
bool read_whole_file(const std::string& path, std::vector<uint8_t>& out) {
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) return false;
  out.clear();
  uint8_t buf[65536];
  size_t n;
  while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
  fclose(f);
  return true;
}

uint32_t be32(const uint8_t* p) {
  return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}
uint16_t be16(const uint8_t* p) { return uint16_t(p[0] << 8 | p[1]); }

constexpr uint32_t DOS_TRACK = 11 * 512;
}  // namespace

bool Disk::load(const std::string& adf_path, const std::string& save_path) {
  save_path_ = save_path;
  std::vector<uint8_t> file;
  if (!read_whole_file(adf_path, file)) {
    fprintf(stderr, "cannot read ADF %s\n", adf_path.c_str());
    return false;
  }
  if (file.size() >= 8 && std::memcmp(file.data(), "UAE-", 4) == 0) {
    if (!load_extended(file)) {
      fprintf(stderr, "%s: bad extended ADF\n", adf_path.c_str());
      return false;
    }
  } else if (file.size() == ADF_SIZE) {
    img_ = std::move(file);
    raw_.assign(NUM_TRACKS, {});
  } else {
    fprintf(stderr, "%s: not an ADF (%zu bytes)\n", adf_path.c_str(), file.size());
    return false;
  }
  // The saved copy holds the sector data as the game last wrote it.
  std::vector<uint8_t> saved;
  if (!save_path.empty() && read_whole_file(save_path, saved) && saved.size() == ADF_SIZE) {
    fprintf(stderr, "using saved disk %s\n", save_path.c_str());
    img_ = std::move(saved);
  }
  for (int t = 0; t < NUM_TRACKS; ++t)
    if (raw_[size_t(t)].data.empty() || raw_[size_t(t)].sync == 0x4489) encode_dos_track(t);
  return true;
}

namespace {
// MFM encoder: each data bit is preceded by a clock bit that is 1 only
// between two 0 data bits.
struct Mfm {
  std::vector<uint8_t> out;
  uint32_t acc = 0;
  int nbits = 0;
  int last = 0;  // previous data bit
  void raw(uint16_t w) {
    for (int i = 15; i >= 0; --i) put((w >> i) & 1);
    last = w & 1;
  }
  void put(int bit) {
    acc = acc << 1 | uint32_t(bit);
    if (++nbits == 8) { out.push_back(uint8_t(acc)); acc = 0; nbits = 0; }
  }
  void bits(uint32_t v, int n) {  // MFM-encode the low n bits of v, MSB first
    for (int i = n - 1; i >= 0; --i) {
      int d = (v >> i) & 1;
      put(!d && !last ? 1 : 0);
      put(d);
      last = d;
    }
  }
};

// MFM-encode the 16 data bits (bits 30, 28, ..., 0) of an odd or even
// half-long: 32 bits on the disk.
void half_long(Mfm& m, uint32_t v) {
  for (int i = 30; i >= 0; i -= 2) m.bits((v >> i) & 1, 1);
}

uint32_t checksum(const std::vector<uint32_t>& halves) {
  uint32_t c = 0;
  for (uint32_t h : halves) c ^= h;
  return c & 0x55555555;
}
}  // namespace

void Disk::encode_dos_track(int t) {
  // Standard AmigaDOS track: per sector $AAAA $AAAA (two zero bytes), sync
  // $4489 $4489, info long, 16 label bytes, header and data checksums and
  // 512 data bytes, all odd/even encoded; then a gap of zero bytes.
  Mfm m;
  for (int s = 0; s < 11; ++s) {
    m.bits(0, 16);
    m.raw(0x4489);
    m.raw(0x4489);
    std::vector<uint32_t> hdr;  // MFM data halves for the header checksum
    uint32_t info = 0xFF000000u | uint32_t(t) << 16 | uint32_t(s) << 8 | uint32_t(11 - s);
    std::vector<uint32_t> data(128);
    for (int i = 0; i < 128; ++i) {
      size_t o = (size_t(t) * 11 + size_t(s)) * 512 + size_t(i) * 4;
      data[size_t(i)] = be32(&img_[o]);
    }
    auto halves = [](const uint32_t* l, size_t n, std::vector<uint32_t>& out) {
      for (size_t i = 0; i < n; ++i) out.push_back((l[i] >> 1) & 0x55555555);
      for (size_t i = 0; i < n; ++i) out.push_back(l[i] & 0x55555555);
    };
    halves(&info, 1, hdr);
    uint32_t label[4] = {};
    halves(label, 4, hdr);
    std::vector<uint32_t> dh;
    halves(data.data(), data.size(), dh);
    uint32_t hsum = checksum(hdr), dsum = checksum(dh);
    for (uint32_t h : hdr) half_long(m, h);
    uint32_t sums[2] = {hsum, dsum};
    std::vector<uint32_t> sh;
    halves(sums, 1, sh);
    halves(sums + 1, 1, sh);
    for (uint32_t h : sh) half_long(m, h);
    for (uint32_t h : dh) half_long(m, h);
  }
  for (int i = 0; i < 700; ++i) m.bits(0, 8);  // gap
  // Deliver it as read after the first sync word of sector 0.
  RawTrack& rt = raw_[size_t(t)];
  rt.sync = 0x4489;
  rt.data.assign(m.out.begin() + 4, m.out.end());
  rt.data.insert(rt.data.end(), m.out.begin(), m.out.begin() + 4);
}

bool Disk::load_extended(const std::vector<uint8_t>& f) {
  img_.assign(ADF_SIZE, 0);
  raw_.assign(NUM_TRACKS, {});
  if (std::memcmp(f.data(), "UAE--ADF", 8) == 0) {
    // Old format: {u16 sync, u16 length} × 160, then the track data. A
    // sync of 0 marks a track of 11 decoded AmigaDOS sectors.
    size_t off = 8 + NUM_TRACKS * 4;
    if (f.size() < off) return false;
    for (int t = 0; t < NUM_TRACKS; ++t) {
      uint16_t sync = be16(&f[8 + size_t(t) * 4]);
      uint32_t len = be16(&f[8 + size_t(t) * 4 + 2]);
      // Images in the wild may be cut short at the end; keep what is there.
      size_t n = std::min<size_t>(len, f.size() > off ? f.size() - off : 0);
      if (sync == 0) {
        std::memcpy(&img_[size_t(t) * DOS_TRACK], &f[off], std::min<size_t>(n, DOS_TRACK));
      } else {
        raw_[size_t(t)].sync = sync;
        raw_[size_t(t)].data.assign(f.begin() + long(off), f.begin() + long(off + n));
      }
      off += len;
    }
    return true;
  }
  if (std::memcmp(f.data(), "UAE-1ADF", 8) == 0) {
    // New format: u16 reserved, u16 tracks; per track {u16 reserved,
    // u16 type, u32 bytes, u32 bits}; type 0 = AmigaDOS, 1 = raw MFM
    // (starting at an arbitrary position, sync included).
    if (f.size() < 12) return false;
    int ntracks = std::min<int>(be16(&f[10]), NUM_TRACKS);
    size_t off = 12 + size_t(be16(&f[10])) * 12;
    for (int t = 0; t < ntracks && off <= f.size(); ++t) {
      const uint8_t* h = &f[12 + size_t(t) * 12];
      uint16_t type = be16(h + 2);
      uint32_t len = be32(h + 4);
      size_t n = std::min<size_t>(len, f.size() - off);
      if (type == 0) {
        std::memcpy(&img_[size_t(t) * DOS_TRACK], &f[off], std::min<size_t>(n, DOS_TRACK));
      } else {
        raw_[size_t(t)].sync = 0xFFFF;  // sync is part of the data
        raw_[size_t(t)].data.assign(f.begin() + long(off), f.begin() + long(off + n));
      }
      off += len;
    }
    return true;
  }
  return false;
}

const Disk::RawTrack* Disk::raw_track(int track) const {
  if (track < 0 || size_t(track) >= raw_.size() || raw_[size_t(track)].data.empty()) return nullptr;
  return &raw_[size_t(track)];
}

bool Disk::read(uint32_t offset, uint32_t len, uint8_t* dst) const {
  if (uint64_t(offset) + len > img_.size()) return false;
  std::memcpy(dst, img_.data() + offset, len);
  return true;
}

bool Disk::write(uint32_t offset, uint32_t len, const uint8_t* src) {
  if (uint64_t(offset) + len > img_.size()) return false;
  if (std::memcmp(img_.data() + offset, src, len) == 0) return true;
  std::memcpy(img_.data() + offset, src, len);
  dirty_ = true;
  for (uint32_t t = offset / DOS_TRACK; t <= (offset + len - 1) / DOS_TRACK && t < NUM_TRACKS; ++t)
    if (raw_[t].sync == 0x4489) encode_dos_track(int(t));
  return true;
}

void Disk::flush() {
  if (!dirty_ || save_path_.empty()) return;
  std::filesystem::path p(save_path_);
  if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
  std::string tmp = save_path_ + ".tmp";
  FILE* f = fopen(tmp.c_str(), "wb");
  if (!f) return;
  bool ok = fwrite(img_.data(), 1, img_.size(), f) == img_.size();
  ok = fclose(f) == 0 && ok;
  if (ok) std::filesystem::rename(tmp, save_path_);
  dirty_ = false;
}

// ---- OFS ----------------------------------------------------------------------

namespace {
constexpr int ROOT_BLOCK = 880;
constexpr int BS = 512;
constexpr int T_HEADER = 2, T_DATA = 8, ST_ROOT = 1, ST_USERDIR = 2, ST_FILE = -3;
constexpr int HT_SIZE = 72;
}  // namespace

uint32_t Disk::l(int block, int index) const {
  size_t o = size_t(block) * BS + size_t(index) * 4;
  if (block < 0 || o + 4 > img_.size()) return 0;
  return uint32_t(img_[o]) << 24 | uint32_t(img_[o + 1]) << 16 | uint32_t(img_[o + 2]) << 8 |
         img_[o + 3];
}

bool Disk::is_dos() const {
  return std::memcmp(img_.data(), "DOS", 3) == 0 && int32_t(l(ROOT_BLOCK, 0)) == T_HEADER &&
         int32_t(l(ROOT_BLOCK, BS / 4 - 1)) == ST_ROOT;
}

std::string Disk::entry_name(int block) const {
  size_t o = size_t(block) * BS + BS - 80;
  int n = std::min<int>(img_[o], 30);
  return std::string(reinterpret_cast<const char*>(&img_[o + 1]), size_t(n));
}

bool Disk::is_dir(int block) const {
  int32_t st = int32_t(l(block, BS / 4 - 1));
  return st == ST_ROOT || st == ST_USERDIR;
}

uint32_t Disk::file_size(int block) const { return l(block, BS / 4 - 47); }

int Disk::parent(int block) const {
  if (block == ROOT_BLOCK) return ROOT_BLOCK;
  return int(l(block, BS / 4 - 3));
}

std::vector<int> Disk::list_dir(int block) const {
  std::vector<int> out;
  for (int i = 0; i < HT_SIZE; ++i)
    for (int b = int(l(block, 6 + i)); b > 0 && b < int(img_.size() / BS);
         b = int(l(b, BS / 4 - 4)))
      out.push_back(b);
  return out;
}

int Disk::find(const std::string& path) const {
  std::string p = path;
  size_t colon = p.find(':');
  if (colon != std::string::npos) p = p.substr(colon + 1);  // device/volume name
  int dir = ROOT_BLOCK;
  size_t start = 0;
  while (start <= p.size()) {
    size_t slash = p.find('/', start);
    std::string part = p.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
    start = slash == std::string::npos ? p.size() + 1 : slash + 1;
    if (part.empty()) continue;
    if (!is_dir(dir)) return -1;
    // Hash as AmigaDOS does: case-insensitive.
    uint32_t h = uint32_t(part.size());
    for (char c : part) h = (h * 13 + uint32_t(std::toupper(uint8_t(c)))) & 0x7FF;
    int found = -1;
    for (int b = int(l(dir, 6 + int(h % HT_SIZE))); b > 0; b = int(l(b, BS / 4 - 4))) {
      std::string n = entry_name(b);
      if (n.size() == part.size() &&
          std::equal(n.begin(), n.end(), part.begin(), [](char a, char c) {
            return std::toupper(uint8_t(a)) == std::toupper(uint8_t(c));
          })) {
        found = b;
        break;
      }
    }
    if (found < 0) return -1;
    dir = found;
  }
  return dir;
}

bool Disk::read_file(int block, std::vector<uint8_t>& out) const {
  if (int32_t(l(block, 0)) != T_HEADER || int32_t(l(block, BS / 4 - 1)) != ST_FILE) return false;
  uint32_t size = file_size(block);
  out.clear();
  out.reserve(size);
  // OFS data blocks form a chain: +12 data size, +16 next data block.
  for (int b = int(l(block, 4)); b > 0 && out.size() < size; b = int(l(b, 4))) {
    if (int32_t(l(b, 0)) != T_DATA) return false;
    uint32_t n = std::min<uint32_t>(l(b, 3), 488);
    size_t o = size_t(b) * BS + 24;
    out.insert(out.end(), img_.begin() + long(o), img_.begin() + long(o + n));
  }
  out.resize(std::min<size_t>(out.size(), size));
  return out.size() == size;
}

}  // namespace amiga
