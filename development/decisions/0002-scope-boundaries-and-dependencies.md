# 0002. Scope, Boundaries and Dependencies

- **Status:** Accepted
- **Date:** 2026-11-15
- **Decided by:**
  - **The request.** The user asked for RDF to be edited, stored and visualised, "because this is important for digital humanity".
  - **The choice.** Asked, the user chose a new foundation, named symploke.
  - **The design.** Claude; the infrastructure's ADR 0118.

## 1. Context

Archives and the digital humanities describe their holdings as linked data, and the infrastructure had none: Histos Harmonia's record is its own JSON, archeion's description its own structures.

## 2. Decision

1. **A foundation, symploke, on morphe, apodeixis and calx.** Linked data names no object; archives, history, law and the sciences' ontologies stand on it.
2. **Scope:** RDF's terms and documents, a store, SPARQL's core, entailment by RDFS and part of OWL 2 RL, SHACL Core, isomorphism.
3. **Pure.** Files, servers and pages are the works'.
4. **Vocabularies are data.** CIDOC CRM, RiC-O, SKOS and their kin are graphs a work loads, not code.

## 3. Alternatives Considered

- **A module of archeion.** Offered; other sciences would reach RDF only through the archive foundation.
- **The data library alone.** Offered; it would give no checked core, and nothing to the C++ works.

## 4. Consequences

symploke stands beside archeion, below the works, and does not depend on it; a work joins them (a holding's description in RDF, fixity by archeion).
