# Opening an archive with wSectorSize >= 32 shifts an int by too many bits

Reproducer: `reproducer.bin` (a `fuzz_patch` input: a full patch MPQ whose header has a large `wSectorSize`).
Run `/mayhem/fuzz_patch-standalone reproducer.bin`. `fuzz_open_archive` reaches the same line with any archive.

```
src/SFileOpenArchive.cpp:494:39: runtime error: shift exponent 50 is too large for 32-bit type 'int'
```

**Cause:** `ha->dwSectorSize = (0x200 << ha->pHeader->wSectorSize)` takes the 16-bit `wSectorSize` straight from the
header. The comment above it says it checks for integer overflow, but only a result of exactly 0 is rejected; an exponent of
32 or more is undefined behaviour (on x86 the count is masked, so the archive opens with a bogus sector size).

**Impact:** undefined behaviour on every open of an untrusted archive; the bogus sector size then drives sector offset
math in the file reader.

**Fix:** reject `wSectorSize >= 22` (a sector above 2 GiB cannot be valid) with `ERROR_FILE_CORRUPT` before shifting.
