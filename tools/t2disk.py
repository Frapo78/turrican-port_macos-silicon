#!/usr/bin/env python3
"""Turrican II disk image tool.

The disk is an old-style UAE extended ADF ("UAE--ADF"): a table of 160
{u16 sync, u16 length} entries, then the track data. Tracks 0-1 are plain
AmigaDOS sectors (bootblock + the stage-2 loader at disk offset $400),
tracks 2-159 are raw MFM read by the loader with sync $9521.

Custom track format (loader at $60000, see docs/disk_format.md):
  13640 bytes after the sync = 1 pad word, 1700 pairs of (odd, even) longs
  giving 6800 data bytes, and one (odd, even) pair holding the checksum
  (EOR of all raw data longs, masked with $55555555).
Directory: the first 400 bytes of track 2, 25 entries of
  {char name[4]; u16 track; u16 offset; u32 length; u32 flags}.
Files are stored contiguously across tracks (6800 bytes each) and are
PowerPacker-compressed ("PP20") when flagged by the loader.

usage: t2disk.py list ADF
       t2disk.py extract ADF DIR [--raw]
       t2disk.py track ADF N OUT
"""
import os
import struct
import sys

TRACK_BYTES = 6800
MASK = 0x55555555


def read_adf(path):
    d = open(path, 'rb').read()
    if d[:8] != b'UAE--ADF':
        raise SystemExit(f'{path}: not an extended ADF')
    tracks = []
    off = 8 + 160 * 4
    for i in range(160):
        sync, ln = struct.unpack_from('>HH', d, 8 + 4 * i)
        tracks.append((sync, d[off:off + ln]))
        off += ln
    return tracks


def decode_track(raw):
    """MFM-decode one custom track; returns (data, checksum_ok)."""
    raw = raw.ljust(13640, b'\xaa')
    longs = struct.unpack_from('>%dL' % (1702 * 2), raw, 2)
    out = []
    chk = 0
    for i in range(1700):
        o, e = longs[2 * i], longs[2 * i + 1]
        chk ^= o ^ e
        out.append(((o & MASK) << 1 | (e & MASK)) & 0xFFFFFFFF)
    o, e = longs[3400], longs[3401]
    stored = ((o & MASK) << 1 | (e & MASK)) & 0xFFFFFFFF
    return struct.pack('>1700L', *out), (chk & MASK) == stored


def decoded_tracks(path):
    tracks = read_adf(path)
    res = {}
    for i, (sync, raw) in enumerate(tracks):
        if sync == 0x9521:
            res[i] = decode_track(raw)
    return tracks, res


def directory(dec):
    d = dec[2][0]
    entries = []
    for i in range(25):
        name, trk, off, ln, ul = struct.unpack_from('>4sHHLL', d, 16 * i)
        if name == b'\0\0\0\0':
            continue
        entries.append((name.decode('latin1'), trk, off, ln, ul))
    return entries


def file_data(dec, trk, off, ln):
    buf = b''
    while len(buf) < off + ln:
        buf += dec[trk][0]
        trk += 1
    return buf[off:off + ln]


def pp20_unpack(src, outlen):
    """PowerPacker decruncher (same algorithm as the loader's routine at $605ac)."""
    eff = src[4:8]
    data = src[8:]
    # last long: (unpacked size << 8) | skip bits
    last = struct.unpack('>L', data[-4:])[0]
    size = last >> 8
    skip = last & 0xFF
    out = bytearray(size)
    pos = len(data) - 4
    bitbuf = 0
    bits = 0

    def rd(n):
        nonlocal bitbuf, bits, pos
        v = 0
        for _ in range(n):
            if bits == 0:
                pos -= 4
                bitbuf = struct.unpack_from('>L', data, pos)[0]
                bits = 32
            v = (v << 1) | (bitbuf & 1)
            bitbuf >>= 1
            bits -= 1
        return v

    rd(skip)
    w = size
    while w > 0:
        if rd(1) == 0:
            n = 1
            while True:
                x = rd(2)
                n += x
                if x != 3:
                    break
            for _ in range(n):
                w -= 1
                out[w] = rd(8)
            if w <= 0:
                break
        x = rd(2)
        nb = eff[x]
        n = x + 2
        if x == 3:
            o = rd(7) if rd(1) == 0 else rd(nb)
            while True:
                y = rd(3)
                n += y
                if y != 7:
                    break
        else:
            o = rd(nb)
        for _ in range(n):
            w -= 1
            out[w] = out[w + o + 1]
    return bytes(out)


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    cmd, adf = sys.argv[1], sys.argv[2]
    tracks, dec = decoded_tracks(adf)
    bad = [i for i, (_, ok) in dec.items() if not ok]
    if cmd == 'list':
        print(f'{len(dec)} custom tracks, checksum errors: {bad or "none"}')
        for name, trk, off, ln, ul in directory(dec):
            data = file_data(dec, trk, off, ln)
            kind = data[:4].decode('latin1') if data[:4] == b'PP20' else 'raw'
            print(f'{name:4s}  track {trk:3d} +{off:5d}  {ln:7d} bytes  {ul:04x}  {kind}')
    elif cmd == 'extract':
        outdir = sys.argv[3]
        raw = '--raw' in sys.argv
        os.makedirs(outdir, exist_ok=True)
        for name, trk, off, ln, ul in directory(dec):
            data = file_data(dec, trk, off, ln)
            if not raw and data[:4] == b'PP20':
                data = pp20_unpack(data, ul)
            fn = name.strip().replace('/', '_').replace('\0', '') or 'noname'
            open(os.path.join(outdir, fn), 'wb').write(data)
            print(f'{fn}: {len(data)} bytes')
    elif cmd == 'track':
        n = int(sys.argv[3], 0)
        open(sys.argv[4], 'wb').write(dec[n][0])
    else:
        print(__doc__)
        sys.exit(1)


if __name__ == '__main__':
    main()
