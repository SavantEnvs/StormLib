# PartStream_CheckFile asserts on the fields of an untrusted PART bitmap entry

Reproducer: `reproducer.bin` (a `fuzz_open_archive` input: config byte `0x02` = `part-file:`, then a PART file).
Run `/mayhem/fuzz_open_archive reproducer.bin` (assertion aborts print no frames under `-standalone`).

```
src/FileStream.cpp:1586: DWORD PartStream_CheckFile(TBlockStream *): Assertion `FileBitmap->LargeValueLo == 0' failed.
    PartStream_CheckFile  src/FileStream.cpp:1586
    PartStream_LoadBitmap src/FileStream.cpp:1651
    PartStream_Open       src/FileStream.cpp:1928
```

**Cause:** `PartStream_LoadBitmap` reads the block map straight from the file and `PartStream_CheckFile` checks
each `PART_FILE_MAP_ENTRY` with `assert(LargeValueHi == 0)`, `assert(LargeValueLo == 0)` and
`assert(Flags == 0 || Flags == 3)` (`src/FileStream.cpp:1585-1587`). Those are file contents, not invariants.
All three asserts are this one root cause.

**Impact:** abort (denial of service) when a debug or assert-enabled build opens an untrusted `part-` archive. A
release build ignores the fields.

**Fix:** reject the bitmap (return `false` from `PartStream_LoadBitmap`) instead of asserting.
