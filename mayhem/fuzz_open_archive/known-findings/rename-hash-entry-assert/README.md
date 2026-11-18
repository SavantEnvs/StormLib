# RenameFileEntry asserts that a new hash entry can always be allocated

Reproducer: `reproducer.bin` (a `fuzz_open_archive` input: config byte, then the archive bytes).
Run `/mayhem/fuzz_open_archive-standalone reproducer.bin`.

```
src/SBaseFileTable.cpp:2257: RenameFileEntry: Assertion `hf->pHashEntry != NULL' failed.
```

**Cause:** `SFileRenameFile` frees the old hash entry, then calls `AllocateHashEntry` and `assert`s the result ("since we freed one hash entry before, this must succeed"). `AllocateHashEntry` probes from the hash of the *new* name and returns `NULL` when `FindFreeHashEntry` finds no usable slot. The reproducer's hash table has 30 entries (the header size is not validated to be a power of two), so the `& (size - 1)` probe mask only reaches some slots. Reached by the harness through a writable open followed by a rename.

**Impact:** denial of service (abort) when renaming a file in an untrusted archive in an assert-enabled build.

**Fix:** handle `NULL` by restoring the old entry and returning `ERROR_DISK_FULL`.
