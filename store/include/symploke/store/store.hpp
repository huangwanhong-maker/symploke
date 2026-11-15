// A store of RDF: a dataset held in memory with its terms numbered and its statements indexed every way, edited by changes
// that can be undone, and compared with another.
//
//   - Dictionary. Each distinct term is given a number once, so that statements are four numbers and comparisons are of
//     integers (the dictionary encoding of RDF stores: Harth and Decker 2005; Neumann and Weikum 2010, The RDF-3X engine,
//     VLDB Journal 19).
//   - Indexes. Every graph keeps its statements in three orders, subject–predicate–object, predicate–object–subject and
//     object–subject–predicate, so that a pattern with any of its positions bound is answered by one range of one index
//     (Weiss, Karras and Bernstein 2008, Hexastore, PVLDB 1, take all six; three suffice for single patterns, since every
//     set of bound positions is a prefix of one of them).
//   - Changes. An edit is a set of statements removed and a set added; applying it returns the change that undoes it, so
//     that a work keeps a history of edits each with its inverse. A statement added twice is held once, and removing one
//     not held changes nothing; the change returned records only what changed.
//   - Comparison. diff() gives what one dataset has that the other has not, statement by statement. Blank nodes are
//     compared by their labels here; canonical.hpp compares graphs up to the renaming of blank nodes.
//
// Terms of every statement added are checked (terms::fault); a statement that is not RDF is refused with the reason.

#pragma once

#include <symploke/terms/terms.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace symploke::store {

using namespace symploke::terms;

using Id = std::uint32_t;
inline constexpr Id any = 0;  // in a pattern, a position left open; no term is numbered 0

struct Change {
  std::vector<Quad> removed, added;
  bool empty() const { return removed.empty() && added.empty(); }
};

struct Pattern {
  std::optional<Term> s, p, o;
  std::optional<Term> g;  // nothing: every graph; an empty term: the default graph
};

class Dataset {
 public:
  Dataset() { terms_.push_back(Term{}); }  // number 0 is "any"; the default graph's name is numbered 1
  std::size_t size() const {
    std::size_t n = 0;
    for (const auto& [g, idx] : graphs_) n += idx.spo.size();
    return n;
  }
  std::size_t size(const Term& graph) const {
    const auto g = find(graph);
    if (!g) return 0;
    const auto it = graphs_.find(*g);
    return it == graphs_.end() ? 0 : it->second.spo.size();
  }

  // The graphs that hold statements; the default graph as an empty term.
  std::vector<Term> graphs() const {
    std::vector<Term> v;
    for (const auto& [g, idx] : graphs_)
      if (!idx.spo.empty()) v.push_back(terms_[g]);
    return v;
  }

  bool contains(const Quad& q) const {
    const auto s = find(q.s), p = find(q.p), o = find(q.o), g = find(q.g);
    if (!s || !p || !o || !g) return false;
    const auto it = graphs_.find(*g);
    return it != graphs_.end() && it->second.spo.count({*s, *p, *o});
  }

  // Applies a change and returns the change that undoes it: what was really removed, then really added.
  Change apply(const Change& c) {
    for (const auto& q : c.added)
      if (const auto f = fault(q.triple()); !f.empty()) throw std::invalid_argument("symploke::store: " + f);
    Change undo;
    for (const auto& q : c.removed)
      if (erase(q)) undo.added.push_back(q);
    for (const auto& q : c.added)
      if (insert(q)) undo.removed.push_back(q);
    return undo;
  }

  bool insert(const Quad& q) {
    const Id s = intern(q.s), p = intern(q.p), o = intern(q.o), g = intern(q.g);
    auto& idx = graphs_[g];
    if (!idx.spo.insert({s, p, o}).second) return false;
    idx.pos.insert({p, o, s});
    idx.osp.insert({o, s, p});
    return true;
  }

  bool erase(const Quad& q) {
    const auto s = find(q.s), p = find(q.p), o = find(q.o), g = find(q.g);
    if (!s || !p || !o || !g) return false;
    const auto it = graphs_.find(*g);
    if (it == graphs_.end() || !it->second.spo.erase({*s, *p, *o})) return false;
    it->second.pos.erase({*p, *o, *s});
    it->second.osp.erase({*o, *s, *p});
    return true;
  }

  // Every statement matching the pattern, in the order of the index that answers it.
  std::vector<Quad> match(const Pattern& pat) const {
    std::vector<Quad> out;
    Id s = any, p = any, o = any;
    if (pat.s && !bind(*pat.s, s)) return out;
    if (pat.p && !bind(*pat.p, p)) return out;
    if (pat.o && !bind(*pat.o, o)) return out;
    for (const auto& [g, idx] : graphs_) {
      if (pat.g) {
        const auto want = find(*pat.g);
        if (!want || *want != g) continue;
      }
      const auto emit = [&](Id a, Id b, Id c) { out.push_back({terms_[a], terms_[b], terms_[c], terms_[g]}); };
      if (s != any) {
        // SPO with s bound, and p (and o) if bound.
        auto lo = idx.spo.lower_bound({s, p, o});
        for (auto it = lo; it != idx.spo.end() && (*it)[0] == s; ++it) {
          if (p != any && (*it)[1] != p) break;
          if (o != any && (*it)[2] != o) {
            if (p != any) break;
            continue;
          }
          emit((*it)[0], (*it)[1], (*it)[2]);
        }
      } else if (p != any) {
        auto lo = idx.pos.lower_bound({p, o, 0});
        for (auto it = lo; it != idx.pos.end() && (*it)[0] == p; ++it) {
          if (o != any && (*it)[1] != o) break;
          emit((*it)[2], (*it)[0], (*it)[1]);
        }
      } else if (o != any) {
        auto lo = idx.osp.lower_bound({o, 0, 0});
        for (auto it = lo; it != idx.osp.end() && (*it)[0] == o; ++it) emit((*it)[1], (*it)[2], (*it)[0]);
      } else {
        for (const auto& t : idx.spo) emit(t[0], t[1], t[2]);
      }
    }
    return out;
  }

  std::vector<Quad> quads() const { return match({}); }
  std::vector<Triple> triples(const Term& graph = {}) const {
    std::vector<Triple> v;
    for (const auto& q : match({std::nullopt, std::nullopt, std::nullopt, graph})) v.push_back(q.triple());
    return v;
  }

  std::optional<Id> find(const Term& t) const {
    const auto it = ids_.find(key(t));
    if (it == ids_.end()) return std::nullopt;
    return it->second;
  }
  const Term& term(Id id) const { return terms_.at(id); }

 private:
  using Key = std::array<Id, 3>;
  struct Index {
    std::set<Key> spo, pos, osp;
  };
  std::vector<Term> terms_;
  std::unordered_map<std::string, Id> ids_;
  std::map<Id, Index> graphs_;

  static std::string key(const Term& t) {
    std::string k(1, static_cast<char>('0' + static_cast<int>(t.kind)));
    k += t.value;
    k += '\x1f';
    k += t.datatype;
    k += '\x1f';
    k += t.language;
    return k;
  }
  Id intern(const Term& t) {
    const auto k = key(t);
    const auto it = ids_.find(k);
    if (it != ids_.end()) return it->second;
    const Id id = static_cast<Id>(terms_.size());
    terms_.push_back(t);
    ids_.emplace(k, id);
    return id;
  }
  bool bind(const Term& t, Id& id) const {
    const auto f = find(t);
    if (!f) return false;
    id = *f;
    return true;
  }
};

struct Diff {
  std::vector<Quad> only_first, only_second;
};

inline Diff diff(const Dataset& a, const Dataset& b) {
  Diff d;
  for (const auto& q : a.quads())
    if (!b.contains(q)) d.only_first.push_back(q);
  for (const auto& q : b.quads())
    if (!a.contains(q)) d.only_second.push_back(q);
  return d;
}

}  // namespace symploke::store
