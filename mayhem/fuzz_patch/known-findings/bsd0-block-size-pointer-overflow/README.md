# ApplyFilePatch_BSD0 advances its patch cursor by unchecked 64-bit block sizes

Reproducer: `reproducer.bin` (a `fuzz_patch` input: a bare BSD0 patch file whose BSDIFF40 header has
`CtrlBlockSize = DataBlockSize = 0x8000000000000000`). Run `/mayhem/fuzz_patch-standalone reproducer.bin`.

```
src/SFilePatchArchives.cpp:258:17: runtime error: pointer index expression with base 0x8000525000011964 overflowed to 0x525000011964
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior src/SFilePatchArchives.cpp:258:17
    ApplyFilePatch_BSD0 <- ApplyFilePatch <- Patch_Process <- ReadMpqFilePatchFile <- SFileReadFile
```

Mayhem files it as an out-of-bounds read (CWE-125) at the same line.

**Cause:** `ApplyFilePatch_BSD0` takes `CtrlBlockSize` and `DataBlockSize` from the BSDIFF40 header of the
patch and adds them to `pbPatchData` (lines 254 and 258) to find the data and extra blocks. Neither value
is checked against the patch buffer size (`dwSizeOfPatchData - sizeof(MPQ_PATCH_HEADER)`), and nothing
checks that the buffer even holds a whole `BLIZZARD_BSDIFF40_FILE` header first, so a large size wraps the
pointer. This is the header step that comes before the control-entry reads recorded as
`bsd0-ctrl-block-oob-read` (:269) and the data-block copy recorded as `bsd0-data-block-oob-read` (:277).
Those two crash later, inside the loop. This one crashes at the pointer arithmetic.

**Impact:** a wrapped pointer to the data and extra blocks while reading a patched file from an untrusted
patch MPQ. The loop then reads (and copies into the patched file) from wherever that pointer lands.

**Fix:** before reading the header, check `cbPatch >= sizeof(BLIZZARD_BSDIFF40_FILE)`. Then check
`CtrlBlockSize <= cbPatch - sizeof(BLIZZARD_BSDIFF40_FILE)` and
`DataBlockSize <= cbPatch - sizeof(BLIZZARD_BSDIFF40_FILE) - CtrlBlockSize` (subtracting, so the check
itself cannot overflow), and return `ERROR_FILE_CORRUPT` otherwise.
