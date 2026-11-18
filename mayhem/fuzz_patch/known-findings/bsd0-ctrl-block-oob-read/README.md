# ApplyFilePatch_BSD0 reads the control block past the end of the patch

Reproducer: `reproducer.bin` (a `fuzz_patch` input: a full patch MPQ, or a bare patch file).
Run `/mayhem/fuzz_patch-standalone reproducer.bin`.

```
ERROR: AddressSanitizer: heap-buffer-overflow  READ of size 4
    ApplyFilePatch_BSD0  src/SFilePatchArchives.cpp:269:33
```

**Cause:** the BSD0 body starts with `CtrlBlockSize`, `DataBlockSize` and `NewFileSize` read from the patch, and the loop reads 12-byte control entries from `pCtrlBlock` without checking them against the size of the patch buffer (`dwPatchSize`), nor that `CtrlBlockSize` / `DataBlockSize` stay inside it.

**Impact:** heap out-of-bounds read while reading a patched file of an untrusted patch MPQ.

**Fix:** validate `sizeof(BLIZZARD_BSDIFF40_FILE) + CtrlBlockSize + DataBlockSize <= patch size` and stop the loop when the control cursor leaves the control block.
