# LoadMpqTable allocates the table size the header claims before reading it

Reproducer: `reproducer.bin` (a `fuzz_patch` input: a full patch MPQ, or a bare patch file).
Run `/mayhem/fuzz_patch reproducer.bin` (the libFuzzer binary: its 2 GiB malloc limit is what reports the allocation; the `-standalone` binary has no limit and just runs on).

```
ERROR: libFuzzer: out-of-memory (malloc(4026532096))
    LoadMpqTable  src/SBaseCommon.cpp:1009:29
```

**Cause:** `LoadHashTable` computes `dwTableSize = pHeader->dwHashTableSize * sizeof(TMPQHash)` from the header and `LoadMpqTable` calls `STORM_ALLOC(BYTE, dwTableSize)` on it (`src/SBaseCommon.cpp:1009`) before any byte of the table has been read, so the allocation is not bounded by the file size.

**Impact:** an unbounded allocation request (several GiB) sized from header fields of an untrusted archive. The library checks `STORM_ALLOC` for `NULL`, so when `malloc` refuses the open fails cleanly; it shows up as a defect only because libFuzzer caps `malloc` at 2 GiB (the `-standalone` binary exits 0). Where the system grants the request, the process commits that much memory for a few bytes of input.

**Fix:** compare `dwTableSize` (and the compressed size) with the bytes left in the stream before allocating.
