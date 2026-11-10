// Machine: memory map, boot, scheduler and 68k<->host call glue.
#include "amiga/machine.hpp"
#include "amiga/hle.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <fstream>

namespace amiga {

Machine::Machine()
    : chip_(CHIP_SIZE), slow_(SLOW_SIZE), fb_(FB_W * FB_H), view_(FB_W * VIEW_H) {
  ram = chip_.data();
  ram_size = CHIP_SIZE;
  cpu.bus = this;
  cpu.line_a = [this](m68k::Cpu&, uint16_t op) { return line_a(op); };
  cpu.fatal = [this](m68k::Cpu& c, int vec) {
    log("FATAL: exception vector %d at pc=%06x (op_pc=%06x)\n", vec, c.pc, c.op_pc);
    for (int i = 0; i < 8; ++i) log("  d%d=%08x a%d=%08x\n", i, c.d[i], i, c.a[i]);
    throw QuitRequested{};
  };
  hle_ = std::make_unique<Hle>(*this);
  // T2_WATCHPC=hexaddr: log every change of a byte with the PC of the
  // instruction that made it (debugging; slows the interpreter down).
  if (const char* w = getenv("T2_WATCHPC")) watch_pc_addr_ = uint32_t(strtoul(w, nullptr, 16));
}

Machine::~Machine() = default;

void Machine::log(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

std::string Machine::cstr(uint32_t a, size_t max) {
  std::string s;
  for (size_t i = 0; i < max; ++i) {
    char ch = char(read8(a + uint32_t(i)));
    if (!ch) break;
    s += ch;
  }
  return s;
}

// ---- memory allocator ---------------------------------------------------------

void Machine::add_free(uint32_t addr, uint32_t size) { free_list_[addr] = size; }

uint32_t Machine::alloc(uint32_t size, bool chip, bool clear) {
  // Like exec with an A500 trapdoor expansion: the $C00000 RAM has a higher
  // priority than chip RAM, so requests that do not need chip RAM are
  // served from there first. (The "Hostages" release depends on this: part
  // I leaves its buffers allocated, and part II needs the chip RAM.)
  if (!chip)
    if (uint32_t a = alloc_in(size, SLOW_BASE, SLOW_BASE + SLOW_SIZE, clear)) return a;
  return alloc_in(size, 0, CHIP_SIZE, clear);
}

uint32_t Machine::alloc_in(uint32_t size, uint32_t lo, uint32_t hi, bool clear) {
  size = (size + 7) & ~7u;
  if (size == 0) size = 8;
  for (auto it = free_list_.begin(); it != free_list_.end(); ++it) {
    if (it->second < size) continue;
    if (it->first < lo || it->first + size > hi) continue;
    uint32_t a = it->first;
    uint32_t rest = it->second - size;
    free_list_.erase(it);
    if (rest) free_list_[a + size] = rest;
    if (clear)
      for (uint32_t i = 0; i < size; i += 4) write32(a + i, 0);
    hle_->update_memlist();
    return a;
  }
  return 0;
}

void Machine::free(uint32_t addr, uint32_t size) {
  if (!addr) return;
  addr &= ~7u;
  size = (size + 7) & ~7u;
  if (size == 0) size = 8;
  auto [it, ok] = free_list_.emplace(addr, size);
  if (!ok) { log("FreeMem: double free %06x\n", addr); return; }
  auto next = std::next(it);
  if (next != free_list_.end() && it->first + it->second == next->first) {
    it->second += next->second;
    free_list_.erase(next);
  }
  if (it != free_list_.begin()) {
    auto prev = std::prev(it);
    if (prev->first + prev->second == it->first) {
      prev->second += it->second;
      free_list_.erase(it);
    }
  }
  hle_->update_memlist();
}

uint32_t Machine::avail(bool chip, bool largest, bool fast) const {
  uint32_t total = 0;
  for (auto& [a, s] : free_list_) {
    if (chip && a >= CHIP_SIZE) continue;
    if (fast && a < CHIP_SIZE) continue;
    total = largest ? std::max(total, s) : total + s;
  }
  return total;
}

// ---- boot ------------------------------------------------------------------

bool Machine::boot(const std::string& adf_path, const std::string& save_path) {
  if (!disk.load(adf_path, save_path)) return false;
  const auto& img = disk.image();
  if (bootdiag_) {
    unsigned custom_tracks = 0;
    unsigned turrican_sync_tracks = 0;
    for (int t = 0; t < Disk::NUM_TRACKS; ++t) {
      const auto* track = disk.raw_track(t);
      if (track && track->sync != 0x4489) {
        ++custom_tracks;
        if (track->sync == 0x9521) ++turrican_sync_tracks;
      }
    }
    log("[bootdiag] disk: source=%s custom_tracks=%u sync_9521_tracks=%u "
        "AmigaDOS_root=%d boot_DOS=%d\n",
        adf_path.c_str(), custom_tracks, turrican_sync_tracks,
        int(disk.is_dos()), int(std::memcmp(img.data(), "DOS", 3) == 0));
    if (!custom_tracks)
      log("[bootdiag] WARNING: no custom raw-MFM tracks found. A standard "
          "ADF cannot preserve the original protected Turrican II tracks; "
          "some alternate versions may use a different loader.\n");
  }
  if (std::memcmp(img.data(), "DOS", 3) != 0) {
    log("%s: not a bootable disk\n", adf_path.c_str());
    return false;
  }
  if (!coverage_path.empty()) executed_.assign(CHIP_SIZE, 0);
  hle_->boot();
  if (bootdiag_)
    log("[bootdiag] CPU start: pc=%06x supervisor=%d cycles=%lld\n",
        cpu.pc, int(cpu.s), (long long)cpu.cycles);
  return true;
}

bool Machine::dump_ram(const std::string& path) const {
  FILE* f = fopen(path.c_str(), "wb");
  if (!f) return false;
  bool ok = fwrite(chip_.data(), 1, chip_.size(), f) == chip_.size() &&
            fwrite(slow_.data(), 1, slow_.size(), f) == slow_.size();
  return fclose(f) == 0 && ok;
}

void Machine::write_coverage() {
  if (coverage_path.empty() || executed_.empty()) return;
  FILE* f = fopen(coverage_path.c_str(), "w");
  if (!f) return;
  for (uint32_t a = 0; a < executed_.size(); ++a)
    if (executed_[a]) fprintf(f, "x %06x\n", a);
  fclose(f);
}

// ---- 68k <-> host calls ----------------------------------------------------

bool Machine::line_a(uint16_t op) {
  int lib = (op >> 8) & 0xF;
  int idx = op & 0xFF;
  if (lib == HOST_LIB) {
    switch (idx) {
      case TRAP_RETURN:
        if (call_depth_ == 0) {
          hle_->program_exit();
          return true;
        }
        throw ReturnFromCall{};
      case TRAP_BOOT_DONE:
        hle_->boot_done();
        return true;
      default:
        if (idx >= TRAP_IRQ && idx < TRAP_IRQ + 6) {
          hle_->irq(idx - TRAP_IRQ + 1);
          return true;
        }
        return false;
    }
  }
  hle_->call(lib, idx * 6);
  return true;
}

uint32_t Machine::call68k(uint32_t addr, const uint32_t* d, const uint32_t* a) {
  m68k::Cpu::Regs saved = cpu.save();
  if (d) for (int i = 0; i < 8; ++i) cpu.d[i] = d[i];
  if (a) for (int i = 0; i < 7; ++i) cpu.a[i] = a[i];
  // Leave some room below the interrupted code's stack.
  cpu.a[7] = (saved.a[7] - 64) & ~1u;
  cpu.push32(hle_->ret_stub);
  cpu.pc = addr;
  cpu.stopped = false;
  ++call_depth_;
  try {
    for (;;) {
      execute(line_start_ + line_cycles_);
      if (cpu.cycles >= line_start_ + line_cycles_) run_line();
    }
  } catch (ReturnFromCall&) {
  } catch (...) {
    --call_depth_;
    throw;
  }
  --call_depth_;
  uint32_t ret = cpu.d[0];
  last_call_z_ = cpu.fz;
  cpu.restore(saved);
  return ret;
}

// ---- scheduler -------------------------------------------------------------

void Machine::sync_cias() {
  int64_t elapsed = cpu.cycles - cia_sync_;
  if (elapsed <= 0) return;
  cia_sync_ = cpu.cycles;
  // The CIAs run in real (A500) time whatever the CPU speed setting is.
  eclk_frac_ += double(elapsed) * ECLOCK_PER_LINE / double(line_cycles_);
  uint32_t n = uint32_t(eclk_frac_);
  if (!n) return;
  eclk_frac_ -= n;
  if (cia_[0].tick(n)) raise(INT_PORTS);
  if (cia_[1].tick(n)) raise(INT_EXTER);
}

void Machine::keyboard_line() {
  if (key_hold_lines_ > 0) { --key_hold_lines_; return; }
  if (key_queue_.empty()) return;
  uint8_t code = key_queue_.front();
  key_queue_.pop_front();
  // The keyboard sends the code rotated left by one bit and inverted.
  cia_[0].sdr = uint8_t(~((code << 1) | (code >> 7)));
  if (cia_[0].set_flag(Cia::ICR_SP)) raise(INT_PORTS);
  // Keep each code in SDR for ~1.5 frames: the game polls it once a frame.
  key_hold_lines_ = LINES_PER_FRAME * 3 / 2;
}

void Machine::run_line() {
  // Called when the CPU has consumed a scanline worth of cycles.
  sync_cias();
  blit_sync();
  line_start_ += line_cycles_;
  audio_line();
  if (cia_[1].tod_pulse()) raise(INT_EXTER);  // CIA-B TOD counts HSYNC
  keyboard_line();
  copper_run_until(vpos_, 0xE2);  // rest of the line's copper program
  ++vpos_;
  if (vpos_ >= LINES_PER_FRAME) {
    vpos_ = 0;
    end_of_frame();
    copper_reset();  // the copper restarts from COP1LC at vertical blank
    raise(INT_VERTB);
    if (cia_[0].tod_pulse()) raise(INT_PORTS);  // CIA-A TOD counts VSYNC
  }
  begin_line();
  check_interrupts();
}

void Machine::begin_line() {
  // The display is produced line by line, interleaved with the CPU: the
  // copper runs up to the start of the line, then the line is drawn with
  // the registers as they are now (CPU writes of the previous line
  // included); the copper's later moves follow at the end of the line.
  copper_run_until(vpos_, 0x10);
  sprite_dma_line(vpos_);
  render_line(vpos_);
}

void Machine::idle_lines(int n) {
  for (int i = 0; i < n; ++i) {
    cpu.cycles = line_start_ + line_cycles_;
    run_line();
  }
}

void Machine::end_of_frame() {
  hle_->before_frame();
  std::copy(fb_.begin(), fb_.begin() + long(VIEW_H) * FB_W, view_.begin());
  if (!frame_rect_.empty()) display_rect_ = frame_rect_;
  frame_rect_ = DisplayRect{};
  ++frame_count_;
  if (bootdiag_ && (frame_count_ == 1 || frame_count_ % 50 == 0)) {
    log("[bootdiag] frame=%llu pc=%06x op_pc=%06x stopped=%d "
        "cyl=%d side=%d dsklen=%04x dsksync=%04x "
        "dma_attempts=%llu dma_misses=%llu last_track=%d last_sync=%04x "
        "dmacon=%04x bplcon0=%04x cop1=%06x intena=%04x intreq=%04x\n",
        (unsigned long long)frame_count_, cpu.pc, cpu.op_pc, int(cpu.stopped),
        cyl_, side_, cs_.dsklen_, cs_.dsksync_,
        (unsigned long long)disk_dma_attempts_,
        (unsigned long long)disk_dma_misses_,
        last_dma_track_, last_dma_sync_,
        cs_.dmacon_, cs_.bplcon0_, cs_.cop1lc_,
        cs_.intena_, cs_.intreq_);
  }
  if (ram_hash_every > 0 && frame_count_ % uint64_t(ram_hash_every) == 0) {
    uint64_t h = 1469598103934665603ull;
    for (uint8_t b : chip_) h = (h ^ b) * 1099511628211ull;
    log("[ramhash] frame %llu %016llx\n", (unsigned long long)frame_count_, (unsigned long long)h);
  }
  if (getenv("T2_COPDUMP") && frame_count_ == uint64_t(atoi(getenv("T2_COPDUMP")))) {
    for (uint32_t lc : {cs_.cop1lc_, cs_.cop2lc_})
      for (uint32_t a = lc; a < lc + 4 * 1000; a += 4) {
        log("cop %06x: %04x %04x\n", a, read16(a), read16(a + 2));
        if (read32(a) == 0xFFFFFFFE) break;
      }
  }
  if (const char* w = getenv("T2_WATCHB")) {
    // debugging: log changes of a byte, checked once per frame
    static int last = -1;
    int v = read8(uint32_t(strtoul(w, nullptr, 16)));
    if (v != last) log("[watch] frame %llu: %02x\n", (unsigned long long)frame_count_, v);
    last = v;
  }
  if (getenv("T2_SPRDUMP") && frame_count_ % 50 == 0) {
    log("[spr] frame %llu dmacon %04x bplcon2 %04x\n", (unsigned long long)frame_count_, cs_.dmacon_, cs_.bplcon2_);
    for (auto& sp : cs_.spr_)
      log("   pt %06x v %d-%d h %d st %d armed %d data %04x %04x\n", sp.pt, sp.vstart, sp.vstop,
          sp.hstart, sp.state, sp.armed, sp.data, sp.datb);
  }
  if (verbose && frame_count_ % 50 == 0)
    log("[frame %llu] pc=%06x diw %04x-%04x ddf %02x-%02x bplcon0 %04x cop1lc %06x\n",
        (unsigned long long)frame_count_, cpu.pc, cs_.diwstrt_, cs_.diwstop_, cs_.ddfstrt_,
        cs_.ddfstop_, cs_.bplcon0_, cs_.cop1lc_);
  if (host_) {
    host_->frame(view_.data());
    host_->audio(audio_buf_.data(), audio_buf_.size() / 2);
  }
  audio_buf_.clear();
}

void Machine::check_interrupts() {
  if (!(cs_.intena_ & INT_INTEN)) return;
  uint16_t pending = cs_.intena_ & cs_.intreq_ & 0x3FFF;
  if (!pending) return;
  int level;
  if (pending & INT_EXTER) level = 6;
  else if (pending & (INT_RBF | INT_DSKSYN)) level = 5;
  else if (pending & (INT_AUD0 | INT_AUD1 | INT_AUD2 | INT_AUD3)) level = 4;
  else if (pending & (INT_COPER | INT_VERTB | INT_BLIT)) level = 3;
  else if (pending & INT_PORTS) level = 2;
  else level = 1;
  if (level > cpu.imask) cpu.interrupt(level);
}

void Machine::set_pc_hook(uint32_t pc, PcHook fn) {
  for (auto& h : pc_hooks_)
    if (h.first == pc) { h.second = std::move(fn); return; }
  pc_hooks_.emplace_back(pc, std::move(fn));
}

void Machine::execute(int64_t until) {
  while (cpu.cycles < until) {
    if (cpu.stopped) { cpu.cycles = until; return; }
    uint32_t pc = cpu.pc;
    bool redirected = false;
    for (auto& [hpc, fn] : pc_hooks_)
      if (hpc == pc) {
        fn();
        redirected = cpu.pc != pc;
        break;
      }
    if (redirected) continue;
    if (!executed_.empty() && pc < CHIP_SIZE) executed_[pc] = 1;
    if (trace)
      log("%06x  %04x  d0=%08x d1=%08x a0=%08x a7=%08x sr=%04x\n", pc, read16(pc), cpu.d[0],
          cpu.d[1], cpu.a[0], cpu.a[7], cpu.sr());
    cpu.step();
    if (watch_pc_addr_) {
      uint8_t v = read8(watch_pc_addr_);
      if (v != watch_pc_val_ && watch_pc_budget_ > 0) {
        --watch_pc_budget_;
        log("[watchpc] %06x: %02x -> %02x at pc=%06x frame %llu line %d\n", watch_pc_addr_,
            watch_pc_val_, v, cpu.op_pc, (unsigned long long)frame_count_, vpos_);
      }
      watch_pc_val_ = v;
    }
  }
}

int Machine::run() {
  try {
    for (;;) {
      execute(line_start_ + line_cycles_);
      if (cpu.cycles >= line_start_ + line_cycles_) run_line();
    }
  } catch (QuitRequested&) {
  }
  disk.flush();
  write_coverage();
  return 0;
}

// ---- save states -----------------------------------------------------------
// Simple binary snapshot.  Layout (little-endian host order for scalars):
//   "T2SS" magic, u32 version=1
//   u32 chip_size, u32 slow_size, chip bytes, slow bytes
//   Cpu::Regs + int64 cycles
//   CustomState (POD)
//   Cia[2] (POD)
//   int64 line_start, line_cycles, cia_sync; double eclk_frac
//   int vpos; u64 frame_count
//   int cyl, side; u8 motor
//   u32 key_queue_len + bytes
//   u32 free_list_count + (addr,size)* 

namespace {

template <class T>
bool wr(std::ostream& o, const T& v) {
  o.write(reinterpret_cast<const char*>(&v), sizeof(T));
  return bool(o);
}
template <class T>
bool rd(std::istream& i, T& v) {
  i.read(reinterpret_cast<char*>(&v), sizeof(T));
  return bool(i);
}

}  // namespace

bool Machine::save_state(const std::string& path) const {
  std::ofstream out(path, std::ios::binary);
  if (!out) return false;
  const char magic[4] = {'T', '2', 'S', 'S'};
  uint32_t ver = 1;
  out.write(magic, 4);
  if (!wr(out, ver)) return false;
  uint32_t cs = uint32_t(chip_.size()), ss = uint32_t(slow_.size());
  if (!wr(out, cs) || !wr(out, ss)) return false;
  out.write(reinterpret_cast<const char*>(chip_.data()), cs);
  out.write(reinterpret_cast<const char*>(slow_.data()), ss);
  auto regs = cpu.save();
  if (!wr(out, regs) || !wr(out, cpu.cycles)) return false;
  if (!wr(out, cs_)) return false;
  if (!wr(out, cia_[0]) || !wr(out, cia_[1])) return false;
  if (!wr(out, line_start_) || !wr(out, line_cycles_) || !wr(out, cia_sync_) ||
      !wr(out, eclk_frac_) || !wr(out, vpos_) || !wr(out, frame_count_))
    return false;
  if (!wr(out, cyl_) || !wr(out, side_)) return false;
  uint8_t mot = motor_ ? 1 : 0;
  if (!wr(out, mot)) return false;
  uint32_t kq = uint32_t(key_queue_.size());
  if (!wr(out, kq)) return false;
  for (uint8_t k : key_queue_)
    if (!wr(out, k)) return false;
  uint32_t fl = uint32_t(free_list_.size());
  if (!wr(out, fl)) return false;
  for (auto& [a, s] : free_list_) {
    if (!wr(out, a) || !wr(out, s)) return false;
  }
  return bool(out);
}

bool Machine::load_state(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  char magic[4];
  in.read(magic, 4);
  if (magic[0] != 'T' || magic[1] != '2' || magic[2] != 'S' || magic[3] != 'S') return false;
  uint32_t ver = 0;
  if (!rd(in, ver) || ver != 1) return false;
  uint32_t cs = 0, ss = 0;
  if (!rd(in, cs) || !rd(in, ss)) return false;
  if (cs != chip_.size() || ss != slow_.size()) return false;
  in.read(reinterpret_cast<char*>(chip_.data()), cs);
  in.read(reinterpret_cast<char*>(slow_.data()), ss);
  m68k::Cpu::Regs regs{};
  if (!rd(in, regs) || !rd(in, cpu.cycles)) return false;
  cpu.restore(regs);
  if (!rd(in, cs_)) return false;
  if (!rd(in, cia_[0]) || !rd(in, cia_[1])) return false;
  if (!rd(in, line_start_) || !rd(in, line_cycles_) || !rd(in, cia_sync_) ||
      !rd(in, eclk_frac_) || !rd(in, vpos_) || !rd(in, frame_count_))
    return false;
  if (!rd(in, cyl_) || !rd(in, side_)) return false;
  uint8_t mot = 0;
  if (!rd(in, mot)) return false;
  motor_ = mot != 0;
  uint32_t kq = 0;
  if (!rd(in, kq) || kq > 256) return false;
  key_queue_.clear();
  for (uint32_t i = 0; i < kq; ++i) {
    uint8_t k = 0;
    if (!rd(in, k)) return false;
    key_queue_.push_back(k);
  }
  uint32_t fl = 0;
  if (!rd(in, fl) || fl > 10000) return false;
  free_list_.clear();
  for (uint32_t i = 0; i < fl; ++i) {
    uint32_t a = 0, s = 0;
    if (!rd(in, a) || !rd(in, s)) return false;
    free_list_[a] = s;
  }
  // Display / audio buffers are regenerated next frame.
  frame_rect_ = DisplayRect{};
  key_hold_lines_ = 0;
  return bool(in);
}

}  // namespace amiga
