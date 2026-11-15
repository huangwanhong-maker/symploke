# Linked Data: the Theory of symploke

Status: **draft** (2026-11-15). The definitions the code rests on, and the results its checks hold.

## 1. Graphs, Datasets and Their Terms

An RDF graph is a set of triples (s, p, o) with s an IRI or blank node, p an IRI, o an IRI, blank node or literal. A dataset is a default graph and a set of named graphs (RDF 1.1 Concepts, §3–4). Blank nodes are existential: a graph says that something exists standing in the stated relations, and its labels are local.

**Definition 1.1.** Graphs G and H are isomorphic when a bijection μ of their blank nodes, the identity on IRIs and literals, carries G onto H (RDF 1.1 Concepts, §3.6).

**Proposition 1.2.** Encode a graph's triples with a blank node as follows:
- a vertex per triple, labelled by its predicate;
- a vertex per blank node, labelled `_`;
- a vertex per other term, labelled by its N-Triples form;
- each triple vertex joined to its subject by an edge labelled s and to its object by an edge labelled o.

Then G ≅ H exactly when their ground triples coincide and their encodings are isomorphic as labelled graphs.

*Proof.* An isomorphism of encodings fixes every term vertex, since its label is unique to its term. It maps blank-node vertices to blank-node vertices, and triple vertices to triple vertices of the same predicate whose s- and o-neighbours correspond, which is a μ. Conversely, a μ induces an isomorphism of encodings. ∎

morphe decides the second condition by canonical forms (McKay and Piperno 2014); their canonical order gives canonical labels.

## 2. Queries

A solution mapping is a partial function from variables to terms. Two are compatible when they agree where both are defined. The algebra of SPARQL (§18):
- the join of Ω₁ and Ω₂ is {μ₁ ∪ μ₂ : compatible};
- the left join keeps μ₁ when no compatible μ₂ passes the filter;
- the union concatenates multisets;
- minus removes μ₁ compatible with some μ₂ whose domain meets its own.

Property paths are evaluated as sets of reachable pairs (§18.4). The checks hold them to Warshall's closure, and joins to the product computed apart.

## 3. Entailment

The rules of RDFS (RDF 1.1 Semantics, §9.2.1) and the declared rules of OWL 2 RL (OWL 2 Profiles, §4.3) are Horn clauses over triples.

**Proposition 3.1.** Applying them in rounds until no new triple appears reaches the least fixpoint, the closure, in finitely many rounds.

*Proof.* No rule introduces a term. So every derivable triple lies in the finite set of triples over the graph's terms, and each round strictly enlarges the set or stops. ∎

The checks hold the subclass closure to morphe's reachability. Each derived triple records one derivation, so explain() unfolds it to asserted triples. A derivation found in a round uses only triples known before it, so the unfolding terminates.

## 4. Shapes

A SHACL shape is satisfied by a focus node when each of its constraint components is (SHACL §4). A property shape's value nodes are the nodes reached by its path. Comparisons by sh:minInclusive and its kin use SPARQL's ordering, and a value that cannot be compared with the bound violates it (§4.4.1). Recursive shapes are refused, since SHACL leaves their semantics undefined (§3.4.4).

## 5. References

See the README.
