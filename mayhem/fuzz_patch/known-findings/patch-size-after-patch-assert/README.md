# ApplyFilePatch asserts that the patched size fits the preallocated buffers

Reproducer: `reproducer.bin` (a `fuzz_patch` input: a full patch MPQ, or a bare patch file).
Run `/mayhem/fuzz_patch-standalone reproducer.bin`.

```
src/SFilePatchArchives.cpp:378: ApplyFilePatch: Assertion `pFullPatch->dwSizeAfterPatch <= pPatcher->cbMaxFileData' failed.
```

**Cause:** `Patch_Process` allocates the two work buffers from the largest `dwFileSize` in the patch chain (`cbMaxFileData`), but `dwSizeAfterPatch` is read from the patch header, so a patch that claims a bigger result than any file in the chain reaches the `assert` in `ApplyFilePatch`.

**Impact:** denial of service (abort) when reading a patched file of an untrusted patch MPQ in an assert-enabled build.

**Fix:** return `ERROR_FILE_CORRUPT` when `dwSizeAfterPatch > cbMaxFileData`.
