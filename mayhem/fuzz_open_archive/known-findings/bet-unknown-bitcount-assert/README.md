# TranslateBetTable asserts that the BET "unknown" bit count is zero

Reproducer: `reproducer.bin` (a `fuzz_open_archive` input: config byte, then the archive bytes).
Run `/mayhem/fuzz_open_archive-standalone reproducer.bin`.

```
src/SBaseFileTable.cpp:1859: TranslateBetTable: Assertion `pBetTable->dwBitCount_Unknown == 0' failed.
    LoadBetTable  src/SBaseFileTable.cpp:2551
    BuildFileTable_HetBet  src/SBaseFileTable.cpp:2669
```

**Cause:** `TranslateBetTable` copies `dwBitCount_Unknown` from the BET header after `VerifyBetHeaderSize` has
accepted it, then `assert`s that it is 0 ("since we don't know what the unknown is"). The field is part of the archive,
and `VerifyBetHeaderSize` allows a nonzero value, so a header that passes validation still aborts.

**Impact:** abort when opening an untrusted archive in an assert-enabled build. This is a different check from
`bet-table-assert` (`:1820`) and `bet-entry-count-assert` (`:1832`).

**Fix:** replace the `assert` with a rejection (`return NULL`, which callers treat as a corrupt BET table), or drop it
if a nonzero bit count is acceptable.
