# PartStream_LoadBitmap divides by a zero PART block size

Reproducer: `reproducer.bin` (a `fuzz_open_archive` input: config byte `0x07` = `part-map:`, then a PART file).
Run `/mayhem/fuzz_open_archive-standalone reproducer.bin`.

```
src/FileStream.cpp:1622:75: runtime error: division by zero
    PartStream_LoadBitmap  src/FileStream.cpp:1622
```

**Cause:** `IsPartHeader` accepts the block size when `(BlockSize & (BlockSize - 1)) == 0`, which is true for 0.
`PartStream_LoadBitmap` then computes `BlockCount = (StreamSize + BlockSize - 1) / BlockSize`.

**Impact:** SIGFPE (denial of service) when opening an untrusted `part-` archive.

**Fix:** also require `BlockSize != 0` (and a sane minimum) in `IsPartHeader`.
