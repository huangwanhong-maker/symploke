// Graphs the same up to the names of their blank nodes: isomorphism, and canonical labels.
//
// Two RDF graphs are isomorphic when a bijection of their blank nodes carries one onto the other, IRIs and literals fixed
// (RDF 1.1 Concepts, §3.6). Blank-node labels are local to a document, so two documents that say the same thing may label
// their blank nodes differently, and only isomorphism tells them equal.
//
// The ground statements (no blank node) must coincide as sets. The others form a labelled graph whose canonical form is
// morphe's (individualisation and refinement, McKay and Piperno 2014). Each statement is a vertex labelled by its predicate,
// joined to its subject by an edge labelled s and to its object by an edge labelled o; each blank node is a vertex labelled
// "_"; each IRI or literal is a vertex labelled by its N-Triples form. Two graphs are isomorphic exactly when these labelled
// graphs are, since a vertex's label fixes every term but the blank nodes and the edge labels keep each statement's
// direction. canonicalise() renames the blank nodes c0, c1, … in morphe's canonical order of their vertices, so that
// isomorphic graphs get the same statements.
//
// Where refinement leaves cells too large to order exactly (highly symmetric graphs of blank nodes), morphe reports its
// form inexact. The answer is then undecided, and says so, rather than wrong.

#pragma once

#include <symploke/terms/terms.hpp>

#include <apodeixis/kernel/verdict.hpp>
#include <morphe/combinatorics/canonical.hpp>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace symploke::canonical {

using namespace symploke::terms;

namespace detail {

struct Encoded {
  morphe::combinatorics::LabelledGraph graph;
  std::map<Term, std::size_t> blank_vertex;
  std::set<Triple> ground;
};

inline Encoded encode(const std::vector<Triple>& triples) {
  Encoded e;
  std::map<Term, std::size_t> vertex;
  const auto vertex_of = [&](const Term& t) {
    const auto it = vertex.find(t);
    if (it != vertex.end()) return it->second;
    const std::size_t v = e.graph.vertex.size();
    e.graph.vertex.push_back(t.is_blank() ? "_" : "term " + nt(t));
    vertex[t] = v;
    if (t.is_blank()) e.blank_vertex[t] = v;
    return v;
  };
  std::set<Triple> seen;
  for (const auto& t : triples) {
    if (!seen.insert(t).second) continue;
    if (!t.s.is_blank() && !t.o.is_blank()) {
      e.ground.insert(t);
      continue;
    }
    const std::size_t st = e.graph.vertex.size();
    e.graph.vertex.push_back("statement " + nt(t.p));
    e.graph.edges.emplace_back(st, vertex_of(t.s), "s");
    e.graph.edges.emplace_back(st, vertex_of(t.o), "o");
  }
  return e;
}

}  // namespace detail

struct Canonical {
  std::vector<Triple> triples;  // blank nodes renamed c0, c1, …, sorted
  bool exact = true;
};

inline Canonical canonicalise(const std::vector<Triple>& triples, std::size_t limit = 40320) {
  const auto e = detail::encode(triples);
  Canonical c;
  std::map<Term, Term> rename;
  if (!e.blank_vertex.empty()) {
    const auto form = morphe::combinatorics::canonical_form(e.graph, limit);
    c.exact = form.exact;
    std::vector<std::size_t> position(e.graph.vertex.size());
    for (std::size_t k = 0; k < form.order.size(); k++) position[form.order[k]] = k;
    std::vector<std::pair<std::size_t, Term>> blanks;
    for (const auto& [t, v] : e.blank_vertex) blanks.push_back({position[v], t});
    std::sort(blanks.begin(), blanks.end());
    for (std::size_t k = 0; k < blanks.size(); k++) rename[blanks[k].second] = blank("c" + std::to_string(k));
  }
  std::set<Triple> out;
  for (const auto& t : triples) {
    Triple r = t;
    if (r.s.is_blank()) r.s = rename.at(r.s);
    if (r.o.is_blank()) r.o = rename.at(r.o);
    out.insert(r);
  }
  c.triples.assign(out.begin(), out.end());
  return c;
}

// Whether two graphs are isomorphic: a verdict, with the statement that differs when they are not.
inline apodeixis::kernel::Verdict isomorphic(const std::vector<Triple>& a, const std::vector<Triple>& b, std::size_t limit = 40320) {
  using apodeixis::kernel::Verdict;
  const std::string claim = "the two graphs are the same up to the names of their blank nodes";
  const auto ea = detail::encode(a), eb = detail::encode(b);
  for (const auto& t : ea.ground)
    if (!eb.ground.count(t)) return Verdict::no(claim, "only the first holds " + nt(t.s) + " " + nt(t.p) + " " + nt(t.o));
  for (const auto& t : eb.ground)
    if (!ea.ground.count(t)) return Verdict::no(claim, "only the second holds " + nt(t.s) + " " + nt(t.p) + " " + nt(t.o));
  if (ea.blank_vertex.size() != eb.blank_vertex.size())
    return Verdict::no(claim, "they have " + std::to_string(ea.blank_vertex.size()) + " and " + std::to_string(eb.blank_vertex.size()) + " blank nodes");
  if (ea.graph.edges.size() != eb.graph.edges.size()) return Verdict::no(claim, "they have different numbers of statements with blank nodes");
  const auto fa = morphe::combinatorics::canonical_form(ea.graph, limit), fb = morphe::combinatorics::canonical_form(eb.graph, limit);
  if (!fa.exact || !fb.exact) {
    if (fa.code != fb.code) return Verdict::no(claim, "their stable partitions differ, which isomorphic graphs' never do");
    return Verdict::unknown(claim, "the blank nodes are too symmetric to order within the limit; their stable partitions agree");
  }
  if (fa.code != fb.code) return Verdict::no(claim, "no renaming of the blank nodes carries one onto the other (their canonical forms differ)");
  return Verdict::yes(claim, "their ground statements coincide and their canonical forms are equal");
}

}  // namespace symploke::canonical
