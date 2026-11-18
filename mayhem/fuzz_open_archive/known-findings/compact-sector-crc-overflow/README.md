# SFileCompactArchive reads a header-claimed number of CRC bytes into a smaller buffer

Reproducer: `reproducer.bin` (a `fuzz_open_archive` input: config byte, then the archive bytes).
Run `/mayhem/fuzz_open_archive-standalone reproducer.bin`.

```
ERROR: AddressSanitizer: heap-buffer-overflow  WRITE of size 1932 (read)
    BaseFile_Read  src/FileStream.cpp:232:26
    CopyMpqFileSectors  src/SFileCompactArchive.cpp:291:17
```

**Cause:** `dwCrcLength = SectorOffsets[dwSectorCount + 1] - SectorOffsets[dwSectorCount]` comes from the file and is passed to `FileStream_Read` with `hf->SectorChksums` as the destination, which was allocated for `dwSectorCount` DWORDs (`src/SBaseCommon.cpp:1443`). A larger value overruns the heap block with file bytes.

**Impact:** heap buffer overflow (write of attacker-controlled bytes) when compacting an untrusted archive.

**Fix:** fail with `ERROR_FILE_CORRUPT` when `dwCrcLength` exceeds `dwSectorCount * sizeof(DWORD)`.
