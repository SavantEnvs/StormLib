# ADPCM BitShift from the stream (same root cause as fuzz_decompress)

Reproducer: `reproducer.bin` (a `fuzz_open_archive` input: config byte, then the archive bytes).
Run `/mayhem/fuzz_open_archive-standalone reproducer.bin`.

```
src/adpcm/adpcm.cpp:374:75: runtime error: shift exponent 68 is too large for 32-bit type 'int'
    Decompress_ADPCM_mono  src/SCompression.cpp:619
    SCompDecompressInternal  src/SCompression.cpp:1020
```

This is the same bug as `mayhem/fuzz_decompress/known-findings/adpcm-shift-exponent`, reached here by reading an
ADPCM-compressed file sector from an archive instead of calling `SCompDecompress2` directly. The cause, impact and fix are
in that README.
