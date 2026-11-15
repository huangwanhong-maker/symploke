// Checks of the store (store.hpp): on random datasets, every pattern of bound and open positions (in every graph and in
// each) answers what filtering every statement answers; a change applied and its undo applied leave the dataset as it was;
// adding a statement twice holds it once; a statement that is not RDF is refused; the difference of two datasets.

#include <symploke/store/store.hpp>

#include <calx/random/stream.hpp>
#include <calx/verification/check.hpp>

#include <set>
#include <string>
#include <vector>

using namespace symploke::store;

int main() {
  calx::verification::Suite s;
  calx::random::Stream rng(20261115, 71, 0, 0);
  const auto term = [&](int kind) {
    const std::string n = std::to_string(rng.below(6));
    if (kind == 0) return rng.bernoulli(0.2) ? blank("b" + n) : iri("http://e/n" + n);
    if (kind == 1) return iri("http://e/p" + std::to_string(rng.below(3)));
    const double u = rng.uniform();
    return u < 0.5 ? iri("http://e/n" + n) : u < 0.8 ? literal("v" + n) : lang("w" + n, "en");
  };
  std::size_t patterns = 0, faults = 0;
  for (int trial = 0; trial < 40; trial++) {
    Dataset d;
    std::set<Quad> all;
    for (int k = 0; k < 60; k++) {
      const Quad q{term(0), term(1), term(2), rng.bernoulli(0.3) ? iri("http://e/g" + std::to_string(rng.below(2))) : Term{}};
      d.insert(q);
      all.insert(q);
    }
    if (d.size() != all.size()) faults++;
    for (int k = 0; k < 40; k++) {
      Pattern p;
      if (rng.bernoulli(0.5)) p.s = term(0);
      if (rng.bernoulli(0.5)) p.p = term(1);
      if (rng.bernoulli(0.5)) p.o = term(2);
      const int g = static_cast<int>(rng.below(4));
      if (g == 1) p.g = Term{};
      if (g == 2) p.g = iri("http://e/g0");
      std::set<Quad> want;
      for (const auto& q : all)
        if ((!p.s || q.s == *p.s) && (!p.p || q.p == *p.p) && (!p.o || q.o == *p.o) && (!p.g || q.g == *p.g)) want.insert(q);
      const auto got = d.match(p);
      if (std::set<Quad>(got.begin(), got.end()) != want || got.size() != want.size()) faults++;
      patterns++;
    }
  }
  s.require("on 40 random datasets, " + std::to_string(patterns) + " patterns of bound and open positions answer what filtering every statement answers", faults == 0, std::to_string(faults));

  Dataset d;
  const Quad a{iri("http://e/a"), iri("http://e/p"), literal("x"), {}}, b{iri("http://e/b"), iri("http://e/p"), iri("http://e/a"), iri("http://e/g")};
  d.apply({{}, {a, b}});
  const auto before = d.quads();
  const Quad c{iri("http://e/c"), iri("http://e/q"), lang("y", "fr"), {}};
  const auto undo = d.apply({{a, c}, {c, b}});  // removes a and an absent c, then adds c and an already present b
  s.require("a change records only what changed: a removed, c added", undo.added.size() == 1 && undo.added[0] == a && undo.removed.size() == 1 && undo.removed[0] == c);
  d.apply(undo);
  const auto after = d.quads();
  s.require("and its undo leaves the dataset as it was", std::set<Quad>(before.begin(), before.end()) == std::set<Quad>(after.begin(), after.end()) && d.size() == 2);
  bool refused = false;
  try { d.apply({{}, {{literal("x"), iri("http://e/p"), iri("http://e/o"), {}}}}); } catch (const std::invalid_argument&) { refused = true; }
  s.require("a statement with a literal subject is refused", refused);
  Dataset e;
  e.apply({{}, {a, c}});
  const auto df = diff(d, e);
  s.require("the difference: b only in the first, c only in the second", df.only_first.size() == 1 && df.only_first[0] == b && df.only_second.size() == 1 && df.only_second[0] == c);
  s.require("the graphs held: the default and ex:g", d.graphs().size() == 2 && d.size(iri("http://e/g")) == 1);
  return s.done();
}
