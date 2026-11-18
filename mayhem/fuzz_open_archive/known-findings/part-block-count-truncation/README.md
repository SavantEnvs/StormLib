# A huge PART file size truncates the block count to 0 and the block map is read out of bounds

Reproducer: `reproducer.bin` (a `fuzz_open_archive` input: config byte `0x02` = `part-file:`, then a PART file
whose header declares a file size near 2^64).
Run `/mayhem/fuzz_open_archive-standalone reproducer.bin`.

```
ERROR: AddressSanitizer: heap-buffer-overflow  READ of size 4
    PartStream_BlockRead   src/FileStream.cpp:1773
    BlockStream_Read       src/FileStream.cpp:1065
    OpenArchiveFromStream  src/SFileOpenArchive.cpp:305
allocated (1-byte region) by
    PartStream_LoadBitmap  src/FileStream.cpp:1629
```

**Cause:** `PartStream_LoadBitmap` computes `BlockCount = (DWORD)((StreamSize + BlockSize - 1) / BlockSize)`
from the header's 64-bit `FileSizeHi:FileSizeLo`. The addition wraps and the quotient is truncated to 32 bits, so
`BlockCount` can be 0 while `StreamSize` is huge. The bitmap is allocated for 0 entries, the size check passes, and
the first read through `PartStream_BlockRead` indexes `FileBitmap[0]` past the allocation.

**Impact:** heap out-of-bounds read of attacker-placed block offsets, then reads from arbitrary file offsets, when
opening an untrusted `part-` archive.

**Fix:** reject a file size whose block count does not fit in 32 bits or that exceeds what the bitmap in the file
can describe, and reject `BlockCount == 0`.
