# Bundled bzip2: signed overflow while accumulating a RUNA/RUNB run length

Reproducer: `reproducer.bin` (a `fuzz_decompress` input: 2-byte output size, then a BZIP2 sector).
Run `/mayhem/fuzz_decompress-standalone reproducer.bin`.

```
src/bzip2/decompress.c:384:48: runtime error: signed integer overflow: 1073742206 + 1073741824 cannot be represented in type 'int'
```

**Cause:** the bundled bzip2 is the 1.0.6 decoder. `es = es + (1+1) * N; N = N * 2;` has no bound on the length of a
RUNA/RUNB run, so a stream of thousands of run symbols overflows `es` (and `N`). Mayhem reports the three
statements separately (`decompress.c:384`, `:385`, `:386`); they are one root cause. This is the bug upstream bzip2 fixed in
1.0.7 / 1.0.8.

**Impact:** undefined behaviour (signed overflow) in `SCompDecompress2` on any BZIP2-compressed sector from an untrusted
archive; with the wrapped value the decoder goes on to size its output check from garbage.

**Fix:** as in bzip2 1.0.8, inside the loop add `if (N >= 2*1024*1024) RETURN(BZ_DATA_ERROR);` before `N = N * 2`.
