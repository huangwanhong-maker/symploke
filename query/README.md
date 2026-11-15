# Query

Status: **draft** (2026-11-15). Module `symploke::query`, [decision 0003](../development/decisions/0003-the-first-modules.md).

## 1. Concepts

The core of SPARQL 1.1: parsing into its algebra and evaluation with its semantics, from basic patterns to aggregates and property paths.

## 2. Content

- `query.hpp`: `parse`, `run`, `Query`, `Result`, `Pattern`, `Expr`, `PathExpr`.

## 3. Checks

`check_symploke_query` (21): a library of books by hand through every construct; paths against Warshall's closure; joins against the product; refusals.
