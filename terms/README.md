# Terms

Status: **draft** (2026-11-15). Module `symploke::terms`, [decision 0003](../development/decisions/0003-the-first-modules.md).

## 1. Concepts

The terms of RDF: IRIs, blank nodes and literals; triples and quads; well-formedness of IRIs, language tags and literals; prefixes that expand and compact; the values of XSD literals by which SPARQL compares; N-Triples forms with their escapes.

## 2. Content

- `terms.hpp`: `Term`, `iri`, `blank`, `literal`, `lang`, `Triple`, `Quad`, `valid_iri`, `valid_language`, `fault`, `Prefixes`, `common_prefixes`, `Value`, `value`, `nt`.

## 3. Checks

`check_symploke_terms` (9): absolute IRIs and tags; faults; prefixes; numbers across types; instants across zones; ill-typed forms; escapes.
