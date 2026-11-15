# Shapes

Status: **draft** (2026-11-15). Module `symploke::shapes`, [decision 0003](../development/decisions/0003-the-first-modules.md).

## 1. Concepts

SHACL Core validation. Shapes are read from a shapes graph, every failure names its focus node, path, value and constraint component, and the report can be written as RDF.

## 2. Content

- `shapes.hpp`: `Result`, `Report`, `validate`, `report_graph`.

## 3. Checks

`check_symploke_shapes` (7): records of archival holdings, one right and one wrong in eleven ways; targets by class through subclasses and by subjects; inverse and sequential paths; or and closed shapes; conformance; the report as RDF.
