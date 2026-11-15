# CLAUDE.md

This file guides Claude Code in the symploke repository.

symploke is one of the foundation libraries of [Noetarium, the Science and Humanity Infrastructure](https://github.com/huangwanhong-maker/Noetarium-The-Science-and-Humanity-Infrastructure), which includes it as the git submodule `foundations/symploke`. It is a project in its own right and must make sense without the infrastructure.

## 1. Purpose and Scope

symploke (συμπλοκή, the interweaving) is linked data:

- the terms of RDF, their well-formedness and the values of their literals;
- documents: N-Triples, N-Quads, Turtle, TriG and JSON-LD, read and written;
- a store indexed every way, edited by changes that undo;
- queries in SPARQL's core; later updates;
- entailment by RDFS and a declared part of OWL 2 RL, with derivations;
- shapes in SHACL Core, with reports;
- graphs compared up to their blank nodes.

It builds on three sibling libraries:

- **morphe**, for canonical forms of labelled graphs (and reachability in the checks);
- **apodeixis**, for verdicts;
- **calx**, for the report of its checks.

Out of scope:

| Topic | Where it belongs |
|---|---|
| Particular vocabularies and ontologies (CIDOC CRM, RiC-O, SKOS, PROV, DDI-CDI) | the infrastructure's domains and works, as data |
| Archival theory: fixity, provenance, custody | archeion |
| Logic beyond entailment rules; derivations checked | apodeixis |
| Storing files, serving, editing in a page | the infrastructure's API and works |

The boundary is [decision 0002](development/decisions/0002-scope-boundaries-and-dependencies.md).

## 2. Invariants

1. **Dependencies.** symploke depends only on morphe, apodeixis and calx among project libraries, and never on the infrastructure's domains, edges or works.
2. **No particular objects.** Nothing in symploke names an object in nature or a particular vocabulary; its examples are illustrations.
3. **Pure.** No I/O, no global state. Documents are strings a caller has read.
4. **Exact or refused.** A document that departs from its grammar, a query that uses what is not supported, a statement that is not RDF: each is refused with the reason, never guessed at or skipped.
5. **Conclusions carry their grounds.** An entailed statement keeps its rule and premises; a failed shape names its node, path, value and constraint; an isomorphism is a verdict.
6. **Truth known.** Checks are held to the standards' own examples, to answers worked by hand and to brute force, never to values from memory.
7. **Reuse.** What a sibling holds is used through its public headers, never written again here.
8. **Cited sources.** Every format, semantics and algorithm cites its standard or source.

## 3. Design Philosophy

symploke shares its governing principle with its sibling repositories: **modular, extensible, revisable**. Status vocabulary: `planned`, `seed`, `draft`, `stable`, `deprecated`.

## 4. Repository Layout

```
symploke/
├── CMakeLists.txt   the project: one header-only target symploke::<module> per module (symploke_module); the siblings added once
├── terms/ · syntax/ · store/ · query/ · entailment/ · shapes/ · canonical/ · jsonld/
├── docs/            the theory note
└── development/     decisions · ideas · release notes
```

Each module directory holds `README.md`, `include/symploke/<module>/` and `tests/check_<module>.cpp`, a check program on calx's `verification`, registered with CTest by `symploke_module`. The modules and their order are in [decision 0003](development/decisions/0003-the-first-modules.md) and [decision 0004](development/decisions/0004-json-ld.md).

## 5. Working Procedures

- Decisions are numbered records in `development/decisions/`, never rewritten; ideas are `development/ideas/YYYY-MM-DD-slug.md`.
- **Release notes.** At each commit, add to `development/release-notes/YYYY-MM-DD.md` a concise summary in plain prose of what is new; not a list of changes; not between commits.
- Before restructuring or changing an invariant, propose the change and wait for the user's agreement. Keep each change to one concept.
- The writing style, status vocabulary and the conventions for decisions, ideas and release notes are shared with every repository of the infrastructure.
- Inside the infrastructure: commit in symploke first, then the pointer in the infrastructure; siblings in dependency order (calx and morphe, then apodeixis, then symploke).

## 6. Conventions

- **Terms** compare by kind, text, datatype, then language; language tags are kept in lower case.
- **The default graph** is named by the empty term.
- **Blank nodes** keep a document's labels; anonymous ones are labelled genid0, genid1, … apart from those.
- **Writing style.** Declarative, numbered, academic headings; concepts before structure; *Rationale.*, *Remark.*, *Caveat.*; British spelling.

## 7. Technology

C++20, header-only targets `symploke::<module>`; CMake 3.20 or later; checks on calx's `verification`; GCC in WSL on the development machine. MIT-licensed.

## 8. Current State

As of 2026-11-15, eight modules are written and checked ([decision 0003](development/decisions/0003-the-first-modules.md), [decision 0004](development/decisions/0004-json-ld.md): JSON-LD; 108 checks). The repository is published at `https://github.com/huangwanhong-maker/symploke`.
