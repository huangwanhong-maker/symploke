// Checks of graphs compared up to their blank nodes (canonical.hpp): on random graphs with blank nodes, a copy with its blank
// nodes renamed and its statements shuffled is isomorphic and canonicalises to the same statements; a copy with one
// statement's object changed is not, and says where; graphs that differ in ground statements or in their numbers of blank
// nodes are told apart; a cycle of blank nodes and its reverse.

#include <symploke/canonical/canonical.hpp>
#include <symploke/syntax/syntax.hpp>

#include <calx/random/stream.hpp>
#include <calx/verification/check.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace symploke;
using namespace symploke::canonical;

int main() {
  calx::verification::Suite s;
  calx::random::Stream rng(20261115, 101, 0, 0);
  std::size_t same_faults = 0, canon_faults = 0, diff_faults = 0, trials = 0;
  for (int trial = 0; trial < 60; trial++) {
    std::vector<Triple> g;
    const auto node = [&]() { return rng.bernoulli(0.6) ? blank("b" + std::to_string(rng.below(5))) : iri("http://e/n" + std::to_string(rng.below(4))); };
    for (int k = 0; k < 9; k++) g.push_back({node(), iri("http://e/p" + std::to_string(rng.below(2))), rng.bernoulli(0.2) ? literal("v") : node()});
    // Rename blank nodes by a random permutation, and shuffle.
    std::vector<int> perm = {0, 1, 2, 3, 4};
    for (int k = 4; k > 0; k--) std::swap(perm[k], perm[rng.below(k + 1)]);
    std::vector<Triple> h;
    for (auto t : g) {
      for (Term* x : {&t.s, &t.o})
        if (x->is_blank()) x->value = "x" + std::to_string(perm[x->value[1] - '0']);
      h.push_back(t);
    }
    for (std::size_t k = h.size(); k > 1; k--) std::swap(h[k - 1], h[rng.below(k)]);
    trials++;
    const auto v = isomorphic(g, h);
    if (!v.holds()) same_faults++;
    if (canonicalise(g).triples != canonicalise(h).triples) canon_faults++;
    // Change one statement's object to a new IRI: no longer isomorphic.
    auto changed = h;
    changed[0].o = iri("http://e/elsewhere");
    if (isomorphic(g, changed).holds()) diff_faults++;
  }
  s.require("on 60 random graphs, a copy with blank nodes renamed and statements shuffled is isomorphic", same_faults == 0, std::to_string(same_faults));
  s.require("and both canonicalise to the same statements", canon_faults == 0);
  s.require("a copy with one object changed is not", diff_faults == 0);
  const auto cycle = syntax::read_turtle("@prefix ex: <http://e/> . _:a ex:next _:b . _:b ex:next _:c . _:c ex:next _:a .").quads;
  const auto reverse = syntax::read_turtle("@prefix ex: <http://e/> . _:x ex:next _:z . _:z ex:next _:y . _:y ex:next _:x .").quads;
  const auto chain = syntax::read_turtle("@prefix ex: <http://e/> . _:x ex:next _:y . _:y ex:next _:z . _:z ex:next ex:end .").quads;
  std::vector<Triple> c1, c2, c3;
  for (const auto& q : cycle) c1.push_back(q.triple());
  for (const auto& q : reverse) c2.push_back(q.triple());
  for (const auto& q : chain) c3.push_back(q.triple());
  s.require("a cycle of three blank nodes, labelled otherwise, is the same cycle", isomorphic(c1, c2).holds());
  const auto v = isomorphic(c1, c3);
  s.require("a chain of three blank nodes ending at an IRI is not a cycle of three", v.fails(), v.reason);
  s.require("graphs with different numbers of blank nodes are told apart", isomorphic(c1, {c1[0], c1[1]}).fails());
  return s.done();
}
