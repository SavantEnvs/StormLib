# TranslateBetTable asserts on an archive with a BET table but no HET table

Reproducer: `reproducer.bin` (a `fuzz_open_archive` input: config byte, then the archive bytes).
Run `/mayhem/fuzz_open_archive-standalone reproducer.bin`.

```
src/SBaseFileTable.cpp:1820: TranslateBetTable: Assertion `ha->pHetTable != NULL' failed.
```

**Cause:** `LoadBetTable` loads the BET table whenever the v3/v4 header gives it a position and size, and hands it to
`TranslateBetTable`, which `assert`s that `ha->pHetTable` was already loaded. A header with a BET table but a missing,
zero-sized or unreadable HET table (`LoadHetTable` returns `NULL`) therefore aborts instead of failing the open.

**Impact:** denial of service (abort) when opening an untrusted archive in an assert-enabled build.

**Fix:** replace the `assert(ha->pHetTable != NULL)` with `if(ha->pHetTable == NULL) return NULL;` (callers already treat a
`NULL` BET table as a corrupt archive).
