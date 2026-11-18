# Opening an archive with wSectorSize >= 32 shifts an int by too many bits

Reproducer: `reproducer.bin` (a `fuzz_open_archive` input: config byte, then the archive bytes).
Run `/mayhem/fuzz_open_archive-standalone reproducer.bin`.

```
src/SFileOpenArchive.cpp:494:39: runtime error: shift exponent 105 is too large for 32-bit type 'int'
```

**Cause:** `ha->dwSectorSize = (0x200 << ha->pHeader->wSectorSize)` takes the 16-bit `wSectorSize` straight from the header (`src/SFileOpenArchive.cpp:494`). The comment says it checks for integer overflow, but only a result of exactly 0 is rejected; an exponent of 32 or more is undefined behaviour. Same root cause as the entry of the same name under `mayhem/fuzz_patch/known-findings/`.

**Impact:** undefined behaviour on every open of an untrusted archive; the bogus sector size then drives sector offset math in the file reader.

**Fix:** reject `wSectorSize >= 22` (a sector above 2 GiB cannot be valid) with `ERROR_FILE_CORRUPT` before shifting.
