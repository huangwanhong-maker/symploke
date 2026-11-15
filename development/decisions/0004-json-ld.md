# 0004. JSON-LD

- **Status:** Accepted, provisionally
- **Date:** 2026-11-15
- **Decided by:** the user asked for JSON-LD among the Archive Studio's second stage (the infrastructure's ADR 0120); the design is Claude's, within decision 0002.

## 1. Context

Decision 0003 planned JSON-LD. Archives, libraries and museums exchange linked data in JSON-LD: IIIF manifests, Web Annotations, schema.org in web pages, Linked Art. A studio that edits linked data must read and write it.

## 2. Decision

1. **An eighth module, `jsonld`,** after `canonical`, on `syntax`. It has a JSON reader and writer of its own (`json.hpp`), since no sibling holds one and the library depends on none of the infrastructure's edges.
2. **Expansion and conversion to RDF follow the W3C's JSON-LD 1.1 API step by step,** with its error codes. Every keyword of the language is read.
3. **Remote contexts are given, never fetched,** as the invariant "pure" requires. One that is not given is refused with *loading remote context failed*, naming its IRI. The edge fetches it and asks again.
4. **Writing is compaction against a context of prefixes.** It is held to pyld's compaction against the same context, and to reading back into the same statements. Compaction against arbitrary term definitions, framing and flattening are planned.
5. **Checks are held to pyld,** the reference implementation, by fixtures it makes (`tests/make_fixtures.py`). Two of pyld's ways are not followed, and the fixtures say so:
   - pyld puts a base of its own in place of an empty one;
   - pyld writes literals whose language tags are not well-formed. symploke yields no statement for them, since RDF cannot carry them.

## 3. Consequences

- symploke has eight modules and 108 checks.
- The infrastructure's API reads and writes JSON-LD as a fifth format (`science::linked`).
