# Compaction asks for sector checksums of a file that has no sector offset table

Reproducer: `reproducer.bin` (a `fuzz_open_archive` input: config byte, then the archive bytes).
Run `/mayhem/fuzz_open_archive-standalone reproducer.bin`.

```
src/SBaseCommon.cpp:1413: AllocateSectorChecksums: Assertion `hf->SectorOffsets != NULL' failed.
    CopyMpqFiles  src/SFileCompactArchive.cpp:424
    SFileCompactArchive  src/SFileCompactArchive.cpp:633
```

**Cause:** `CopyMpqFiles` calls `AllocateSectorOffsets` and then, if the block's flags have `MPQ_FILE_SECTOR_CRC`,
`AllocateSectorChecksums`. `AllocateSectorOffsets` returns `ERROR_SUCCESS` without a table for a
`MPQ_FILE_SINGLE_UNIT` file and for a file with no `MPQ_FILE_COMPRESS_MASK` bit
(`src/SBaseCommon.cpp:1249`, `:1269`). `AllocateSectorChecksums` `assert`s that the table exists. The block flags come
from the archive, so a stored or single-unit file with `MPQ_FILE_SECTOR_CRC` set aborts `SFileCompactArchive`. The
read path (`SFileReadFile`) does not reach it this way.

**Impact:** abort when compacting an untrusted archive in an assert-enabled build.

**Fix:** in `CopyMpqFiles`, call `AllocateSectorChecksums` only when `hf->SectorOffsets != NULL`, or have it return
`ERROR_SUCCESS` when there is no table (as it already does for single-unit files two lines later).
