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

KNOWN = "ce37990a07089a08d41291b9705c67da99534fc470b93f1ca49cf8bed0ced4b4"


def inspect(path: Path) -> dict:
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
    elif header[:8] == b"UAE-1ADF" and size >= 12:
        tracks = int.from_bytes(header[10:12], "big")
        d.update(format="UAE-1ADF", tracks=tracks, reason="track table must be examined for game compatibility")
    return d


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("adf", type=Path)
    args = parser.parse_args()
    if not args.adf.is_file():
        parser.error("file not found")
    print(json.dumps(inspect(args.adf), indent=2))
