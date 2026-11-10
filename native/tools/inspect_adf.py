#!/usr/bin/env python3
"""Local ADF metadata inspection: no asset extraction, never uploads files.

Prints only the file's technical fingerprint and track-table counts.
A matching hash identifies a tested dump but does NOT prove legal ownership.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

MFM_MASK = 0x55555555


def decode_mfm_track(raw: bytes) -> tuple[bytes, bool]:
    """Decode payload for local metadata inspection; never writes game data."""
    raw = raw.ljust(13640, b"\xaa")
    if len(raw) < 13610:
        raise ValueError("track data too short")
    words = struct.unpack_from(">3402I", raw, 2)
    result = []
    checksum = 0
    for i in range(1700):
        odd, even = words[i * 2], words[i * 2 + 1]
        checksum ^= odd ^ even
        result.append(((odd & MFM_MASK) << 1 | (even & MFM_MASK)) & 0xffffffff)
    odd, even = words[3400:3402]
    expected = ((odd & MFM_MASK) << 1 | (even & MFM_MASK)) & 0xffffffff
    return struct.pack(">1700I", *result), (checksum & MFM_MASK) == expected


def inspect_directory(decoded: bytes) -> list[dict]:
    entries = []
    for i in range(30):
        name, track, offset, length, flags = struct.unpack_from(">4sHHII", decoded, 16 * i)
        if name == b"\0" * 4:
            continue
        if not all(32 <= byte <= 126 for byte in name):
            continue
        entries.append({"name": name.decode("ascii"), "track": track,
                        "offset": offset, "stored_bytes": length, "flags": flags})
    return entries

KNOWN = "ce37990a07089a08d41291b9705c67da99534fc470b93f1ca49cf8bed0ced4b4"


def inspect(path: Path, deep: bool = False) -> dict:
    size = path.stat().st_size
    sha = hashlib.sha256()
    with path.open("rb") as file:
        header = file.read(12)
        file.seek(0)
        while chunk := file.read(1048576):
            sha.update(chunk)
    d = {"size_bytes": size, "sha256": sha.hexdigest(), "known_reference_match": False,
         "format": "unsupported", "reason": "unrecognized container"}
    d["known_reference_match"] = d["sha256"] == KNOWN
    if size == 901120:
        d.update(format="standard-ADF", tracks=160, custom_raw_tracks=0,
                 reason="sector-only image (may lack copy-protected tracks)")
    elif header[:8] == b"UAE--ADF" and size >= 648:
        with path.open("rb") as file:
            file.seek(8)
            directory = file.read(160 * 4)
        entries = [struct.unpack_from(">HH", directory, i * 4) for i in range(160)]
        declared = 648 + sum(length for _, length in entries)
        missing = max(0, declared - size)
        d.update(format="UAE--ADF", tracks=160,
                 custom_raw_tracks=sum(sync != 0 for sync, _ in entries),
                 turrican_sync_9521_tracks=sum(sync == 0x9521 for sync, _ in entries),
                 declared_size_bytes=declared, missing_tail_bytes=missing,
                 structurally_supported=all(length > 0 for _, length in entries) and 0 <= declared - size <= 256,
                 reason="declared track table differs from file length" if missing else "track length consistent")
        if deep:
            failures = []
            directory = []
            track_pos = 648
            with path.open("rb") as file:
                for i, (sync, track_length) in enumerate(entries):
                    file.seek(track_pos)
                    data = file.read(track_length)
                    track_pos += track_length
                    if sync != 0x9521:
                        continue
                    decoded, valid = decode_mfm_track(data)
                    if not valid:
                        failures.append(i)
                    if i == 2:
                        directory = inspect_directory(decoded)
            d.update(mfm_checksum_error_tracks=failures,
                     directory_entry_count=len(directory), directory=directory)
    elif header[:8] == b"UAE-1ADF" and size >= 12:
        tracks = int.from_bytes(header[10:12], "big")
        d.update(format="UAE-1ADF", tracks=tracks, reason="track table must be examined for game compatibility")
    return d


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("adf", type=Path)
    parser.add_argument("--deep", action="store_true",
                        help="verify custom MFM track checksums and list metadata-only directory")
    args = parser.parse_args()
    if not args.adf.is_file():
        parser.error("file not found")
    print(json.dumps(inspect(args.adf, deep=args.deep), indent=2))
