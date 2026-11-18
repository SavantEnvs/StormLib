# ApplyFilePatch_BSD0 copies a control-block length out of a smaller data block

Reproducer: `reproducer.bin` (a `fuzz_patch` input: a full patch MPQ, or a bare patch file).
Run `/mayhem/fuzz_patch-standalone reproducer.bin`.

```
ERROR: AddressSanitizer: heap-buffer-overflow  READ of size 8192 (__asan_memcpy)
    ApplyFilePatch_BSD0  src/SFilePatchArchives.cpp:277:9
```

**Cause:** `memcpy(pbNewData + dwNewOffset, pDataBlock, dwAddDataLength)` uses `dwAddDataLength` from the control block, which is only checked against `dwNewSize`, never against the end of the data block, so it reads past the patch buffer (and `pDataBlock` advances past it for the following entries).

**Impact:** heap out-of-bounds read while reading a patched file of an untrusted patch MPQ; the bytes end up in the patched file.

**Fix:** check `dwAddDataLength` (and the extra-block length) against the remaining data/extra block bytes.
