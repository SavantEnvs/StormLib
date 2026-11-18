# BuildFileTableFromBlockTable asserts HiBlockTablePos64 == 0 on a cut archive

Reproducer: `reproducer.bin` (a `fuzz_open_archive` input: config byte, then the archive bytes).
Run `/mayhem/fuzz_open_archive-standalone reproducer.bin`.

```
src/SBaseFileTable.cpp:994: BuildFileTableFromBlockTable: Assertion `pHeader->HiBlockTablePos64 == 0' failed.
```

**Cause:** when the hash or block table is cut (`MPQ_FLAG_HASH_TABLE_CUT` / `MPQ_FLAG_BLOCK_TABLE_CUT`) the function `assert`s that the header has no hi-block table. The header is untrusted: the reproducer is a v2-format header (44 bytes) with `HiBlockTablePos64 = 5` whose tables run past the end of the file, which sets the cut flags and reaches the assert.

**Impact:** denial of service (abort) when opening an untrusted archive in an assert-enabled build.

**Fix:** fail the open with `ERROR_FILE_CORRUPT` (or ignore the hi-block table) instead of asserting.
