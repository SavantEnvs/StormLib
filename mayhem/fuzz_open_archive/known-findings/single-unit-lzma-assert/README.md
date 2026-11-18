# SCompDecompress asserts on an LZMA compression mask when reading a single-unit file

Reproducer: `reproducer.bin` (a `fuzz_open_archive` input: config byte, then the archive bytes).
Run `/mayhem/fuzz_open_archive-standalone reproducer.bin`.

```
src/SCompression.cpp:974: SCompDecompressInternal: Assertion `uCompressionMask1 != MPQ_COMPRESSION_LZMA' failed.
```

**Cause:** `ReadMpqFileSingleUnit` decompresses a file with the legacy `SCompDecompress`, which asserts that the first byte of the compressed data (the compression mask, taken from the file) is not `MPQ_COMPRESSION_LZMA` (0x12). A single-unit file whose data starts with 0x12 therefore aborts instead of failing the read.

**Impact:** denial of service (abort) when reading a file from an untrusted archive in an assert-enabled build.

**Fix:** return failure (`0`) for the LZMA mask in `SCompDecompressInternal`, or have the single-unit path use `SCompDecompress2`.
