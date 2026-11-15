// Checks of RDF terms (terms.hpp): absolute IRIs and language tags told from malformed ones; terms' faults; prefixes expanded
// and compacted to the longest namespace; the values of literals compared as SPARQL compares them (numbers across types,
// dates by their instants, ill-typed lexical forms named); terms written in N-Triples with its escapes.

#include <symploke/terms/terms.hpp>

#include <calx/verification/check.hpp>

#include <string>

using namespace symploke::terms;

int main() {
  calx::verification::Suite s;
  s.require("absolute IRIs are accepted, relative ones and those with spaces or brackets refused",
            valid_iri("http://example.org/a#b") && valid_iri("urn:isbn:0451450523") && !valid_iri("a/b") && !valid_iri("http://ex.org/a b") && !valid_iri("http://ex.org/<a>") &&
                !valid_iri(":x") && !valid_iri("1http://x"));
  s.require("language tags of letters, then letters and digits, are accepted; empty parts and long ones are not",
            valid_language("en") && valid_language("zh-Hant-TW") && valid_language("de-CH-1996") && !valid_language("") && !valid_language("en-") && !valid_language("toolongtag") &&
                !valid_language("1en"));
  s.require("a literal cannot be a subject; a predicate is an IRI; a language tag only on rdf:langString",
            fault(Triple{literal("x"), iri("http://e/p"), iri("http://e/o")}) == "a literal cannot be a subject" && fault(Triple{iri("http://e/s"), blank("b"), iri("http://e/o")}) == "a predicate is an IRI" &&
                !fault(Term{Kind::literal, "x", vocabulary::xsd_string, "en"}).empty() && fault(lang("chat", "FR")).empty() && lang("chat", "FR").language == "fr");
  auto p = common_prefixes();
  p.map["ex"] = "http://example.org/";
  p.map["exa"] = "http://example.org/a/";
  s.require("prefixes expand, and compact to the longest namespace whose remainder is a plain name",
            *p.expand("ex:b") == "http://example.org/b" && *p.compact("http://example.org/a/c") == "exa:c" && *p.compact(vocabulary::rdf_type) == "rdf:type" &&
                !p.compact("http://example.org/a b").has_value() && !p.expand("nope:x").has_value());
  const auto i = value(literal("42", vocabulary::xsd + "integer")), d = value(literal("42.0", vocabulary::xsd + "decimal")), f = value(literal("4.2E1", vocabulary::xsd + "double"));
  s.require("42, 42.0 and 4.2E1 have the same number", i.number == 42 && d.number == 42 && f.number == 42 && i.type == Value::Type::integer && f.type == Value::Type::floating);
  const auto a = value(literal("2026-11-15T10:00:00+01:00", vocabulary::xsd + "dateTime")), b = value(literal("2026-11-15T09:00:00Z", vocabulary::xsd + "dateTime"));
  s.require("10:00 at UTC+1 and 09:00 UTC are one instant", a.number == b.number && a.zoned && b.zoned);
  s.check("1970-01-02 is 86,400 seconds after the epoch", value(literal("1970-01-02", vocabulary::xsd + "date")).number - 86400, 0);
  s.require("ill-typed lexical forms are named so", value(literal("forty", vocabulary::xsd + "integer")).type == Value::Type::ill_typed &&
                                                         value(literal("2026-13-01", vocabulary::xsd + "date")).type == Value::Type::ill_typed &&
                                                         value(literal("maybe", vocabulary::xsd + "boolean")).type == Value::Type::ill_typed);
  s.require("terms in N-Triples, with quotes, backslashes, line breaks and controls escaped",
            nt(literal("say \"hi\"\n\\")) == "\"say \\\"hi\\\"\\n\\\\\"" && nt(lang("été", "fr")) == "\"été\"@fr" && nt(literal("1", vocabulary::xsd + "integer")) == "\"1\"^^<http://www.w3.org/2001/XMLSchema#integer>" &&
                nt(blank("b0")) == "_:b0" && nt(iri("http://e/a")) == "<http://e/a>" && nt(literal(std::string("a\x01"))) == "\"a\\u0001\"");
  return s.done();
}
