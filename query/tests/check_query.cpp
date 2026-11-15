// Checks of queries (query.hpp): on a small library of books worked by hand: basic patterns, OPTIONAL with and without its
// filter, UNION, MINUS, FILTER with numbers, strings, regular expressions and languages, BIND, VALUES, GRAPH, aggregates
// with GROUP BY and HAVING, ORDER BY, LIMIT, OFFSET and DISTINCT, ASK and CONSTRUCT; property paths held to reachability
// computed apart on random graphs; joins held to the brute-force product of two patterns; refusals.

#include <symploke/query/query.hpp>
#include <symploke/syntax/syntax.hpp>

#include <calx/random/stream.hpp>
#include <calx/verification/check.hpp>

#include <set>
#include <string>
#include <vector>

using namespace symploke;
using namespace symploke::query;

namespace {

store::Dataset load(const std::string& trig) {
  store::Dataset d;
  store::Change c;
  c.added = syntax::read_trig(trig).quads;
  d.apply(c);
  return d;
}

std::vector<std::string> column(const Result& r, std::size_t k) {
  std::vector<std::string> v;
  for (const auto& row : r.rows) v.push_back(row[k] ? row[k]->value : "UNBOUND");
  return v;
}

}  // namespace

int main() {
  calx::verification::Suite s;
  const auto d = load(R"(@prefix ex: <http://e/> .
ex:mm a ex:Novel ; ex:title "Middlemarch"@en ; ex:year 1871 ; ex:author ex:eliot ; ex:pages 880 .
ex:ad a ex:Novel ; ex:title "Adam Bede"@en ; ex:year 1859 ; ex:author ex:eliot ; ex:pages 600 .
ex:dc a ex:Novel ; ex:title "David Copperfield"@en ; ex:year 1850 ; ex:author ex:dickens .
ex:es a ex:Essay ; ex:title "Essays"@en, "Essais"@fr ; ex:year 1580 ; ex:author ex:montaigne .
ex:eliot ex:name "George Eliot" ; ex:born 1819 .
ex:dickens ex:name "Charles Dickens" ; ex:born 1812 .
ex:montaigne ex:name "Michel de Montaigne" .
ex:mm ex:cites ex:ad . ex:ad ex:cites ex:dc .
GRAPH ex:reviews { ex:mm ex:stars 5 . ex:dc ex:stars 4 . }
)");
  const std::string P = "PREFIX ex: <http://e/> ";
  const auto novels = run(d, P + "SELECT ?b WHERE { ?b a ex:Novel } ORDER BY ?b");
  s.require("three novels, in order of their IRIs", column(novels, 0) == std::vector<std::string>{"http://e/ad", "http://e/dc", "http://e/mm"});
  const auto eliot = run(d, P + "SELECT ?t WHERE { ?b ex:author ?a . ?a ex:name \"George Eliot\" . ?b ex:title ?t } ORDER BY DESC(?t)");
  s.require("George Eliot's titles, by a join through the author, in descending order", column(eliot, 0) == std::vector<std::string>{"Middlemarch", "Adam Bede"});
  const auto born = run(d, P + "SELECT ?n ?y WHERE { ?a ex:name ?n OPTIONAL { ?a ex:born ?y } } ORDER BY ?n");
  s.require("OPTIONAL keeps Montaigne without a year of birth", column(born, 1) == std::vector<std::string>{"1812", "1819", "UNBOUND"});
  const auto early = run(d, P + "SELECT ?n ?y WHERE { ?a ex:name ?n OPTIONAL { ?a ex:born ?y FILTER(?y < 1815) } } ORDER BY ?n");
  s.require("an OPTIONAL's filter sees its own side: Eliot, born after 1815, stays without a year", column(early, 1) == std::vector<std::string>{"1812", "UNBOUND", "UNBOUND"});
  const auto either = run(d, P + "SELECT ?b WHERE { { ?b a ex:Essay } UNION { ?b ex:year ?y FILTER(?y > 1860) } } ORDER BY ?b");
  s.require("UNION: the essay, and the book after 1860", column(either, 0) == std::vector<std::string>{"http://e/es", "http://e/mm"});
  const auto unreviewed = run(d, P + "SELECT ?b WHERE { ?b a ex:Novel MINUS { GRAPH ex:reviews { ?b ex:stars ?s } } }");
  s.require("MINUS with GRAPH: the one novel not reviewed", column(unreviewed, 0) == std::vector<std::string>{"http://e/ad"});
  const auto french = run(d, P + "SELECT ?t WHERE { ?b ex:title ?t FILTER(LANGMATCHES(LANG(?t), \"fr\")) }");
  s.require("a title in French", column(french, 0) == std::vector<std::string>{"Essais"});
  const auto regex = run(d, P + "SELECT ?t WHERE { ?b ex:title ?t FILTER(REGEX(?t, \"^m\", \"i\") || CONTAINS(STR(?t), \"Bede\")) } ORDER BY ?t");
  s.require("REGEX ignoring case, or CONTAINS", column(regex, 0) == std::vector<std::string>{"Adam Bede", "Middlemarch"});
  const auto age = run(d, P + "SELECT ?b ?age WHERE { ?b ex:year ?y ; ex:author ?a . ?a ex:born ?z BIND(?y - ?z AS ?age) } ORDER BY ?age");
  s.require("BIND: the authors' ages at publication, 38, 40 and 52", column(age, 1) == std::vector<std::string>{"38", "40", "52"});
  const auto values = run(d, P + "SELECT ?b ?y WHERE { VALUES ?b { ex:mm ex:es } ?b ex:year ?y } ORDER BY ?y");
  s.require("VALUES restricts the books", column(values, 1) == std::vector<std::string>{"1580", "1871"});
  const auto stars = run(d, P + "SELECT ?b ?s WHERE { GRAPH ?g { ?b ex:stars ?s } } ORDER BY DESC(?s)");
  s.require("GRAPH with a variable reads the named graph", column(stars, 1) == std::vector<std::string>{"5", "4"});
  const auto counts = run(d, P + "SELECT ?a (COUNT(?b) AS ?n) (AVG(?y) AS ?mean) WHERE { ?b ex:author ?a ; ex:year ?y } GROUP BY ?a HAVING (COUNT(?b) > 1)");
  s.require("GROUP BY with HAVING: Eliot alone wrote two, in 1865 on average", counts.rows.size() == 1 && counts.rows[0][0]->value == "http://e/eliot" && counts.rows[0][1]->value == "2" &&
                                                                                 query::detail::compare(*counts.rows[0][2], literal("1865", vocabulary::xsd + "integer")) == 0);
  const auto total = run(d, P + "SELECT (SUM(?p) AS ?pages) (COUNT(*) AS ?n) (GROUP_CONCAT(?p; SEPARATOR=\"+\") AS ?list) WHERE { ?b ex:pages ?p }");
  s.require("aggregates over everything: 1,480 pages over two books", total.rows.size() == 1 && total.rows[0][0]->value == "1480" && total.rows[0][1]->value == "2");
  const auto page = run(d, P + "SELECT DISTINCT ?a WHERE { ?b ex:author ?a } ORDER BY ?a LIMIT 1 OFFSET 1");
  s.require("DISTINCT, ORDER BY, LIMIT and OFFSET: the second author", column(page, 0) == std::vector<std::string>{"http://e/eliot"});
  s.require("ASK", run(d, P + "ASK { ex:mm ex:cites ex:ad }").truth && !run(d, P + "ASK { ex:dc ex:cites ?x }").truth);
  const auto built = run(d, P + "CONSTRUCT { ?a ex:wrote ?b } WHERE { ?b ex:author ?a }");
  s.require("CONSTRUCT turns authorship round", built.triples.size() == 4);
  const auto cites = run(d, P + "SELECT ?x WHERE { ex:mm ex:cites+ ?x } ORDER BY ?x");
  s.require("a path ex:cites+ from Middlemarch reaches Adam Bede and David Copperfield", column(cites, 0) == std::vector<std::string>{"http://e/ad", "http://e/dc"});
  const auto back = run(d, P + "SELECT ?n WHERE { ex:dc ^ex:cites/ex:author/ex:name ?n }");
  s.require("an inverse path in a sequence: the author of what cites David Copperfield", column(back, 0) == std::vector<std::string>{"George Eliot"});

  // ---- Paths and joins on random graphs ----------------------------------------------------------------------------------------
  calx::random::Stream rng(20261115, 81, 0, 0);
  std::size_t path_faults = 0, join_faults = 0, graphs = 0;
  for (int trial = 0; trial < 30; trial++) {
    store::Dataset g;
    const int n = 8;
    std::vector<std::vector<int>> adj(n, std::vector<int>(n, 0)), adj2(n, std::vector<int>(n, 0));
    store::Change c;
    for (int k = 0; k < 14; k++) {
      const int a = static_cast<int>(rng.below(n)), b = static_cast<int>(rng.below(n));
      const bool second = rng.bernoulli(0.4);
      (second ? adj2 : adj)[a][b] = 1;
      c.added.push_back({iri("http://e/" + std::to_string(a)), iri(second ? "http://e/q" : "http://e/p"), iri("http://e/" + std::to_string(b)), {}});
    }
    g.apply(c);
    graphs++;
    // Reachability by p: Warshall's closure, apart from the evaluator.
    auto reach = adj;
    for (int k = 0; k < n; k++)
      for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
          if (reach[i][k] && reach[k][j]) reach[i][j] = 1;
    for (int start = 0; start < n; start++) {
      const auto r = run(g, "SELECT ?x WHERE { <http://e/" + std::to_string(start) + "> <http://e/p>* ?x }");
      std::set<std::string> got, want{"http://e/" + std::to_string(start)};
      for (const auto& row : r.rows) got.insert(row[0]->value);
      for (int j = 0; j < n; j++)
        if (reach[start][j]) want.insert("http://e/" + std::to_string(j));
      if (got != want) path_faults++;
    }
    // A join of two patterns against the product computed apart.
    const auto r = run(g, "SELECT ?a ?b ?c WHERE { ?a <http://e/p> ?b . ?b <http://e/q> ?c }");
    std::set<std::string> got, want;
    for (const auto& row : r.rows) got.insert(row[0]->value + " " + row[1]->value + " " + row[2]->value);
    for (int a = 0; a < n; a++)
      for (int b = 0; b < n; b++)
        for (int cc = 0; cc < n; cc++)
          if (adj[a][b] && adj2[b][cc]) want.insert("http://e/" + std::to_string(a) + " http://e/" + std::to_string(b) + " http://e/" + std::to_string(cc));
    if (got != want || r.rows.size() != want.size()) join_faults++;
  }
  s.require("on " + std::to_string(graphs) + " random graphs, ex:p* from every node reaches what Warshall's closure reaches", path_faults == 0, std::to_string(path_faults));
  s.require("and a join of two patterns answers the product computed apart", join_faults == 0, std::to_string(join_faults));

  bool r1 = false, r2 = false;
  try { (void)run(d, P + "SELECT ?x WHERE { SERVICE <http://x> { ?x ?y ?z } }"); } catch (const Error&) { r1 = true; }
  try { (void)run(d, "SELECT ?x WHERE { ?x nope:p ?y }"); } catch (const Error&) { r2 = true; }
  s.require("SERVICE and an undeclared prefix are refused", r1 && r2);
  return s.done();
}
