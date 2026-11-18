# TranslateBetTable asserts that the BET entry count fits the hash table

Reproducer: `reproducer.bin` (a `fuzz_open_archive` input: config byte, then the archive bytes).
Run `/mayhem/fuzz_open_archive-standalone reproducer.bin`.

```
src/SBaseFileTable.cpp:1832: TranslateBetTable: Assertion `pBetHeader->dwEntryCount <= ha->dwMaxFileCount' failed.
    LoadBetTable  src/SBaseFileTable.cpp:2551
    BuildFileTable_HetBet  src/SBaseFileTable.cpp:2669
```

**Cause:** `ha->dwMaxFileCount` is the classic hash table's size from the header (`src/SBaseFileTable.cpp:2584`), and
`pBetHeader->dwEntryCount` is a field of the BET table. Both come from the archive, and nothing makes them agree before
`TranslateBetTable` `assert`s `dwEntryCount <= dwMaxFileCount`. The next line (`VerifyBetHeaderSize`) is where the
header is actually validated.

**Impact:** abort when opening an untrusted archive in an assert-enabled build. It is not the same check as
`bet-table-assert` (`:1820`, a missing HET table).

**Fix:** turn the `assert` into a rejection (`return NULL`) or a clamp, so the open fails or falls back to the classic tables.
