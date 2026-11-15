# Canonical

Status: **draft** (2026-11-15). Module `symploke::canonical`, [decision 0003](../development/decisions/0003-the-first-modules.md).

## 1. Concepts

Graphs compared up to the names of their blank nodes, and canonical labels, on morphe's canonical forms; the answer a verdict, undecided when the symmetry exceeds a bound.

## 2. Content

- `canonical.hpp`: `canonicalise`, `Canonical`, `isomorphic`.

## 3. Checks

`check_symploke_canonical` (6): random graphs renamed and shuffled; changed copies told apart; cycles of blank nodes.
