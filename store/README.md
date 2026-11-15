# Store

Status: **draft** (2026-11-15). Module `symploke::store`, [decision 0003](../development/decisions/0003-the-first-modules.md).

## 1. Concepts

A dataset in memory, its terms numbered and its statements indexed SPO, POS and OSP per graph. Every change returns the change that undoes it, and two datasets can be compared.

## 2. Content

- `store.hpp`: `Dataset` (`insert`, `erase`, `apply`, `match`, `contains`, `graphs`, `triples`, `quads`), `Change`, `Pattern`, `Diff`, `diff`.

## 3. Checks

`check_symploke_store` (6): every pattern against filtering on random datasets; undo; refusals; differences.
