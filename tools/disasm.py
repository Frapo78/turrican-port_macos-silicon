#!/usr/bin/env python3
"""Disassemble a file from the Turrican II disk at the address it runs at.

    python3 tools/disasm.py T2.adf MAIN > main.dis          # base $C0
    python3 tools/disasm.py T2.adf RA-I --base 0x30000
    python3 tools/disasm.py T2.adf MAIN --coverage cov.txt  # mark executed
    python3 tools/disasm.py --ram game.ram --start 0x20700 --stop 0x24000

Files are unpacked (PP20) like the loader does. --coverage takes the file
written by `turrican2 --coverage FILE` ("x <hexaddr>" lines); executed
instructions get a '*' in the first column, which separates code from the
data that is interleaved with it. Needs m68k objdump (nix dev shell), or
set M68K_OBJDUMP.
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import t2disk  # noqa: E402

# Load addresses known so far (stage 2 at $60000 loads these).
BASES = {'RA-I': 0x30000, 'MAIN': 0xC0}


def find_objdump():
    for name in (os.environ.get('M68K_OBJDUMP'), 'm68k-unknown-linux-gnu-objdump',
                 'm68k-linux-gnu-objdump', 'm68k-elf-objdump'):
        if name and shutil.which(name):
            return name
    sys.exit('m68k objdump not found (use the nix dev shell or set M68K_OBJDUMP)')


def load_file(adf, name):
    _, dec = t2disk.decoded_tracks(adf)
    for n, trk, off, ln, _ in t2disk.directory(dec):
        if n.strip() == name:
            data = t2disk.file_data(dec, trk, off, ln)
            if data[:4] == b'PP20':
                data = t2disk.pp20_unpack(data, 0)
            return data
    sys.exit(f'{name}: not on the disk')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('adf', nargs='?')
    ap.add_argument('name', nargs='?')
    ap.add_argument('--ram', help='RAM dump (script "dump" command), base 0')
    ap.add_argument('--base', type=lambda s: int(s, 0))
    ap.add_argument('--start', type=lambda s: int(s, 0))
    ap.add_argument('--stop', type=lambda s: int(s, 0))
    ap.add_argument('--coverage')
    a = ap.parse_args()
    if a.ram:
        base = 0
        data = open(a.ram, 'rb').read()
    else:
        if not a.name:
            ap.error('need ADF and file name, or --ram')
        base = a.base if a.base is not None else BASES.get(a.name, 0)
        data = load_file(a.adf, a.name)
    executed = set()
    if a.coverage:
        for line in open(a.coverage):
            p = line.split()
            if len(p) == 2 and p[0] == 'x':
                executed.add(int(p[1], 16))
    with tempfile.NamedTemporaryFile(suffix='.bin') as f:
        f.write(data)
        f.flush()
        cmd = [find_objdump(), '-D', '-b', 'binary', '-m', 'm68k',
               f'--adjust-vma={base:#x}']
        if a.start is not None:
            cmd.append(f'--start-address={a.start:#x}')
        if a.stop is not None:
            cmd.append(f'--stop-address={a.stop:#x}')
        cmd.append(f.name)
        out = subprocess.run(cmd, capture_output=True, text=True, check=True).stdout
    for line in out.splitlines()[7:]:
        addr = line.split(':', 1)[0].strip()
        try:
            mark = '*' if int(addr, 16) in executed else ' '
        except ValueError:
            mark = ' '
        print(mark + line)


if __name__ == '__main__':
    main()
