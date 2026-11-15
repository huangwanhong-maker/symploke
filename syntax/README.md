# Syntax

Status: **draft** (2026-11-15). Module `symploke::syntax`, [decision 0003](../development/decisions/0003-the-first-modules.md).

## 1. Concepts

N-Triples, N-Quads, Turtle and TriG, read by their grammars and written back. Relative IRIs are resolved by RFC 3986 §5.2. Anonymous nodes are labelled apart from the document's own labels, and refusals carry their line and column.

## 2. Content

- `syntax.hpp`: `Error`, `Document`, `resolve`, `read_ntriples`, `read_nquads`, `read_turtle`, `read_trig`, `write_ntriples`, `write_nquads`, `write_turtle`, `write_trig`.

## 3. Checks

`check_symploke_syntax` (11): all 41 examples of RFC 3986 §5.4; a Turtle document with every construct against 18 statements by hand; TriG graphs; N-Triples and N-Quads; writers read back; refusals.
