// Checks of JSON-LD (json.hpp, jsonld.hpp): JSON read with the line and column of what is wrong, and written back; the JSON
// Canonicalization Scheme held to RFC 8785's own examples of numbers and of the order of members; then every document of
// fixtures.hpp read into the statements pyld, the reference implementation, gives for it, up to the names of blank nodes; every
// document pyld refuses refused with the same error code; datasets written as pyld compacts them against the same prefixes; and
// every document written back and read again into the same statements.

#include <symploke/canonical/canonical.hpp>
#include <symploke/jsonld/jsonld.hpp>

#include <calx/verification/check.hpp>

#include "fixtures.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace symploke;
using jsonld::Json;

namespace {

// A dataset as one graph: each statement a blank node joined to its subject, predicate, object and graph, so that two datasets
// are isomorphic exactly when these graphs are.
std::vector<terms::Triple> as_graph(const std::vector<terms::Quad>& qs) {
  std::vector<terms::Triple> out;
  const auto v = [](const char* s) { return terms::iri(std::string("urn:check:") + s); };
  std::size_t n = 0;
  for (const auto& q : qs) {
    const terms::Term st = terms::blank("statement" + std::to_string(n++));
    out.push_back({st, v("s"), q.s});
    out.push_back({st, v("p"), q.p});
    out.push_back({st, v("o"), q.o});
    out.push_back({st, v("g"), q.g.empty() ? v("default") : q.g});
  }
  for (auto& t : out)
    for (terms::Term* x : {&t.s, &t.o})
      if (x->is_blank() && x->value.rfind("statement", 0) != 0) x->value = "doc_" + x->value;
  return out;
}

bool same_dataset(const std::vector<terms::Quad>& a, const std::vector<terms::Quad>& b, std::string& why) {
  std::set<terms::Quad> sa(a.begin(), a.end()), sb(b.begin(), b.end());
  const std::vector<terms::Quad> ua(sa.begin(), sa.end()), ub(sb.begin(), sb.end());
  if (ua.size() != ub.size()) {
    why = std::to_string(ua.size()) + " statements against " + std::to_string(ub.size());
    return false;
  }
  const auto v = canonical::isomorphic(as_graph(ua), as_graph(ub));
  why = v.reason;
  return v.status == apodeixis::kernel::Status::holds;
}

// JSON compared as JSON-LD's compaction leaves it free: an array of one value as the value, arrays (but lists) as multisets.
Json normal(const Json& v, bool in_list = false) {
  if (v.is_array()) {
    Json a = Json::array();
    for (const auto& x : v.items) a.items.push_back(normal(x));
    if (!in_list && a.items.size() == 1) return a.items[0];
    if (!in_list) std::sort(a.items.begin(), a.items.end(), [](const Json& x, const Json& y) { return jsonld::canonical(x) < jsonld::canonical(y); });
    return a;
  }
  if (v.is_object()) {
    Json o = Json::object();
    for (const auto& [k, x] : v.members) {
      Json n = normal(x, k == "@list");
      if (k == "@list" && !n.is_array()) n = Json::array({n});
      o.set(k, n);
    }
    return o;
  }
  return v;
}

std::string lines(const std::vector<terms::Quad>& qs) {
  std::string s;
  for (const auto& q : qs) s += terms::nt(q.s) + " " + terms::nt(q.p) + " " + terms::nt(q.o) + (q.g.empty() ? "" : " " + terms::nt(q.g)) + " .\n";
  return s;
}

}  // namespace

int main() {
  calx::verification::Suite s;

  // ---- JSON --------------------------------------------------------------------------------------------------------------------
  {
    const Json v = jsonld::parse("{\n  \"a\": [1, -2.5e3, true, null, \"\\u00e9\\ud83d\\ude00\"],\n  \"b\": {}\n}");
    const Json* a = v.get("a");
    const bool ok = a && a->items.size() == 5 && a->items[0].lexical == "1" && a->items[1].number == -2500 && a->items[2].truth && a->items[3].is_null() &&
                    a->items[4].text == "\xC3\xA9\xF0\x9F\x98\x80" && a->line == 2 && a->column == 8 && v.get("b")->is_object();
    s.require("json: values read, escapes and surrogate pairs decoded, each value placed at its line and column", ok);
    std::size_t line = 0, column = 0;
    try {
      jsonld::parse("{\"a\": [1, 2,]}");
    } catch (const jsonld::SyntaxError& e) {
      line = e.line, column = e.column;
    }
    s.require("json: a trailing comma refused at its line and column", line == 1 && column == 13, std::to_string(line) + ":" + std::to_string(column));
    bool refused = false;
    try {
      jsonld::parse("[\"\\ud800x\"]");
    } catch (const jsonld::SyntaxError&) {
      refused = true;
    }
    s.require("json: a lone surrogate refused", refused);
    s.require("json: written back and read again the same", jsonld::same(jsonld::parse(jsonld::dump(v, 2)), v));
  }
  {
    // RFC 8785 §3.2.2.3 and Appendix B: numbers as ECMAScript writes them.
    const std::vector<std::pair<double, std::string>> numbers = {
        {333333333.33333329, "333333333.3333333"}, {1e30, "1e+30"}, {4.50, "4.5"}, {2e-3, "0.002"}, {1e-27, "1e-27"}, {0.000001, "0.000001"},
        {1e-7, "1e-7"},   {1e21, "1e+21"},     {1e20, "100000000000000000000"}, {-1.5, "-1.5"}, {9007199254740992.0, "9007199254740992"}, {5e-324, "5e-324"}};
    bool ok = true;
    std::string bad;
    for (const auto& [x, want] : numbers)
      if (const auto got = jsonld::detail::ecmascript(x); got != want) ok = false, bad += got + " for " + want + "; ";
    s.require("json canonical: numbers as ECMAScript writes them (RFC 8785's examples)", ok, bad);
    // RFC 8785 §3.2.3: members sorted by the UTF-16 code units of their names.
    const Json o = jsonld::parse("{\"\\u20ac\":\"Euro Sign\",\"\\r\":\"Carriage Return\",\"\\ufb33\":\"Hebrew Letter Dalet With Dagesh\",\"1\":\"One\",\"\\ud83d\\ude00\":\"Emoji: Grinning Face\",\"\\u0080\":\"Control\",\"\\u00f6\":\"Latin Small Letter O With Diaeresis\"}");
    const std::string c = jsonld::canonical(o);
    std::vector<std::string> order;
    for (const auto& m : jsonld::parse(c).members) order.push_back(m.second.text);
    const std::vector<std::string> want = {"Carriage Return", "One", "Control", "Latin Small Letter O With Diaeresis", "Euro Sign", "Emoji: Grinning Face", "Hebrew Letter Dalet With Dagesh"};
    s.require("json canonical: members in the order of their names' UTF-16 code units (RFC 8785 §3.2.3)", order == want);
  }

  // ---- To RDF, held to pyld --------------------------------------------------------------------------------------------------------
  std::size_t agreed = 0;
  for (const auto& c : fixtures::to_rdf) {
    jsonld::Options o;
    o.base = c.base;
    o.documents = c.documents;
    std::string why;
    try {
      const auto got = jsonld::read(c.input, o).quads;
      const auto want = syntax::read_nquads(c.expected).quads;
      const bool ok = same_dataset(got, want, why);
      agreed += ok;
      s.require("jsonld to RDF as pyld: " + c.name, ok, why + "\n" + lines(got));
    } catch (const std::exception& e) {
      s.require("jsonld to RDF as pyld: " + c.name, false, e.what());
    }
  }
  {
    std::size_t refused_alike = 0;
    std::string bad;
    for (const auto& c : fixtures::errors) {
      std::string code = "none";
      try {
        jsonld::Options o;
        o.base = c.base;
        jsonld::read(c.input, o);
      } catch (const jsonld::Error& e) {
        code = e.code;
      }
      if (code == c.expected) refused_alike++;
      else bad += c.name + ": " + code + " where pyld says " + c.expected + "; ";
    }
    s.require("jsonld: every document pyld refuses refused with the same error code (" + std::to_string(fixtures::errors.size()) + ")", refused_alike == fixtures::errors.size(), bad);
  }
  {
    std::string missing, where;
    try {
      jsonld::read("{\n  \"@context\": \"http://example.org/ctx\",\n  \"@id\": \"http://example.org/x\"\n}");
    } catch (const jsonld::Error& e) {
      missing = e.missing, where = std::to_string(e.line) + ":" + std::to_string(e.column);
    }
    s.require("jsonld: a remote context not given is named, with where it is asked for", missing == "http://example.org/ctx" && where == "2:15", missing + " " + where);
    const auto d = jsonld::read(R"({"@id": "http://example.org/x", "http://example.org/p": {"@value": "x", "@language": "not a tag"}, "http://example.org/q": {"@value": "y", "@language": "en-GB"}})");
    s.require("jsonld: a literal whose language tag is not well formed yields no statement; one well formed is kept in lower case",
            d.quads.size() == 1 && d.quads[0].o.language == "en-gb");
    const auto p = jsonld::read(R"({"@context": {"dcterms": "http://purl.org/dc/terms/", "title": "dcterms:title", "ns": "http://example.org/ns"}, "@id": "http://example.org/x", "title": "t"})");
    s.require("jsonld: the prefixes of a document's context are kept, a term whose IRI ends otherwise is not a prefix",
            p.prefixes.map.count("dcterms") && !p.prefixes.map.count("ns") && !p.prefixes.map.count("title"));
  }

  // ---- Compacted as pyld compacts --------------------------------------------------------------------------------------------------
  for (const auto& c : fixtures::compact) {
    terms::Prefixes prefixes;
    for (const auto& [k, v] : c.prefixes) prefixes.map[k] = v;
    const auto quads = syntax::read_nquads(c.nquads).quads;
    std::string text;
    try {
      text = jsonld::write(quads, prefixes);
      jsonld::read(text);
    } catch (const std::exception& e) {
      s.require("jsonld written as pyld compacts it: " + c.name, false, std::string(e.what()) + "\n" + text);
      continue;
    }
    const Json got = normal(jsonld::parse(text)), want = normal(jsonld::parse(c.expected));
    const bool alike = jsonld::same(got, want);
    s.require("jsonld written as pyld compacts it: " + c.name, alike, alike ? "" : text + "\nwanted " + jsonld::canonical(want));
    std::string why;
    s.require("jsonld written and read back the same: " + c.name, same_dataset(jsonld::read(text).quads, quads, why), why);
  }
  {
    std::size_t back = 0;
    std::string bad;
    for (const auto& c : fixtures::to_rdf) {
      jsonld::Options o;
      o.base = c.base;
      o.documents = c.documents;
      const auto d = jsonld::read(c.input, o);
      std::string why;
      if (same_dataset(jsonld::read(jsonld::write(d.quads, d.prefixes)).quads, d.quads, why)) back++;
      else bad += c.name + ": " + why + "; ";
    }
    s.require("jsonld: every document read, written back and read again into the same statements", back == fixtures::to_rdf.size(), bad);
  }
  s.require("jsonld: every fixture agrees with pyld", agreed == fixtures::to_rdf.size(), std::to_string(agreed) + " of " + std::to_string(fixtures::to_rdf.size()));
  return s.done();
}
