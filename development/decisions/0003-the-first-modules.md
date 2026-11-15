# 0003. The First Modules

- **Status:** Accepted, provisionally
- **Date:** 2026-11-15
- **Decided by:** Claude, within decision 0002

## 1. Decision

Seven modules, in this order:

1. `terms` (9 checks);
2. `syntax`: N-Triples, N-Quads, Turtle, TriG (11);
3. `store` (6);
4. `query`: SPARQL's core (21);
5. `entailment`: RDFS and part of OWL 2 RL (11);
6. `shapes`: SHACL Core (7);
7. `canonical`: isomorphism on morphe's canonical forms (6).

JSON-LD, SPARQL Update, RDF/XML and RDFC-1.0's exact canonical labels are planned.

## 2. Consequences

symploke has 71 checks, held to RFC 3986's examples, to documents and queries worked by hand, and to brute force (patterns of the store, joins, paths, subclass closure, isomorphism under renaming).
