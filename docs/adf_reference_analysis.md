# ADF technical examination — supplied reference image

This is a **metadata-only** report; no proprietary game data, code, audio,
tracks or assets are included in this repository. The binary was examined
locally using Python and its track header table.

| Property | Observed |
| --- | --- |
| Container | Old UAE extended ADF (`UAE--ADF`) |
| Actual file size | 2,166,784 bytes |
| SHA-256 | `ce37990a07089a08d41291b9705c67da99534fc470b93f1ca49cf8bed0ced4b4` |
| Table entries | 160 tracks |
| Tracks 0–1 | AmigaDOS sector tracks, sync `0x0000`, 5,632 bytes each |
| Tracks 2–159 | 158 MFM custom tracks, sync `0x9521`, 13,640 bytes each |
| Expected length from the descriptor table | 2,167,032 bytes |
| Tail shortfall | 248 bytes, entirely within the last track |
| Boot sector marker | `DOS` appears in the decoded first sector |

This layout is consistent with the original project's documented
trackloader, but it is **not proof that every level of this dump works**.
The file name indicates a bad dump, and the declared size mismatch is real.
The program intentionally allows the small final-track discrepancy during
technical identification, without silently claiming it has repaired the
missing bytes. Accepting the file is not a guarantee of gameplay compatibility.

The SHA-256 above recognizes **this exact binary copy** as a tested
reference. Other legitimate editions can have different hashes, and a copied
image can have the same hash even if the person has no license. This is a
technical version identifier, **not a legal certificate of ownership**.

No extraction or distribution of the user's proprietary game assets is
performed by the M0 native prototype. After identifying the selected ADF on
the user's Mac, it stores only a JSON manifest, not the source ADF. Real
native gameplay with original data requires a later locally running extractor
or licensed-to-redistribute resources.

## About the requested external repository

[`xianminx/Reverse-Engineering-ChatGPT`](https://github.com/xianminx/Reverse-Engineering-ChatGPT)
documents attempts to recover ChatGPT prompt instructions. It does not
supply Amiga binary analysis, ADF decoding, disassembly or decompilation
software, so no code from it is used for this game analysis.

## Reproduce locally

```bash
python3 native/tools/inspect_adf.py '/path/to/your/Turrican II.adf'
```

The tool displays only format and hash metadata. Never put an ADF or
extracted proprietary game assets into a public GitHub repository.
