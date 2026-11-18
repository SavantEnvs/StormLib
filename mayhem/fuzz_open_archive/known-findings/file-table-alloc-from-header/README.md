# BuildFileTable allocates a header-sized file table with no bound

Reproducer: `reproducer.bin` (a `fuzz_open_archive` input: config byte, then the archive bytes).
Run `/mayhem/fuzz_open_archive reproducer.bin` (the libFuzzer binary: its 2 GiB malloc limit is what reports the allocation; the `-standalone` binary has no limit and just runs on).

```
ERROR: libFuzzer: out-of-memory (malloc(8264442048))
    BuildFileTable  src/SBaseFileTable.cpp:2796:22
```

**Cause:** `BuildFileTable` sizes the file table as `max(pHeader->dwBlockTableSize, ha->dwMaxFileCount)` entries (`src/SBaseFileTable.cpp:2796`). Both numbers come straight from the archive header and are not checked against the size of the archive, so a few hundred bytes of header can ask for several GiB.

**Impact:** an unbounded allocation request (several GiB) sized from header fields of an untrusted archive. The library checks `STORM_ALLOC` for `NULL`, so when `malloc` refuses the open fails cleanly; it shows up as a defect only because libFuzzer caps `malloc` at 2 GiB (the `-standalone` binary exits 0). Where the system grants the request, the process commits that much memory for a few bytes of input.

**Fix:** reject a block table or max file count that cannot fit in the archive (the table needs `dwBlockTableSize * 16` bytes of the file) before allocating, as `LoadMpqTable` callers are expected to.
