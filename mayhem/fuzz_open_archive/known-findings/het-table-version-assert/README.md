# TranslateHetTable asserts on the HET table version read from the archive

Reproducer: `reproducer.bin` (a `fuzz_open_archive` input: config byte, then the archive bytes).
Run `/mayhem/fuzz_open_archive-standalone reproducer.bin`.

```
src/SBaseFileTable.cpp:1539: TranslateHetTable: Assertion `pHetHeader->ExtHdr.dwVersion == 1' failed.
    LoadHetTable  src/SBaseFileTable.cpp:2528
    LoadAnyHashTable  src/SBaseFileTable.cpp:2569
```

**Cause:** `LoadHetTable` loads the HET table whenever the v3/v4 header gives it a position and size. `LoadExtTable`
checks the signature, but not the version, and `TranslateHetTable` then `assert`s that `ExtHdr.dwVersion == 1`. The
version is a field of the archive, so any other value aborts the open. `TranslateBetTable` has the same version
`assert` at `:1819`.

**Impact:** abort when opening an untrusted archive in an assert-enabled build. It is a different check from
`bet-table-assert` (`:1820`), with a different fix.

**Fix:** replace the version `assert`s with `if(pHetHeader->ExtHdr.dwVersion != 1) return NULL;` (and the same in
`TranslateBetTable`). Callers already treat a `NULL` table as a missing or corrupt table.
