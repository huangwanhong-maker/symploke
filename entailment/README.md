# Entailment

Status: **draft** (2026-11-15). Module `symploke::entailment`, [decision 0003](../development/decisions/0003-the-first-modules.md).

## 1. Concepts

RDFS and a declared part of OWL 2 RL, materialised to a fixpoint with each conclusion's derivation; inconsistencies reported, not repaired.

## 2. Content

- `entailment.hpp`: `Regime`, `Derivation`, `Entailed`, `materialise`, `Explanation`, `explain`.

## 3. Checks

`check_symploke_entailment` (11): every RDFS rule by hand; explanations to asserted statements; idempotence; subclass closure against morphe's reachability; OWL's rules; two inconsistencies.
