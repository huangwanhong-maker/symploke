// Checks of SHACL validation (shapes.hpp): a shapes graph for records of archival holdings and a data graph with one record
// right and one wrong in every way the shapes can tell, worked by hand: each result with its focus node, its value and its
// constraint component; targets by class through subclasses and by subjects; paths inverse, sequential and alternative;
// logic, nesting and closed shapes; data that conforms has no result; the report written as RDF.

#include <symploke/shapes/shapes.hpp>
#include <symploke/syntax/syntax.hpp>

#include <calx/verification/check.hpp>

#include <map>
#include <set>
#include <string>

using namespace symploke;
using namespace symploke::shapes;

namespace {
store::Dataset load(const std::string& ttl) {
  store::Dataset d;
  store::Change c;
  c.added = syntax::read_turtle(ttl).quads;
  d.apply(c);
  return d;
}
}  // namespace

int main() {
  calx::verification::Suite s;
  const auto shapes = load(R"(@prefix sh: <http://www.w3.org/ns/shacl#> . @prefix ex: <http://e/> . @prefix xsd: <http://www.w3.org/2001/XMLSchema#> .
ex:RecordShape a sh:NodeShape ; sh:targetClass ex:Record ;
  sh:property [ sh:path ex:title ; sh:minCount 1 ; sh:maxCount 2 ; sh:datatype <http://www.w3.org/1999/02/22-rdf-syntax-ns#langString> ; sh:uniqueLang true ] ;
  sh:property [ sh:path ex:year ; sh:datatype xsd:integer ; sh:minInclusive 1000 ; sh:maxExclusive 2100 ] ;
  sh:property [ sh:path ex:identifier ; sh:pattern "^[A-Z]{2}-[0-9]+$" ; sh:maxLength 12 ] ;
  sh:property [ sh:path ex:creator ; sh:class ex:Agent ; sh:nodeKind sh:IRI ] ;
  sh:property [ sh:path ex:access ; sh:in ( "open" "restricted" "closed" ) ] ;
  sh:property [ sh:path ( ex:creator ex:name ) ; sh:minCount 1 ] ;
  sh:property [ sh:path [ sh:inversePath ex:partOf ] ; sh:maxCount 3 ] ;
  sh:property [ sh:path ex:opened ; sh:lessThan ex:closed ] .
ex:FondsShape a sh:NodeShape ; sh:targetSubjectsOf ex:hasPart ;
  sh:or ( [ sh:path ex:title ; sh:minCount 1 ] [ sh:path ex:label ; sh:minCount 1 ] ) ;
  sh:closed true ; sh:ignoredProperties ( <http://www.w3.org/1999/02/22-rdf-syntax-ns#type> ) ;
  sh:property [ sh:path ex:hasPart ] ; sh:property [ sh:path ex:title ] ; sh:property [ sh:path ex:label ] .
)");
  const auto data = load(R"(@prefix ex: <http://e/> . @prefix rdfs: <http://www.w3.org/2000/01/rdf-schema#> .
ex:Letter rdfs:subClassOf ex:Record .
ex:good a ex:Letter ; ex:title "Letter to Cara"@en ; ex:year 1859 ; ex:identifier "GE-17" ; ex:creator ex:eliot ; ex:access "open" ;
  ex:opened 1 ; ex:closed 2 .
ex:eliot a ex:Agent ; ex:name "George Eliot" .
ex:bad a ex:Record ; ex:title "Titre"@fr, "Title"@fr, "Third"@en ; ex:year "1859" ; ex:identifier "ge17" ;
  ex:creator "Eliot" ; ex:access "secret" ; ex:opened 5 ; ex:closed 3 .
ex:p1 ex:partOf ex:bad . ex:p2 ex:partOf ex:bad . ex:p3 ex:partOf ex:bad . ex:p4 ex:partOf ex:bad .
ex:fonds ex:hasPart ex:good ; ex:extent "3 boxes" .
)");
  const auto r = validate(data, shapes);
  std::map<std::string, std::set<std::string>> by_focus;
  for (const auto& x : r.results) by_focus[x.focus.value].insert(x.component);
  s.require("the record that is right in every way, a Letter and so a Record through the subclass, has no result", !by_focus.count("http://e/good"));
  const std::set<std::string> want = {"sh:MaxCountConstraintComponent", "sh:UniqueLangConstraintComponent", "sh:DatatypeConstraintComponent", "sh:MinInclusiveConstraintComponent", "sh:MaxExclusiveConstraintComponent",
                                      "sh:PatternConstraintComponent",  "sh:ClassConstraintComponent",      "sh:NodeKindConstraintComponent", "sh:InConstraintComponent",
                                      "sh:MinCountConstraintComponent", "sh:LessThanConstraintComponent"};
  std::string got;
  for (const auto& c : by_focus["http://e/bad"]) got += c + " ";
  s.require("the wrong record breaks eleven kinds of constraint, each found (a year given as a string fails both its bounds, being comparable with neither)", by_focus["http://e/bad"] == want, got);
  std::size_t inverse = 0, title_count = 0;
  for (const auto& x : r.results) {
    if (x.path == "^<http://e/partOf>" && x.component == "sh:MaxCountConstraintComponent") inverse++;
    if (x.path == "<http://e/title>" && x.component == "sh:MaxCountConstraintComponent") title_count++;
  }
  s.require("four parts exceed the inverse path's three, and three titles the maximum of two", inverse == 1 && title_count == 1);
  s.require("the fonds, found as a subject of ex:hasPart, is closed to its extent", by_focus["http://e/fonds"] == std::set<std::string>{"sh:ClosedConstraintComponent", "sh:OrConstraintComponent"});
  s.require("the data does not conform", !r.conforms);
  const auto ok = validate(load(R"(@prefix ex: <http://e/> .
ex:r a ex:Record ; ex:title "T"@en ; ex:creator ex:a . ex:a a ex:Agent ; ex:name "A" .)"), shapes);
  s.require("a record that conforms has no result", ok.conforms && ok.results.empty());
  const auto report = report_graph(r);
  std::size_t results = 0;
  for (const auto& t : report)
    if (t.p.value == vocabulary::sh + "result") results++;
  s.require("the report written as RDF has a sh:result for every result", results == r.results.size() && results > 0);
  return s.done();
}
