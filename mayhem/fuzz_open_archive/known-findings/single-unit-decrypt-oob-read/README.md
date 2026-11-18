# ReadMpqFileSingleUnit decrypts dwCmpSize bytes of a smaller buffer

Reproducer: `reproducer.bin` (a `fuzz_open_archive` input: config byte, then the archive bytes).
Run `/mayhem/fuzz_open_archive-standalone reproducer.bin`.

```
ERROR: AddressSanitizer: heap-buffer-overflow  READ of size 4
    DecryptMpqBlock  src/SBaseCommon.cpp:508:42
    ReadMpqFileSingleUnit  src/SFileReadFile.cpp:290:13
```

**Cause:** for an encrypted, uncompressed single-unit file the raw data is read into the sector buffer (`cbRawData = hf->dwDataSize`), but the decryption runs over `pFileEntry->dwCmpSize` bytes. A crafted entry with `dwCmpSize` larger than the file size makes `DecryptMpqBlock` walk past the buffer. Reached while the archive is opened, through `(listfile)`.

**Impact:** heap out-of-bounds read (and write: the block is decrypted in place) when opening or reading an untrusted archive.

**Fix:** decrypt `min(dwCmpSize, cbRawData)` bytes, or reject entries with `dwCmpSize != dwFileSize` for uncompressed files.
