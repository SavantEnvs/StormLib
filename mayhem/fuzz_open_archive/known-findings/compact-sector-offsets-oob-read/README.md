# SFileCompactArchive copies a header-claimed number of sector-offset bytes

Reproducer: `reproducer.bin` (a `fuzz_open_archive` input: config byte, then the archive bytes).
Run `/mayhem/fuzz_open_archive-standalone reproducer.bin`.

```
ERROR: AddressSanitizer: heap-buffer-overflow  READ of size 210 (__asan_memcpy)
    CopyMpqFileSectors  src/SFileCompactArchive.cpp:199:13
```

**Cause:** `CopyMpqFileSectors` sets `dwSectorOffsLen = hf->SectorOffsets[0]`, which is the first entry of the sector offset table as read from the file, and `memcpy`s that many bytes out of `hf->SectorOffsets`, whose in-memory size is `(dwSectorCount + 1)` DWORDs. A first entry larger than that reads past the heap block. Reached by the harness through a writable open followed by compaction.

**Impact:** heap out-of-bounds read (the bytes are written to the new archive) when compacting an untrusted archive.

**Fix:** clamp `dwSectorOffsLen` to `(hf->dwSectorCount + 1) * sizeof(DWORD)` or fail with `ERROR_FILE_CORRUPT`.
