# symploke

*συμπλοκή, the interweaving: in Plato's Sophist (259e), discourse arises from the weaving together of forms; a graph of linked data is woven of statements.*

symploke is a foundation library of linked data: the terms and documents of RDF, a store, queries, entailment, shapes, and graphs compared up to their blank nodes. It is one of the foundation libraries of [Noetarium, the Science and Humanity Infrastructure](https://github.com/huangwanhong-maker/Noetarium-The-Science-and-Humanity-Infrastructure), and a project in its own right.

Status: **draft** (2026-11-15). Licence: MIT. Checks: 108.

## 1. Philosophy

The digital humanities, archives and libraries describe their objects as linked data: a manuscript, its maker, its place, the event that made it, each a node, each statement a typed link. That covers CIDOC CRM, Records in Contexts, SKOS, PROV and Dublin Core. So do the sciences, with their ontologies.

symploke holds what every such work needs, and nothing of any particular vocabulary:
- documents read exactly or refused with their line and column;
- a store whose every edit can be undone;
- queries with SPARQL's semantics;
- conclusions that carry their derivations;
- shapes whose failures name their node, path, value and constraint.

Its pure core has no I/O; storing files, serving and editing are the works'.

## 2. Modules

| Module | Content | Status |
|---|---|---|
| [terms](terms/README.md) | IRIs, blank nodes, literals with datatypes and language tags; triples and quads; well-formedness; prefixes; the values of XSD literals; N-Triples forms | draft: 9 checks |
| [syntax](syntax/README.md) | N-Triples, N-Quads, Turtle and TriG read and written; RFC 3986 resolution; anonymous nodes labelled apart; refusals with line and column | draft: 11 checks |
| [store](store/README.md) | A dataset with its terms numbered and statements indexed three ways per graph; changes with their undo; differences | draft: 6 checks |
| [query](query/README.md) | SPARQL's core: SELECT, ASK, CONSTRUCT; OPTIONAL, UNION, MINUS, FILTER, BIND, VALUES, GRAPH; property paths; aggregates, GROUP BY, HAVING; ORDER BY, LIMIT, OFFSET, DISTINCT | draft: 21 checks |
| [entailment](entailment/README.md) | RDFS and a declared part of OWL 2 RL materialised to a fixpoint, each conclusion with its derivation; inconsistencies reported | draft: 11 checks |
| [shapes](shapes/README.md) | SHACL Core: targets, paths, value types, cardinality, ranges, strings, lists, pairs, logic, nesting, closed shapes; reports as RDF | draft: 7 checks |
| [canonical](canonical/README.md) | Isomorphism of graphs up to their blank nodes and canonical labels, on morphe's canonical forms; a verdict | draft: 6 checks |
| [jsonld](jsonld/README.md) | JSON-LD 1.1: JSON read with lines and columns and written canonically (RFC 8785); documents expanded and turned into RDF by the API's algorithms, every keyword handled, remote contexts given by IRI and never fetched; datasets written compacted against prefixes; the API's error codes | draft: 37 checks |
| jsonld (further) | Compaction against any context, framing and flattening | planned |
| update | SPARQL Update; FROM and FROM NAMED | planned |

## 3. Dependencies

morphe (canonical forms, reachability in the checks), apodeixis (verdicts), calx (the checks).

## 4. Building

Inside the infrastructure, `node development/tools/run-cpp.mjs foundations/symploke foundations/symploke` builds and runs the 108 checks.

## 5. References

- W3C (2014), *RDF 1.1 Concepts and Abstract Syntax*; *RDF 1.1 Semantics*; *RDF 1.1 N-Triples*, *N-Quads*, *Turtle*, *TriG*.
- W3C (2013), *SPARQL 1.1 Query Language*.
- W3C (2012), *OWL 2 Web Ontology Language Profiles*, §4.3.
- W3C (2017), *Shapes Constraint Language (SHACL)*.
- W3C (2020), *JSON-LD 1.1* and *JSON-LD 1.1 Processing Algorithms and API*.
- Rundgren, A., Jordan, B. and Erdtman, S. (2020), *RFC 8785: JSON Canonicalization Scheme (JCS)*.
- Berners-Lee, T., Fielding, R. and Masinter, L. (2005), *RFC 3986: Uniform Resource Identifier (URI): Generic Syntax*.
