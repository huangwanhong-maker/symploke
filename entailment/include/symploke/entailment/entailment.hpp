// Entailment: what a graph's vocabulary of classes and properties lets one conclude, materialised, each conclusion with the
// rule and the statements it came from.
//
// A graph that says ex:Novel rdfs:subClassOf ex:Book and ex:Middlemarch a ex:Novel entails ex:Middlemarch a ex:Book.
// Rules are applied to a fixpoint (forward chaining), and every statement added keeps its derivation, so that a conclusion
// can be explained down to what was asserted (as apodeixis keeps the evidence of a verdict). The rules written are:
//
//   RDFS (RDF 1.1 Semantics, W3C Recommendation 2014, §9.2.1, the rules that conclude from a graph's own statements):
//     rdfs2  (p rdfs:domain c), (x p y)          ⇒ (x rdf:type c)
//     rdfs3  (p rdfs:range c), (x p y), y not a literal ⇒ (y rdf:type c)
//     rdfs5  (p rdfs:subPropertyOf q), (q rdfs:subPropertyOf r) ⇒ (p rdfs:subPropertyOf r)
//     rdfs7  (p rdfs:subPropertyOf q), (x p y)   ⇒ (x q y)
//     rdfs9  (c rdfs:subClassOf d), (x rdf:type c) ⇒ (x rdf:type d)
//     rdfs11 (c rdfs:subClassOf d), (d rdfs:subClassOf e) ⇒ (c rdfs:subClassOf e)
//   The axiomatic triples and the rules that type every resource as rdfs:Resource (rdfs4a, rdfs4b, rdfs8, rdfs10, rdfs12,
//   rdfs13) are not applied: they add statements true of everything, which tell a reader nothing about the graph.
//
//   OWL 2 RL, a declared part (OWL 2 Profiles, W3C Recommendation 2012, §4.3, tables 4–9):
//     prp-symp (p a owl:SymmetricProperty), (x p y) ⇒ (y p x)
//     prp-trp  (p a owl:TransitiveProperty), (x p y), (y p z) ⇒ (x p z)
//     prp-inv1/2 (p owl:inverseOf q), (x p y) ⇒ (y q x), and from q to p
//     cax-eqc1/2 (c owl:equivalentClass d) ⇒ the two as subclasses of each other
//     prp-eqp1/2 (p owl:equivalentProperty q) ⇒ the two as subproperties of each other
//     eq-sym, eq-trans (owl:sameAs symmetric and transitive)
//   and two inconsistencies, reported and not repaired: cax-dw (x a c), (x a d), (c owl:disjointWith d); and eq-diff1
//   (x owl:sameAs y), (x owl:differentFrom y).
//
// The fixpoint is reached by rounds, each applying every rule to all statements; it terminates because no rule invents a
// term. Conclusions are added to a stated graph of the store (by default the graph read), and nothing asserted is changed.

#pragma once

#include <symploke/store/store.hpp>

#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace symploke::entailment {

using namespace symploke::terms;

enum class Regime { rdfs, rdfs_owl };

struct Derivation {
  Triple conclusion;
  std::string rule;
  std::vector<Triple> premises;
};

struct Entailed {
  std::vector<Derivation> derived;           // in the order found
  std::vector<std::string> inconsistencies;  // each with the statements that make it
  std::size_t rounds = 0;
};

namespace detail {
inline Term T(const std::string& s) { return iri(s); }
}  // namespace detail

// Materialises the entailments of `graph` into `into` (default: the same graph).
inline Entailed materialise(store::Dataset& d, Regime regime = Regime::rdfs, const Term& graph = {}, std::optional<Term> into = std::nullopt, std::size_t most = 1'000'000) {
  using namespace vocabulary;
  const Term target = into ? *into : graph;
  const Term TYPE = iri(rdf_type), SUBC = iri(rdfs + "subClassOf"), SUBP = iri(rdfs + "subPropertyOf"), DOMAIN = iri(rdfs + "domain"), RANGE = iri(rdfs + "range");
  const Term SYM = iri(owl + "SymmetricProperty"), TRANS = iri(owl + "TransitiveProperty"), INV = iri(owl + "inverseOf"), EQC = iri(owl + "equivalentClass"),
             EQP = iri(owl + "equivalentProperty"), SAME = iri(owl + "sameAs"), DIFF = iri(owl + "differentFrom"), DISJ = iri(owl + "disjointWith");
  Entailed out;
  std::set<Triple> known;
  const auto all = [&]() {
    std::vector<Triple> v = d.triples(graph);
    if (target != graph)
      for (const auto& t : d.triples(target)) v.push_back(t);
    return v;
  };
  for (const auto& t : all()) known.insert(t);
  std::set<std::string> reported;
  bool changed = true;
  while (changed) {
    changed = false;
    out.rounds++;
    const auto ts = all();
    std::map<Term, std::vector<Triple>> by_p;
    for (const auto& t : ts) by_p[t.p].push_back(t);
    const auto with = [&](const Term& p) -> const std::vector<Triple>& {
      static const std::vector<Triple> none;
      const auto it = by_p.find(p);
      return it == by_p.end() ? none : it->second;
    };
    std::vector<Derivation> found;
    const auto add = [&](Triple c, std::string rule, std::vector<Triple> premises) {
      if (c.s.is_literal() || !c.p.is_iri()) return;
      if (known.count(c)) return;
      known.insert(c);
      found.push_back({std::move(c), std::move(rule), std::move(premises)});
    };
    for (const auto& dom : with(DOMAIN))
      for (const auto& t : with(dom.s)) add({t.s, TYPE, dom.o}, "rdfs2", {dom, t});
    for (const auto& rng : with(RANGE))
      for (const auto& t : with(rng.s))
        if (!t.o.is_literal()) add({t.o, TYPE, rng.o}, "rdfs3", {rng, t});
    for (const auto& a : with(SUBP))
      for (const auto& b : with(SUBP))
        if (a.o == b.s) add({a.s, SUBP, b.o}, "rdfs5", {a, b});
    for (const auto& sp : with(SUBP))
      for (const auto& t : with(sp.s)) add({t.s, sp.o, t.o}, "rdfs7", {sp, t});
    for (const auto& sc : with(SUBC))
      for (const auto& t : with(TYPE))
        if (t.o == sc.s) add({t.s, TYPE, sc.o}, "rdfs9", {sc, t});
    for (const auto& a : with(SUBC))
      for (const auto& b : with(SUBC))
        if (a.o == b.s) add({a.s, SUBC, b.o}, "rdfs11", {a, b});
    if (regime == Regime::rdfs_owl) {
      for (const auto& decl : with(TYPE)) {
        if (decl.o == SYM)
          for (const auto& t : with(decl.s)) add({t.o, t.p, t.s}, "prp-symp", {decl, t});
        if (decl.o == TRANS)
          for (const auto& a : with(decl.s))
            for (const auto& b : with(decl.s))
              if (a.o == b.s) add({a.s, decl.s, b.o}, "prp-trp", {decl, a, b});
      }
      for (const auto& inv : with(INV)) {
        for (const auto& t : with(inv.s)) add({t.o, inv.o, t.s}, "prp-inv1", {inv, t});
        for (const auto& t : with(inv.o)) add({t.o, inv.s, t.s}, "prp-inv2", {inv, t});
      }
      for (const auto& e : with(EQC)) add({e.s, SUBC, e.o}, "cax-eqc1", {e}), add({e.o, SUBC, e.s}, "cax-eqc2", {e});
      for (const auto& e : with(EQP)) add({e.s, SUBP, e.o}, "prp-eqp1", {e}), add({e.o, SUBP, e.s}, "prp-eqp2", {e});
      for (const auto& a : with(SAME)) {
        add({a.o, SAME, a.s}, "eq-sym", {a});
        for (const auto& b : with(SAME))
          if (a.o == b.s) add({a.s, SAME, b.o}, "eq-trans", {a, b});
      }
      // Inconsistencies.
      std::map<Term, std::vector<Triple>> types;
      for (const auto& t : with(TYPE)) types[t.s].push_back(t);
      for (const auto& dj : with(DISJ))
        for (const auto& [x, ts2] : types) {
          const Triple* a = nullptr;
          const Triple* b = nullptr;
          for (const auto& t : ts2) {
            if (t.o == dj.s) a = &t;
            if (t.o == dj.o) b = &t;
          }
          if (a && b) {
            const std::string why = "cax-dw: " + nt(x) + " is of " + nt(dj.s) + " and of " + nt(dj.o) + ", declared disjoint";
            if (reported.insert(why).second) out.inconsistencies.push_back(why);
          }
        }
      for (const auto& s : with(SAME))
        for (const auto& df : with(DIFF))
          if ((df.s == s.s && df.o == s.o) || (df.s == s.o && df.o == s.s)) {
            // Once per pair, whichever way round sameness was stated or entailed.
            const Term& lo = s.s < s.o ? s.s : s.o;
            const Term& hi = s.s < s.o ? s.o : s.s;
            const std::string why = "eq-diff1: " + nt(lo) + " is the same as and different from " + nt(hi);
            if (reported.insert(why).second) out.inconsistencies.push_back(why);
          }
    }
    if (!found.empty()) {
      changed = true;
      store::Change c;
      for (const auto& f : found) c.added.push_back({f.conclusion.s, f.conclusion.p, f.conclusion.o, target});
      d.apply(c);
      out.derived.insert(out.derived.end(), found.begin(), found.end());
      if (out.derived.size() > most) throw std::length_error("symploke::entailment: more than " + std::to_string(most) + " statements entailed");
    }
  }
  return out;
}

struct Explanation {
  Triple conclusion;
  std::string rule;  // "asserted" for a statement of the graph
  std::vector<Explanation> from;
};

// How a statement came to hold: its derivation, down to what was asserted.
inline Explanation explain(const Entailed& e, const Triple& t, std::size_t depth = 0) {
  if (depth > 64) return {t, "…", {}};
  for (const auto& d : e.derived)
    if (d.conclusion == t) {
      Explanation x{t, d.rule, {}};
      for (const auto& p : d.premises) x.from.push_back(explain(e, p, depth + 1));
      return x;
    }
  return {t, "asserted", {}};
}

}  // namespace symploke::entailment
