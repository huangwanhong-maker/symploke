// Checks of RDF documents (syntax.hpp): every example of resolving a reference in RFC 3986 §5.4, normal and abnormal;
// a Turtle document using every construct of the grammar read into the statements it means, written by hand in N-Triples;
// TriG's graphs; N-Triples and N-Quads read; anonymous nodes labelled apart from the document's own labels; what the writers
// write read back as the same statements; malformed documents refused with their line and column.

#include <symploke/syntax/syntax.hpp>

#include <calx/verification/check.hpp>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

using namespace symploke::syntax;

namespace {

std::set<std::string> lines(const std::vector<Quad>& qs) {
  std::set<std::string> v;
  for (const auto& q : qs) v.insert(nt(q.s) + " " + nt(q.p) + " " + nt(q.o) + (q.g.empty() ? "" : " " + nt(q.g)));
  return v;
}

}  // namespace

int main() {
  calx::verification::Suite s;

  // ---- RFC 3986 §5.4 ------------------------------------------------------------------------------------------------------------
  const std::string base = "http://a/b/c/d;p?q";
  const std::vector<std::pair<std::string, std::string>> examples = {
      {"g:h", "g:h"},           {"g", "http://a/b/c/g"},        {"./g", "http://a/b/c/g"},      {"g/", "http://a/b/c/g/"},       {"/g", "http://a/g"},
      {"//g", "http://g"},      {"?y", "http://a/b/c/d;p?y"},   {"g?y", "http://a/b/c/g?y"},    {"#s", "http://a/b/c/d;p?q#s"},  {"g#s", "http://a/b/c/g#s"},
      {"g?y#s", "http://a/b/c/g?y#s"}, {";x", "http://a/b/c/;x"}, {"g;x", "http://a/b/c/g;x"}, {"g;x?y#s", "http://a/b/c/g;x?y#s"}, {"", "http://a/b/c/d;p?q"},
      {".", "http://a/b/c/"},   {"./", "http://a/b/c/"},        {"..", "http://a/b/"},          {"../", "http://a/b/"},          {"../g", "http://a/b/g"},
      {"../..", "http://a/"},   {"../../", "http://a/"},        {"../../g", "http://a/g"},      {"../../../g", "http://a/g"},    {"../../../../g", "http://a/g"},
      {"/./g", "http://a/g"},   {"/../g", "http://a/g"},        {"g.", "http://a/b/c/g."},      {".g", "http://a/b/c/.g"},       {"g..", "http://a/b/c/g.."},
      {"..g", "http://a/b/c/..g"}, {"./../g", "http://a/b/g"},   {"./g/.", "http://a/b/c/g/"},   {"g/./h", "http://a/b/c/g/h"},   {"g/../h", "http://a/b/c/h"},
      {"g;x=1/./y", "http://a/b/c/g;x=1/y"}, {"g;x=1/../y", "http://a/b/c/y"}, {"g?y/./x", "http://a/b/c/g?y/./x"}, {"g?y/../x", "http://a/b/c/g?y/../x"},
      {"g#s/./x", "http://a/b/c/g#s/./x"}, {"g#s/../x", "http://a/b/c/g#s/../x"}};
  std::string wrong;
  for (const auto& [ref, want] : examples)
    if (resolve(base, ref) != want) wrong += ref + " → " + resolve(base, ref) + " (not " + want + ") ";
  s.require("all " + std::to_string(examples.size()) + " examples of RFC 3986 §5.4 resolve as the RFC says", wrong.empty(), wrong);

  // ---- Turtle --------------------------------------------------------------------------------------------------------------------
  const std::string ttl = R"(@base <http://example.org/books/> .
@prefix ex: <http://example.org/> .
PREFIX dc: <http://purl.org/dc/terms/>
# a comment
<middlemarch> a ex:Novel ;
    dc:title "Middlemarch"@en, "米德尔马契"@zh ;
    ex:year 1871 ; ex:rating 4.5 ; ex:weight 1.2e3 ; ex:inPrint true ;
    ex:author [ ex:name 'George Eliot' ; ex:born "1819-11-22"^^<http://www.w3.org/2001/XMLSchema#date> ] ;
    ex:parts ( "Miss Brooke" _:old ) ;
    ex:note """two
lines with "quotes" and \t a tab""" .
_:old ex:label "Old and Youngé" .
ex:dots\.and\-dashes ex:local ex:a.b .
)";
  const auto doc = read_turtle(ttl);
  const std::string X = "http://www.w3.org/2001/XMLSchema#", R = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
  const std::set<std::string> want = {
      "<http://example.org/books/middlemarch> <" + R + "type> <http://example.org/Novel>",
      "<http://example.org/books/middlemarch> <http://purl.org/dc/terms/title> \"Middlemarch\"@en",
      "<http://example.org/books/middlemarch> <http://purl.org/dc/terms/title> \"米德尔马契\"@zh",
      "<http://example.org/books/middlemarch> <http://example.org/year> \"1871\"^^<" + X + "integer>",
      "<http://example.org/books/middlemarch> <http://example.org/rating> \"4.5\"^^<" + X + "decimal>",
      "<http://example.org/books/middlemarch> <http://example.org/weight> \"1.2e3\"^^<" + X + "double>",
      "<http://example.org/books/middlemarch> <http://example.org/inPrint> \"true\"^^<" + X + "boolean>",
      "<http://example.org/books/middlemarch> <http://example.org/author> _:genid0",
      "_:genid0 <http://example.org/name> \"George Eliot\"",
      "_:genid0 <http://example.org/born> \"1819-11-22\"^^<" + X + "date>",
      "<http://example.org/books/middlemarch> <http://example.org/parts> _:genid1",
      "_:genid1 <" + R + "first> \"Miss Brooke\"",
      "_:genid1 <" + R + "rest> _:genid2",
      "_:genid2 <" + R + "first> _:old",
      "_:genid2 <" + R + "rest> <" + R + "nil>",
      "<http://example.org/books/middlemarch> <http://example.org/note> \"two\\nlines with \\\"quotes\\\" and \\t a tab\"",
      "_:old <http://example.org/label> \"Old and Youngé\"",
      "<http://example.org/dots.and-dashes> <http://example.org/local> <http://example.org/a.b>"};
  const auto got = lines(doc.quads);
  std::string missing;
  for (const auto& w : want)
    if (!got.count(w)) missing += w + " | ";
  s.require("a Turtle document using every construct means the 18 statements written by hand", got == want && doc.quads.size() == 18, missing);
  s.require("its prefixes and base are kept", doc.prefixes.map.at("dc") == "http://purl.org/dc/terms/" && doc.base == "http://example.org/books/");

  // Anonymous nodes are labelled apart from a document's own labels.
  const auto clash = read_turtle("@prefix ex: <http://e/> . _:genid0 ex:p [ ex:q 1 ] .");
  const auto linking = std::find_if(clash.quads.begin(), clash.quads.end(), [](const Quad& q) { return q.p.value == "http://e/p"; });
  s.require("an anonymous node is not given a label the document already uses", linking != clash.quads.end() && linking->s.value == "genid0" && linking->o.is_blank() && linking->o.value != "genid0");

  // ---- TriG ------------------------------------------------------------------------------------------------------------------------
  const auto trig = read_trig(R"(@prefix ex: <http://e/> .
ex:a ex:p ex:b .
GRAPH ex:g1 { ex:a ex:p ex:c . ex:c ex:p ex:d }
ex:g2 { ex:x ex:y "z" }
{ ex:m ex:n ex:o . }
)");
  std::size_t in_default = 0, in_g1 = 0, in_g2 = 0;
  for (const auto& q : trig.quads) (q.g.empty() ? in_default : q.g.value == "http://e/g1" ? in_g1 : in_g2)++;
  s.require("TriG: two statements in the default graph, two in ex:g1 and one in ex:g2", in_default == 2 && in_g1 == 2 && in_g2 == 1);

  // ---- N-Triples and N-Quads -------------------------------------------------------------------------------------------------------
  const auto ntd = read_ntriples("<http://e/s> <http://e/p> \"a\\u00E9\\n\"@en-GB . # a comment\n\n_:b1 <http://e/p> <http://e/o> .\n");
  s.require("N-Triples: two statements, the escapes undone and the tag kept in lower case", ntd.quads.size() == 2 && ntd.quads[0].o.value == "aé\n" && ntd.quads[0].o.language == "en-gb");
  const auto nqd = read_nquads("<http://e/s> <http://e/p> <http://e/o> <http://e/g> .\n<http://e/s> <http://e/p> \"x\" .\n");
  s.require("N-Quads: a statement in a named graph and one in the default graph", nqd.quads[0].g.value == "http://e/g" && nqd.quads[1].g.empty());

  // ---- Writers read back ------------------------------------------------------------------------------------------------------------
  std::vector<Triple> triples;
  for (const auto& q : doc.quads) triples.push_back(q.triple());
  const auto again = read_turtle(write_turtle(triples, doc.prefixes));
  s.require("Turtle written and read again gives the same statements", lines(again.quads) == got, write_turtle(triples, doc.prefixes));
  const auto nt_again = read_ntriples(write_ntriples(triples));
  s.require("and so does N-Triples", lines(nt_again.quads) == got);
  const auto trig_again = read_trig(write_trig(trig.quads, trig.prefixes));
  s.require("and TriG, graphs and all", lines(trig_again.quads) == lines(trig.quads));

  // ---- Refusals ------------------------------------------------------------------------------------------------------------------------
  const auto refused = [](const std::string& text, bool turtle, std::size_t line) {
    try {
      turtle ? (void)read_turtle(text) : (void)read_ntriples(text);
    } catch (const Error& e) {
      return e.line == line;
    }
    return false;
  };
  s.require("an undeclared prefix, a literal subject, a relative IRI without a base, an unclosed string and a missing dot are refused at their lines",
            refused("@prefix ex: <http://e/> .\nex:a ex:p nope:b .", true, 2) && refused("\"x\" <http://e/p> <http://e/o> .", true, 1) && refused("<a> <http://e/p> <http://e/o> .", true, 1) &&
                refused("<http://e/s> <http://e/p> \"open .\n", false, 1) && refused("<http://e/s> <http://e/p> <http://e/o>\n", false, 1));
  return s.done();
}
