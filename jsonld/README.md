# JSON-LD

Status: **draft** (2026-11-15). Module `symploke::jsonld`, [decision 0004](../development/decisions/0004-json-ld.md).

## 1. Concepts

JSON-LD writes linked data as JSON, so that a web page, an API or a IIIF manifest can be read as statements without a second format. A document's context maps its short names to IRIs. Expansion removes the context and writes every name in full; the expanded form is then turned into RDF. Compaction does the reverse: it puts a context back.

The module follows the W3C's *JSON-LD 1.1 Processing Algorithms and API* (2020) step by step. It handles every keyword of the language:
- contexts embedded, remote and imported;
- `@vocab`, `@base`, default languages and directions;
- compact IRIs and prefixes, and type coercion;
- containers of every kind and their combinations;
- reverse properties, `@nest` and `@included`;
- scoped contexts on properties and on types, with `@propagate`;
- protected terms.

Remote contexts are never fetched, since the library does no I/O. A caller gives them by IRI. One that is not given is refused with the API's error *loading remote context failed*, naming the IRI, so that the edge can fetch it and ask again.

## 2. Content

- `json.hpp`: JSON read with the line and column of each value and of what is wrong (RFC 8259). It is written back compact or indented, and canonically by the JSON Canonicalization Scheme (RFC 8785), which gives the lexical form of `rdf:JSON` literals.
- `jsonld.hpp`:
  - `read(text, options)`: a document's statements (API §5.1, §4.1–4.2, §5.2–5.3, §8.1–8.3), with the prefixes its context declares;
  - `from_rdf(quads)`: the expanded form of a dataset (API §8.4), with well-formed lists as `@list`;
  - `write(quads, prefixes)`: a document compacted against a context of prefixes;
  - `Error`: the API's error code (§9.4.2), the line and column, and the IRI of a missing context.

## 3. Checks

`check_symploke_jsonld` (37 checks):
- JSON read, refused and written back.
- The canonical scheme held to RFC 8785's own examples of numbers and of the order of members.
- 17 documents read into the statements pyld gives for them, up to blank-node names. pyld is Digital Bazaar's reference implementation, and every keyword above is covered.
- 12 documents refused with pyld's error codes.
- Four datasets written as pyld compacts them.
- Every document written back and read again into the same statements.

The fixtures are made by `tests/make_fixtures.py`, which runs pyld.

## 4. Limits

- Framing and flattening are not here.
- Compaction is against a context of prefixes, not against arbitrary term definitions.
- A base direction is kept in expansion and dropped in RDF, which is the API's default (`rdfDirection` null).
- A literal whose language tag is not well-formed yields no statement, since RDF cannot carry it; pyld writes it out anyway, as unparseable N-Quads.
