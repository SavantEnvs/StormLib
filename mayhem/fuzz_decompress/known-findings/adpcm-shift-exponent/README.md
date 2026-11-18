# ADPCM decoder shifts the step size by a byte taken from the stream

Reproducer: `reproducer.bin` (a `fuzz_decompress` input: 2-byte output size, then a compressed sector).
Run `/mayhem/fuzz_decompress-standalone reproducer.bin`.

```
src/adpcm/adpcm.cpp:374:75: runtime error: shift exponent 255 is too large for 32-bit type 'int'
```

**Cause:** `DecompressADPCM` reads `BitShift` from the second byte of the stream (`ReadByteSample(BitShift)`, `src/adpcm/adpcm.cpp:318`) and computes `StepSize >> BitShift` with no range check. The encoder only writes `CompressionLevel - 1` (0..5).

**Impact:** undefined behaviour (shift exponent >= 32) on every ADPCM-compressed sector of an untrusted archive.

**Fix:** reject `BitShift >= 32` (or anything above the encoder's range) before decoding.
