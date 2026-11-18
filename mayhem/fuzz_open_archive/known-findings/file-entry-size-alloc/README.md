# Reading a single-unit file allocates the size its file entry claims

Reproducer: `reproducer.bin` (a `fuzz_open_archive` input: config byte `0x06` = `flat-file:`, then the archive).
Run `/mayhem/fuzz_open_archive reproducer.bin` (the libFuzzer binary: its 2 GiB malloc limit is what reports the
allocation; the `-standalone` binary has no limit and just runs on).

```
ERROR: libFuzzer: out-of-memory (malloc(4244632317))
    AllocateSectorBuffer   src/SBaseCommon.cpp:1157
    ReadMpqFileSingleUnit  src/SFileReadFile.cpp:251
    SFileReadFile          src/SFileReadFile.cpp:748
    CreatePseudoFileName   src/SFileGetFileInfo.cpp:555
```

**Cause:** for a `MPQ_FILE_SINGLE_UNIT` file the sector size is the file's whole data size, and
`AllocateSectorBuffer` calls `STORM_ALLOC(BYTE, hf->dwDataSize)` on the block table's `dwFileSize`
(`src/SBaseCommon.cpp:1157`). `ReadMpqFileSingleUnit` does the same with `dwCmpSize` (`src/SFileReadFile.cpp:270`),
and `SAttrLoadAttributes` with the `(attributes)` file size (`src/SFileAttributes.cpp:387`). None of the sizes is
checked against the archive size. The same fuzz run hit all three allocations: one root cause.

**Impact:** an unbounded allocation request (several GiB) sized from file-entry fields when reading a file from an
untrusted archive. The library checks `STORM_ALLOC` for `NULL`, so when `malloc` refuses the read fails cleanly; it shows
up as a defect only because libFuzzer caps `malloc` at 2 GiB (the `-standalone` binary exits 0). Searching for
files is enough, because `SFileGetFileName` reads unnamed files to guess an extension.

**Fix:** reject file entries whose `dwCmpSize` points past the end of the archive, and cap `dwFileSize` for
single-unit files before allocating.
