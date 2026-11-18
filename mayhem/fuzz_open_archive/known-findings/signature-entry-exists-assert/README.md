# OpenArchiveFromStream asserts that the (signature) file entry exists

Reproducer: `reproducer.bin` (a `fuzz_open_archive` input: config byte, then the archive bytes).
Run `/mayhem/fuzz_open_archive-standalone reproducer.bin`.

```
src/SFileOpenArchive.cpp:552: OpenArchiveFromStream: Assertion `(pFileEntry->dwFlags & MPQ_FILE_EXISTS) != 0' failed.
```

**Cause:** `GetFileEntryLocale(ha, "(signature)", ...)` can return an entry whose `MPQ_FILE_EXISTS` flag is clear (the flags come from the block/BET table of the archive); the code then `assert`s that the flag is set instead of treating the file as absent.

**Impact:** denial of service (abort) when opening an untrusted archive in an assert-enabled build.

**Fix:** replace the `assert` with an `if` that skips the weak-signature bookkeeping when the entry does not exist.
