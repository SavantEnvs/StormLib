# Bundled bzip2: signed overflow while accumulating a RUNA/RUNB run length

Reproducer: `reproducer.bin` (a `fuzz_open_archive` input: config byte, then the archive bytes).
Run `/mayhem/fuzz_open_archive-standalone reproducer.bin`.

```
src/bzip2/decompress.c:384:48: runtime error: signed integer overflow: 1073807374 + 1073741824 cannot be represented in type 'int'
```

**Cause:** the bundled bzip2 is the 1.0.6 decoder; `es = es + (1+1) * N; N = N * 2;` has no bound on the length of a RUNA/RUNB run (`src/bzip2/decompress.c:384-386`). Same root cause as the entry of the same name under `mayhem/fuzz_decompress/known-findings/`; here it is reached by reading a BZIP2-compressed file out of an archive.

**Impact:** undefined behaviour (signed overflow) when reading a BZIP2 sector of an untrusted archive.

**Fix:** as in bzip2 1.0.8, add `if (N >= 2*1024*1024) RETURN(BZ_DATA_ERROR);` before `N = N * 2`.
