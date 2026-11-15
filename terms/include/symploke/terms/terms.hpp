// The terms of RDF: IRIs, blank nodes and literals; triples and quads; prefixes; the values of the common datatypes.
//
// RDF describes a world as a set of statements, each a triple (subject, predicate, object): the subject an IRI or a blank
// node, the predicate an IRI, the object an IRI, a blank node or a literal (RDF 1.1 Concepts and Abstract Syntax, W3C
// Recommendation 2014, §3). A literal has a lexical form and a datatype IRI, and a language tag exactly when its datatype
// is rdf:langString; a literal written without either is an xsd:string. A dataset is a default graph and named graphs,
// each statement then a quad (§4).
//
//   - Term: an IRI, a blank node (its label is local to a document or a store) or a literal. Terms are values, compared by
//     kind, then text, then datatype, then language; a language tag is kept in lower case, since tags compare without case
//     (§3.3, after BCP 47).
//   - well-formedness: an IRI must be absolute and free of the characters N-Triples forbids in one (space, < > " { } | ^ `
//     \ and controls); a language tag must match [a-zA-Z]{1,8}(-[a-zA-Z0-9]{1,8})*; a literal of a known XSD datatype must
//     have a lexical form in its lexical space (an ill-typed literal is not refused as a term, but value() says it is).
//   - Prefixes: a map from prefixes to namespaces that expands prefixed names and compacts IRIs to the longest namespace.
//   - value(): the value of a literal of xsd:string, boolean, integer (and its derived types), decimal, double, float, date
//     and dateTime, by which SPARQL compares and orders them (XML Schema 1.1 Part 2, §3.3): numbers as numbers, booleans,
//     dates and times by their instants where comparable.
//   - nt(): a term in N-Triples, with the escapes of the N-Triples grammar (RDF 1.1 N-Triples, §2).
//
// Nothing here reads a file: syntax.hpp parses and writes documents.

#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace symploke::terms {

namespace vocabulary {
inline const std::string rdf = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
inline const std::string rdfs = "http://www.w3.org/2000/01/rdf-schema#";
inline const std::string xsd = "http://www.w3.org/2001/XMLSchema#";
inline const std::string owl = "http://www.w3.org/2002/07/owl#";
inline const std::string sh = "http://www.w3.org/ns/shacl#";
inline const std::string rdf_type = rdf + "type";
inline const std::string rdf_first = rdf + "first";
inline const std::string rdf_rest = rdf + "rest";
inline const std::string rdf_nil = rdf + "nil";
inline const std::string lang_string = rdf + "langString";
inline const std::string xsd_string = xsd + "string";
}  // namespace vocabulary

enum class Kind : std::uint8_t { iri, blank, literal };

struct Term {
  Kind kind = Kind::iri;
  std::string value;     // the IRI, the blank node's label, or the literal's lexical form
  std::string datatype;  // literals: the datatype IRI
  std::string language;  // literals of rdf:langString: the tag, in lower case

  bool is_iri() const { return kind == Kind::iri; }
  bool is_blank() const { return kind == Kind::blank; }
  bool is_literal() const { return kind == Kind::literal; }
  bool empty() const { return kind == Kind::iri && value.empty(); }  // the default graph's name
  auto key() const { return std::tie(kind, value, datatype, language); }
  bool operator==(const Term& o) const { return key() == o.key(); }
  bool operator!=(const Term& o) const { return !(*this == o); }
  bool operator<(const Term& o) const { return key() < o.key(); }
};

inline Term iri(std::string s) { return {Kind::iri, std::move(s), {}, {}}; }
inline Term blank(std::string label) { return {Kind::blank, std::move(label), {}, {}}; }
inline Term literal(std::string lexical, std::string datatype = vocabulary::xsd_string) { return {Kind::literal, std::move(lexical), std::move(datatype), {}}; }
inline Term lang(std::string lexical, std::string tag) {
  std::transform(tag.begin(), tag.end(), tag.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return {Kind::literal, std::move(lexical), vocabulary::lang_string, std::move(tag)};
}
inline Term default_graph() { return {}; }

struct Triple {
  Term s, p, o;
  auto key() const { return std::tie(s, p, o); }
  bool operator==(const Triple& t) const { return key() == t.key(); }
  bool operator<(const Triple& t) const { return key() < t.key(); }
};

struct Quad {
  Term s, p, o, g;  // g empty for the default graph
  auto key() const { return std::tie(g, s, p, o); }
  bool operator==(const Quad& q) const { return key() == q.key(); }
  bool operator<(const Quad& q) const { return key() < q.key(); }
  Triple triple() const { return {s, p, o}; }
};

// ---- Well-formedness ------------------------------------------------------------------------------------------------------------

inline bool valid_iri(const std::string& s) {
  const auto colon = s.find(':');
  if (colon == std::string::npos || colon == 0) return false;
  if (!std::isalpha(static_cast<unsigned char>(s[0]))) return false;
  for (std::size_t i = 1; i < colon; i++) {
    const char c = s[i];
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '+' && c != '-' && c != '.') return false;
  }
  for (const unsigned char c : s)
    if (c <= 0x20 || c == '<' || c == '>' || c == '"' || c == '{' || c == '}' || c == '|' || c == '^' || c == '`' || c == '\\') return false;
  return true;
}

inline bool valid_language(const std::string& tag) {
  if (tag.empty()) return false;
  std::size_t i = 0, part = 0;
  while (i < tag.size()) {
    std::size_t j = i;
    while (j < tag.size() && tag[j] != '-') j++;
    const std::size_t len = j - i;
    if (len < 1 || len > 8) return false;
    for (std::size_t k = i; k < j; k++) {
      const unsigned char c = static_cast<unsigned char>(tag[k]);
      if (part == 0 ? !std::isalpha(c) : !std::isalnum(c)) return false;
    }
    part++;
    i = j + 1;
    if (j < tag.size() && i == tag.size()) return false;  // a trailing hyphen
  }
  return true;
}

// Why a term cannot stand where it stands, or "" when it can.
inline std::string fault(const Term& t) {
  switch (t.kind) {
    case Kind::iri: return valid_iri(t.value) ? "" : "<" + t.value + "> is not an absolute IRI";
    case Kind::blank: return t.value.empty() ? "a blank node without a label" : "";
    case Kind::literal:
      if (!valid_iri(t.datatype)) return "the literal \"" + t.value + "\" has no valid datatype";
      if (t.datatype == vocabulary::lang_string ? !valid_language(t.language) : !t.language.empty()) return "the literal \"" + t.value + "\" has a language tag exactly when it is a rdf:langString";
      return "";
  }
  return "";
}

inline std::string fault(const Triple& t) {
  if (t.s.is_literal()) return "a literal cannot be a subject";
  if (!t.p.is_iri()) return "a predicate is an IRI";
  for (const auto* x : {&t.s, &t.p, &t.o})
    if (auto f = fault(*x); !f.empty()) return f;
  return "";
}

// ---- Prefixes -----------------------------------------------------------------------------------------------------------------------

struct Prefixes {
  std::map<std::string, std::string> map;  // prefix → namespace

  std::optional<std::string> expand(const std::string& prefixed) const {
    const auto colon = prefixed.find(':');
    if (colon == std::string::npos) return std::nullopt;
    const auto it = map.find(prefixed.substr(0, colon));
    if (it == map.end()) return std::nullopt;
    return it->second + prefixed.substr(colon + 1);
  }
  // The IRI as prefix:local with the longest namespace whose remainder is a plain local name, or nothing.
  std::optional<std::string> compact(const std::string& iri) const {
    std::optional<std::string> best;
    std::size_t longest = 0;
    for (const auto& [p, ns] : map) {
      if (ns.size() <= longest || iri.compare(0, ns.size(), ns) != 0) continue;
      const std::string local = iri.substr(ns.size());
      bool plain = true;
      for (std::size_t i = 0; i < local.size() && plain; i++) {
        const unsigned char c = static_cast<unsigned char>(local[i]);
        plain = std::isalnum(c) || c == '_' || (i > 0 && (c == '-' || (c == '.' && i + 1 < local.size()))) || c >= 0x80;
      }
      if (plain) best = p + ":" + local, longest = ns.size();
    }
    return best;
  }
};

inline Prefixes common_prefixes() {
  using namespace vocabulary;
  return {{{"rdf", rdf}, {"rdfs", rdfs}, {"xsd", xsd}, {"owl", owl}, {"sh", sh}, {"dcterms", "http://purl.org/dc/terms/"}, {"skos", "http://www.w3.org/2004/02/skos/core#"},
           {"prov", "http://www.w3.org/ns/prov#"}, {"foaf", "http://xmlns.com/foaf/0.1/"}, {"schema", "https://schema.org/"}}};
}

// ---- Values of literals ---------------------------------------------------------------------------------------------------------------

struct Value {
  enum class Type { string, lang_string, boolean, integer, decimal, floating, date, datetime, other, ill_typed } type = Type::other;
  double number = 0;      // numbers, and dates and times as seconds since 1970-01-01T00:00:00Z (local times taken as UTC)
  bool truth = false;
  bool zoned = true;      // dates and times: whether a time zone was given
  std::string text;       // strings: the lexical form
};

namespace detail {
inline bool is_integer(const std::string& s) {
  std::size_t i = (!s.empty() && (s[0] == '+' || s[0] == '-')) ? 1 : 0;
  if (i == s.size()) return false;
  for (; i < s.size(); i++)
    if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
  return true;
}
inline bool is_decimal(const std::string& s) {
  std::size_t i = (!s.empty() && (s[0] == '+' || s[0] == '-')) ? 1 : 0;
  bool digits = false, dot = false;
  for (; i < s.size(); i++) {
    if (std::isdigit(static_cast<unsigned char>(s[i]))) digits = true;
    else if (s[i] == '.' && !dot) dot = true;
    else return false;
  }
  return digits;
}
inline bool is_double(const std::string& s) {
  if (s == "INF" || s == "+INF" || s == "-INF" || s == "NaN") return true;
  const auto e = s.find_first_of("eE");
  if (e == std::string::npos) return is_decimal(s);
  return is_decimal(s.substr(0, e)) && is_integer(s.substr(e + 1));
}
// Days from 1970-01-01 to y-m-d in the proleptic Gregorian calendar (Hinnant's algorithm).
inline long long days_from_civil(long long y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<long long>(doe) - 719468;
}
// YYYY-MM-DD[THH:MM:SS[.s]][Z|±HH:MM] → seconds; false when malformed.
inline bool instant(const std::string& s, bool with_time, double& seconds, bool& zoned) {
  std::size_t i = 0;
  const bool negative = !s.empty() && s[0] == '-';
  if (negative) i++;
  std::size_t y0 = i;
  while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) i++;
  if (i - y0 < 4 || i >= s.size() || s[i] != '-') return false;
  long long y = std::stoll(s.substr(y0, i - y0)) * (negative ? -1 : 1);
  const auto two = [&](std::size_t at, int& v) {
    if (at + 2 > s.size() || !std::isdigit(static_cast<unsigned char>(s[at])) || !std::isdigit(static_cast<unsigned char>(s[at + 1]))) return false;
    v = (s[at] - '0') * 10 + (s[at + 1] - '0');
    return true;
  };
  int mo = 0, d = 0, h = 0, mi = 0, se = 0;
  if (!two(i + 1, mo) || i + 3 >= s.size() || s[i + 3] != '-' || !two(i + 4, d)) return false;
  i += 6;
  if (mo < 1 || mo > 12 || d < 1 || d > 31) return false;
  double frac = 0;
  if (with_time) {
    if (i >= s.size() || s[i] != 'T' || !two(i + 1, h) || i + 3 >= s.size() || s[i + 3] != ':' || !two(i + 4, mi) || i + 6 >= s.size() || s[i + 6] != ':' || !two(i + 7, se)) return false;
    i += 9;
    if (i < s.size() && s[i] == '.') {
      std::size_t j = i + 1;
      while (j < s.size() && std::isdigit(static_cast<unsigned char>(s[j]))) j++;
      if (j == i + 1) return false;
      frac = std::stod("0" + s.substr(i, j - i));
      i = j;
    }
    if (h > 24 || mi > 59 || se > 60) return false;
  }
  double offset = 0;
  zoned = false;
  if (i < s.size()) {
    zoned = true;
    if (s[i] == 'Z' && i + 1 == s.size()) {
      offset = 0;
    } else if ((s[i] == '+' || s[i] == '-') && i + 6 == s.size() && s[i + 3] == ':') {
      int oh = 0, om = 0;
      if (!two(i + 1, oh) || !two(i + 4, om)) return false;
      offset = (s[i] == '-' ? -1 : 1) * (oh * 3600.0 + om * 60.0);
    } else {
      return false;
    }
  }
  seconds = static_cast<double>(days_from_civil(y, static_cast<unsigned>(mo), static_cast<unsigned>(d))) * 86400.0 + h * 3600.0 + mi * 60.0 + se + frac - offset;
  return true;
}
}  // namespace detail

inline Value value(const Term& t) {
  using T = Value::Type;
  Value v;
  if (!t.is_literal()) return v;
  const std::string& x = t.value;
  const std::string& dt = t.datatype;
  const std::string& X = vocabulary::xsd;
  if (dt == vocabulary::xsd_string) return {T::string, 0, false, true, x};
  if (dt == vocabulary::lang_string) return {T::lang_string, 0, false, true, x};
  if (dt == X + "boolean") {
    if (x == "true" || x == "1") return {T::boolean, 1, true, true, {}};
    if (x == "false" || x == "0") return {T::boolean, 0, false, true, {}};
    return {T::ill_typed, 0, false, true, x};
  }
  static const std::vector<std::string> integers = {"integer", "long", "int", "short", "byte", "nonNegativeInteger", "positiveInteger", "nonPositiveInteger", "negativeInteger",
                                                    "unsignedLong", "unsignedInt", "unsignedShort", "unsignedByte"};
  for (const auto& name : integers)
    if (dt == X + name) return detail::is_integer(x) ? Value{T::integer, std::stod(x), false, true, {}} : Value{T::ill_typed, 0, false, true, x};
  if (dt == X + "decimal") return detail::is_decimal(x) ? Value{T::decimal, std::stod(x), false, true, {}} : Value{T::ill_typed, 0, false, true, x};
  if (dt == X + "double" || dt == X + "float") {
    if (!detail::is_double(x)) return {T::ill_typed, 0, false, true, x};
    const double n = x == "INF" || x == "+INF" ? HUGE_VAL : x == "-INF" ? -HUGE_VAL : x == "NaN" ? std::nan("") : std::stod(x);
    return {T::floating, n, false, true, {}};
  }
  if (dt == X + "date" || dt == X + "dateTime") {
    double s = 0;
    bool zoned = true;
    if (!detail::instant(x, dt == X + "dateTime", s, zoned)) return {T::ill_typed, 0, false, true, x};
    return {dt == X + "date" ? T::date : T::datetime, s, false, zoned, {}};
  }
  return {T::other, 0, false, true, x};
}

// ---- N-Triples form of a term ------------------------------------------------------------------------------------------------------------

inline std::string escape(const std::string& s, bool iri) {
  std::string out;
  for (const unsigned char c : s) {
    if (iri) {
      if (c <= 0x20 || c == '<' || c == '>' || c == '"' || c == '{' || c == '}' || c == '|' || c == '^' || c == '`' || c == '\\') {
        char buf[8];
        std::snprintf(buf, sizeof buf, "\\u%04X", c);
        out += buf;
      } else {
        out += static_cast<char>(c);
      }
      continue;
    }
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20 || c == 0x7f) {
          char buf[8];
          std::snprintf(buf, sizeof buf, "\\u%04X", c);
          out += buf;
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  return out;
}

inline std::string nt(const Term& t) {
  switch (t.kind) {
    case Kind::iri: return "<" + escape(t.value, true) + ">";
    case Kind::blank: return "_:" + t.value;
    case Kind::literal: {
      std::string s = "\"" + escape(t.value, false) + "\"";
      if (t.datatype == vocabulary::lang_string) return s + "@" + t.language;
      if (t.datatype == vocabulary::xsd_string) return s;
      return s + "^^<" + escape(t.datatype, true) + ">";
    }
  }
  return {};
}

}  // namespace symploke::terms
