# BuildFileTable_HetBet reads pFileFlags[] with an unchecked flag index

Reproducer: `reproducer.bin` (a `fuzz_open_archive` input: config byte, then the archive bytes).
Run `/mayhem/fuzz_open_archive-standalone reproducer.bin`.

```
ERROR: AddressSanitizer: heap-buffer-overflow  READ of size 4, 4 bytes after a 20-byte region
    BuildFileTable_HetBet  src/SBaseFileTable.cpp:2760:39
```

**Cause:** `dwFlagIndex` is read from the BET bit array (`dwBitCount_FlagIndex` bits, up to 32) and used as `pBetTable->pFileFlags[dwFlagIndex]` without comparing it with `pBetTable->dwFlagCount`.

**Impact:** heap out-of-bounds read while opening an untrusted v3/v4 archive; the value read becomes the file entry flags.

**Fix:** fail with `ERROR_FILE_CORRUPT` when `dwFlagIndex >= pBetTable->dwFlagCount`.
