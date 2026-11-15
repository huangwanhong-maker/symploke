// Checks of entailment (entailment.hpp): a small vocabulary worked by hand through every RDFS rule; the derivation of a
// conclusion explained down to what was asserted; materialising twice adds nothing; on random hierarchies of classes, the
// subclasses entailed are exactly the pairs morphe's reachability finds; OWL's symmetric, transitive and inverse
// properties, equivalences and sameness; inconsistencies reported, not repaired.

#include <symploke/entailment/entailment.hpp>
#include <symploke/syntax/syntax.hpp>

#include <morphe/combinatorics/acyclic.hpp>

#include <calx/random/stream.hpp>
#include <calx/verification/check.hpp>

#include <set>
#include <string>

using namespace symploke;
using namespace symploke::entailment;

namespace {
store::Dataset load(const std::string& ttl) {
  store::Dataset d;
  store::Change c;
  c.added = syntax::read_turtle(ttl).quads;
  d.apply(c);
  return d;
}
bool holds(const store::Dataset& d, const std::string& s, const std::string& p, const std::string& o) {
  return d.contains({iri("http://e/" + s), iri(p.find(':') == std::string::npos ? "http://e/" + p : p), iri("http://e/" + o), {}});
}
}  // namespace

int main() {
  calx::verification::Suite s;
  const std::string RDF = vocabulary::rdf, RDFS = vocabulary::rdfs, OWL = vocabulary::owl;
  auto d = load(R"(@prefix ex: <http://e/> . @prefix rdfs: <http://www.w3.org/2000/01/rdf-schema#> .
ex:Novel rdfs:subClassOf ex:Book . ex:Book rdfs:subClassOf ex:Work .
ex:wrote rdfs:domain ex:Author ; rdfs:range ex:Book ; rdfs:subPropertyOf ex:made .
ex:made rdfs:subPropertyOf ex:related .
ex:eliot ex:wrote ex:mm . ex:mm a ex:Novel .
ex:eliot ex:name "George Eliot" .
)");
  const auto e = materialise(d);
  s.require("rdfs2: the domain makes Eliot an author", holds(d, "eliot", RDF + "type", "Author"));
  s.require("rdfs3: the range makes Middlemarch a book", holds(d, "mm", RDF + "type", "Book"));
  s.require("rdfs5 and rdfs7: Eliot made and is related to Middlemarch", holds(d, "wrote", RDFS + "subPropertyOf", "related") && holds(d, "eliot", "made", "mm") && holds(d, "eliot", "related", "mm"));
  s.require("rdfs9 and rdfs11: Middlemarch is a work", holds(d, "mm", RDF + "type", "Work") && holds(d, "Novel", RDFS + "subClassOf", "Work"));
  s.require("no literal is typed by a range, and nothing asserted is lost", !d.match({literal("George Eliot"), std::nullopt, std::nullopt, std::nullopt}).size() && holds(d, "mm", RDF + "type", "Novel"));
  const auto x = explain(e, {iri("http://e/mm"), iri(RDF + "type"), iri("http://e/Work")});
  s.require("the conclusion that Middlemarch is a work is explained by a rule, down to asserted statements",
            x.rule.rfind("rdfs", 0) == 0 && !x.from.empty() && [&] {
              std::function<bool(const Explanation&)> leaves = [&](const Explanation& n) {
                if (n.from.empty()) return n.rule == "asserted";
                for (const auto& c : n.from)
                  if (!leaves(c)) return false;
                return true;
              };
              return leaves(x);
            }());
  const std::size_t size = d.size();
  const auto again = materialise(d);
  s.require("materialising again adds nothing", again.derived.empty() && d.size() == size && again.rounds == 1);

  // ---- Random hierarchies -------------------------------------------------------------------------------------------------------
  calx::random::Stream rng(20261115, 91, 0, 0);
  std::size_t faults = 0;
  for (int trial = 0; trial < 30; trial++) {
    const std::size_t n = 10;
    std::vector<morphe::combinatorics::Arc> arcs;
    store::Dataset h;
    store::Change c;
    for (int k = 0; k < 14; k++) {
      const std::size_t a = rng.below(n), b = rng.below(n);
      if (a >= b) continue;  // a ⊑ b with a < b: acyclic
      arcs.push_back({a, b});
      c.added.push_back({iri("http://e/C" + std::to_string(a)), iri(RDFS + "subClassOf"), iri("http://e/C" + std::to_string(b)), {}});
    }
    h.apply(c);
    materialise(h);
    const auto before = morphe::combinatorics::reachability(n, arcs);
    for (std::size_t a = 0; a < n; a++)
      for (std::size_t b = 0; b < n; b++) {
        const bool want = a != b && before[a][b];
        const bool got = h.contains({iri("http://e/C" + std::to_string(a)), iri(RDFS + "subClassOf"), iri("http://e/C" + std::to_string(b)), {}});
        faults += want != got;
      }
  }
  s.require("on 30 random hierarchies, the subclasses entailed are exactly the pairs morphe's reachability finds", faults == 0, std::to_string(faults));

  // ---- OWL ----------------------------------------------------------------------------------------------------------------------------
  auto o = load(R"(@prefix ex: <http://e/> . @prefix owl: <http://www.w3.org/2002/07/owl#> .
ex:knows a owl:SymmetricProperty . ex:ancestorOf a owl:TransitiveProperty .
ex:parentOf owl:inverseOf ex:childOf . ex:Person owl:equivalentClass ex:Human .
ex:a ex:knows ex:b . ex:a ex:ancestorOf ex:b . ex:b ex:ancestorOf ex:c . ex:c ex:ancestorOf ex:d .
ex:a ex:parentOf ex:b . ex:x owl:sameAs ex:y . ex:y owl:sameAs ex:z .
)");
  materialise(o, Regime::rdfs_owl);
  s.require("symmetric, transitive to every distance, and inverse properties", holds(o, "b", "knows", "a") && holds(o, "a", "ancestorOf", "d") && holds(o, "b", "childOf", "a"));
  s.require("equivalent classes are subclasses of each other; sameness is symmetric and transitive",
            holds(o, "Person", RDFS + "subClassOf", "Human") && holds(o, "Human", RDFS + "subClassOf", "Person") && holds(o, "z", OWL + "sameAs", "x"));
  auto bad = load(R"(@prefix ex: <http://e/> . @prefix owl: <http://www.w3.org/2002/07/owl#> . @prefix rdfs: <http://www.w3.org/2000/01/rdf-schema#> .
ex:Cat owl:disjointWith ex:Dog . ex:Kitten rdfs:subClassOf ex:Cat . ex:rex a ex:Dog, ex:Kitten .
ex:p owl:sameAs ex:q . ex:p owl:differentFrom ex:q .
)");
  const auto be = materialise(bad, Regime::rdfs_owl);
  s.require("a kitten that is a dog, and a thing the same as and different from another, are reported", be.inconsistencies.size() == 2, be.inconsistencies.empty() ? "" : be.inconsistencies[0]);
  return s.done();
}
