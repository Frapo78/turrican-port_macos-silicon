// The game disk: a plain 880 KB ADF or an extended ADF ("UAE--ADF" /
// "UAE-1ADF") with raw MFM tracks. Turrican II uses the old extended
// format: tracks 0-1 are AmigaDOS sectors, the rest are custom MFM tracks
// (sync $9521) that only the game's own trackloader reads via disk DMA.
// Sector-level writes (trackdisk.device) go to a copy in the save directory.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace amiga {

constexpr uint32_t ADF_SIZE = 901120;  // 80 cylinders × 2 heads × 11 sectors × 512

class Disk {
public:
  // Load the image; if `save_path` exists it is used instead (it holds the
  // disk as the game last wrote it). Writes go to save_path.
  bool load(const std::string& adf_path, const std::string& save_path);
  bool read(uint32_t offset, uint32_t len, uint8_t* dst) const;
  bool write(uint32_t offset, uint32_t len, const uint8_t* src);
  // Write the image to save_path if it was modified.
  void flush();
  // Sector view: the bytes of all AmigaDOS tracks (zero for raw tracks).
  const std::vector<uint8_t>& image() const { return img_; }

  // ---- raw tracks (disk DMA) ----------------------------------------------
  static constexpr int NUM_TRACKS = 160;  // cylinder × 2 + side
  struct RawTrack {
    uint16_t sync = 0;           // sync word preceding `data`
    std::vector<uint8_t> data;   // MFM bytes following the sync word
  };
  // The raw MFM track as the drive head would read it after the sync word
  // (AmigaDOS tracks are encoded from the sector data); nullptr if there is
  // none.
  const RawTrack* raw_track(int track) const;

  // ---- AmigaDOS (OFS) file system, read-only -----------------------------
  // True if the disk has a valid AmigaDOS root block.
  bool is_dos() const;
  // Header block of a file or directory ("s/startup-sequence", "df0:x",
  // case-insensitive); the root block for "" or ":"; -1 if not found.
  int find(const std::string& path) const;
  bool is_dir(int block) const;
  // Contents of the file whose header block is `block`.
  bool read_file(int block, std::vector<uint8_t>& out) const;
  std::string entry_name(int block) const;
  uint32_t file_size(int block) const;
  // Header blocks of the entries of a directory.
  std::vector<int> list_dir(int block) const;
  int parent(int block) const;

private:
  uint32_t l(int block, int index) const;  // big-endian long of a block
  bool load_extended(const std::vector<uint8_t>& file);
  // Encode AmigaDOS track `t` of img_ as MFM (11 sectors + gap), as it is
  // read after its first $4489 sync word.
  void encode_dos_track(int t);
  std::vector<uint8_t> img_;
  std::vector<RawTrack> raw_;  // custom tracks + MFM of the AmigaDOS tracks
  std::string save_path_;
  bool dirty_ = false;
};

}  // namespace amiga
