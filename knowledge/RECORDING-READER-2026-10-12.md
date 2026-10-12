# Recording reader cost on Unity Mono

The DRIVE 20-minute diagnostic reached its real duration limit and drained all
115,534 accepted samples, but package saving exceeded its before-close deadline.
The immutable original is 796,449,634 bytes. This change reduces reader overhead;
it does not change the schema, writer, limits, completion policy or recorded data.

`SessionReader` now scans 16 KiB blocks instead of calling `ReadByte` and
`WriteByte` for every byte. Records wholly within a block need one byte-array
allocation; crossing records use bounded accumulation. A serializer belongs to
one enumeration and is reused sequentially. Strict UTF-8, JSON syntax, payload,
channel, ordering and footer validation remain in place. No trust or DTOs are
cached between records or enumerations.

The existing unpublished recording DLL's eight source hashes all match the
toolkit baseline at `0930a42` after normalizing CRLF to LF. This is a source
continuation, not a claim that the extension was in the released v0.12.0 package.

## Offline measurement

The exact installed DRIVE Unity Mono runtime hosted a filesystem-only benchmark.
It did not initialize Unity, open devices, send force or launch a game. Each
variant enumerated the original through its validated duration-limit footer:
115,534 samples, zero markers, all accepted samples written. No source was edited.

| Reader | Wall seconds | Process CPU seconds |
|---|---:|---:|
| Previous vendored reader | 97.62 | 95.66 |
| Block reader | 91.66 | 87.66 |
| Block reader + per-enumeration serializer | 88.50 | 87.44 |

These are single trials on a shared development PC with peer builds/functional
game tests, not a controlled game-frame-rate comparison. The serializer's small
incremental gain is not isolated from that noise. JSON decoding remains the
dominant cost. DRIVE separately removes repeated nested seal verification within
one package operation; this reader does not remove any verification pass.

Private evidence: hub `artifacts/drive-rc7-sealing-cost-20261012`, logs
`mono-baseline.log`, `mono-reader-block.log`, and
`mono-reader-block-serializer.log`. The original SHA-256 is
`54cd72c549326cb747b33777d9c61f954272e779fb0d9e121c885e3636488972`.

## Checks and adoption

66 recording tests pass, including new real-file block-boundary cases, split
CRLF and UTF-8, exact 1 MiB bounds (CR included, LF excluded), distinct truncated
and oversized errors, footer withholding after trailing data, and interleaved
enumerators with independent returned dictionaries. Peer source review found no
blocker. Writer/source-format and force behavior are unchanged.

Consumers must explicitly rebuild and repin the managed DLL and its provenance.
This offline result alone does not establish faster Saved status in a running
game or change any consumer's installed package.
