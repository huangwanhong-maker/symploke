// Shapes: what a graph must hold, declared in RDF and checked, every failure named with its node, its path, its value and the
// constraint it breaks.
//
// The Shapes Constraint Language (SHACL, W3C Recommendation 2017) states conditions on the nodes of a data graph in a shapes
// graph. Written here is SHACL Core's validation of node shapes and property shapes (§2–§4):
//
//   - targets: sh:targetNode, sh:targetClass (with rdfs:subClassOf followed in the data graph, §2.1.3.3), an implicit class
//     target when the shape is itself an rdfs:Class, sh:targetSubjectsOf, sh:targetObjectsOf;
//   - paths: a predicate, sh:inversePath, a sequence written as an RDF list, sh:alternativePath, sh:zeroOrMorePath,
//     sh:oneOrMorePath and sh:zeroOrOnePath (§2.3.1);
//   - value types: sh:class, sh:datatype (and a well-formed lexical form for XSD types), sh:nodeKind;
//   - cardinality: sh:minCount, sh:maxCount;
//   - values: sh:minInclusive, sh:maxInclusive, sh:minExclusive, sh:maxExclusive by SPARQL's comparison; sh:minLength,
//     sh:maxLength, sh:pattern (with sh:flags), sh:languageIn, sh:uniqueLang; sh:in, sh:hasValue;
//   - pairs: sh:equals, sh:disjoint, sh:lessThan, sh:lessThanOrEquals;
//   - logic and nesting: sh:not, sh:and, sh:or, sh:xone, sh:node, sh:property; sh:closed with sh:ignoredProperties;
//   - sh:deactivated, sh:severity (sh:Violation by default) and sh:message.
//
// A validation report says whether the data conforms and lists every result: focus node, path, value, source shape,
// constraint component and severity (§3.6). It can be written back as RDF in SHACL's vocabulary. sh:sparql, the advanced
// features and shapes recursion are not supported; a shape that refers to itself through sh:node is refused rather than
// looped on.

#pragma once

#include <symploke/query/query.hpp>
#include <symploke/store/store.hpp>

#include <algorithm>
#include <functional>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace symploke::shapes {

using namespace symploke::terms;

struct Result {
  Term focus, value, shape;
  std::string path;       // the path as written in the shapes graph, in Turtle-like form
  std::string component;  // e.g. sh:MinCountConstraintComponent
  std::string severity = "Violation";
  std::string message;
  bool has_value = false;
};

struct Report {
  bool conforms = true;
  std::vector<Result> results;
};

namespace detail {

using store::Dataset;
using store::Pattern;

inline std::string SH(const std::string& local) { return vocabulary::sh + local; }

inline std::vector<Term> objects(const Dataset& d, const Term& s, const std::string& p, const Term& g) {
  std::vector<Term> v;
  for (const auto& q : d.match({s, iri(p), std::nullopt, g})) v.push_back(q.o);
  return v;
}
inline std::optional<Term> object(const Dataset& d, const Term& s, const std::string& p, const Term& g) {
  const auto v = objects(d, s, p, g);
  if (v.empty()) return std::nullopt;
  return v.front();
}
inline std::vector<Term> list(const Dataset& d, Term head, const Term& g) {
  std::vector<Term> items;
  std::set<Term> seen;
  while (!(head.is_iri() && head.value == vocabulary::rdf_nil)) {
    if (!seen.insert(head).second) throw std::invalid_argument("symploke::shapes: an RDF list that loops");
    const auto first = object(d, head, vocabulary::rdf_first, g);
    const auto rest = object(d, head, vocabulary::rdf_rest, g);
    if (!first || !rest) throw std::invalid_argument("symploke::shapes: a malformed RDF list at " + nt(head));
    items.push_back(*first);
    head = *rest;
  }
  return items;
}

class Validator {
 public:
  Validator(const Dataset& data, const Term& data_graph, const Dataset& shapes, const Term& shapes_graph) : d_(data), dg_(data_graph), s_(shapes), sg_(shapes_graph) {}

  Report run() {
    Report r;
    std::set<Term> shapes;
    for (const auto& q : s_.match({std::nullopt, iri(vocabulary::rdf_type), iri(SH("NodeShape")), sg_})) shapes.insert(q.s);
    for (const auto& q : s_.match({std::nullopt, iri(vocabulary::rdf_type), iri(SH("PropertyShape")), sg_})) shapes.insert(q.s);
    for (const auto& p : {"targetNode", "targetClass", "targetSubjectsOf", "targetObjectsOf"})
      for (const auto& q : s_.match({std::nullopt, iri(SH(p)), std::nullopt, sg_})) shapes.insert(q.s);
    for (const auto& shape : shapes) {
      for (const auto& focus : targets(shape)) validate(shape, focus, r.results);
    }
    r.conforms = r.results.empty();
    return r;
  }

  std::set<Term> targets(const Term& shape) {
    std::set<Term> t;
    for (const auto& n : objects(s_, shape, SH("targetNode"), sg_)) t.insert(n);
    std::vector<Term> classes = objects(s_, shape, SH("targetClass"), sg_);
    if (!s_.match({shape, iri(vocabulary::rdf_type), iri(vocabulary::rdfs + "Class"), sg_}).empty()) classes.push_back(shape);
    for (const auto& c : classes)
      for (const auto& n : instances(c)) t.insert(n);
    for (const auto& p : objects(s_, shape, SH("targetSubjectsOf"), sg_))
      for (const auto& q : d_.match({std::nullopt, p, std::nullopt, dg_})) t.insert(q.s);
    for (const auto& p : objects(s_, shape, SH("targetObjectsOf"), sg_))
      for (const auto& q : d_.match({std::nullopt, p, std::nullopt, dg_})) t.insert(q.o);
    return t;
  }

  // Instances of a class and of its subclasses in the data graph.
  std::set<Term> instances(const Term& c) {
    std::set<Term> classes{c}, out;
    std::vector<Term> stack{c};
    while (!stack.empty()) {
      const Term k = stack.back();
      stack.pop_back();
      for (const auto& q : d_.match({std::nullopt, iri(vocabulary::rdfs + "subClassOf"), k, dg_}))
        if (classes.insert(q.s).second) stack.push_back(q.s);
    }
    for (const auto& k : classes)
      for (const auto& q : d_.match({std::nullopt, iri(vocabulary::rdf_type), k, dg_})) out.insert(q.s);
    return out;
  }

  bool is_instance(const Term& node, const Term& c) {
    std::set<Term> seen;
    std::vector<Term> stack;
    for (const auto& q : d_.match({node, iri(vocabulary::rdf_type), std::nullopt, dg_})) stack.push_back(q.o);
    while (!stack.empty()) {
      const Term k = stack.back();
      stack.pop_back();
      if (k == c) return true;
      if (!seen.insert(k).second) continue;
      for (const auto& q : d_.match({k, iri(vocabulary::rdfs + "subClassOf"), std::nullopt, dg_})) stack.push_back(q.o);
    }
    return false;
  }

  // ---- Paths ----
  std::string path_text(const Term& p) {
    if (p.is_iri()) {
      const auto c = common_prefixes().compact(p.value);
      return c ? *c : nt(p);
    }
    if (const auto inv = object(s_, p, SH("inversePath"), sg_)) return "^" + path_text(*inv);
    if (const auto alt = object(s_, p, SH("alternativePath"), sg_)) {
      std::string s;
      for (const auto& x : list(s_, *alt, sg_)) s += (s.empty() ? "" : "|") + path_text(x);
      return "(" + s + ")";
    }
    for (const auto& [prop, mark] : std::vector<std::pair<std::string, std::string>>{{"zeroOrMorePath", "*"}, {"oneOrMorePath", "+"}, {"zeroOrOnePath", "?"}})
      if (const auto x = object(s_, p, SH(prop), sg_)) return path_text(*x) + mark;
    if (object(s_, p, vocabulary::rdf_first, sg_)) {
      std::string s;
      for (const auto& x : list(s_, p, sg_)) s += (s.empty() ? "" : "/") + path_text(x);
      return s;
    }
    return nt(p);
  }

  std::set<Term> step(const Term& path, const std::set<Term>& from) {
    std::set<Term> out;
    if (path.is_iri()) {
      for (const auto& f : from)
        for (const auto& q : d_.match({f, path, std::nullopt, dg_})) out.insert(q.o);
      return out;
    }
    if (const auto inv = object(s_, path, SH("inversePath"), sg_)) {
      if (!inv->is_iri()) throw std::invalid_argument("symploke::shapes: only a predicate may be inverted");
      for (const auto& f : from)
        for (const auto& q : d_.match({std::nullopt, *inv, f, dg_})) out.insert(q.s);
      return out;
    }
    if (const auto alt = object(s_, path, SH("alternativePath"), sg_)) {
      for (const auto& x : list(s_, *alt, sg_))
        for (const auto& t : step(x, from)) out.insert(t);
      return out;
    }
    const auto closure = [&](const Term& inner, bool zero) {
      std::set<Term> seen = zero ? from : std::set<Term>{};
      std::set<Term> frontier = step(inner, from);
      while (!frontier.empty()) {
        std::set<Term> next;
        for (const auto& t : frontier)
          if (seen.insert(t).second) next.insert(t);
        frontier = step(inner, next);
      }
      return seen;
    };
    if (const auto x = object(s_, path, SH("zeroOrMorePath"), sg_)) return closure(*x, true);
    if (const auto x = object(s_, path, SH("oneOrMorePath"), sg_)) return closure(*x, false);
    if (const auto x = object(s_, path, SH("zeroOrOnePath"), sg_)) {
      out = step(*x, from);
      out.insert(from.begin(), from.end());
      return out;
    }
    if (object(s_, path, vocabulary::rdf_first, sg_)) {
      std::set<Term> cur = from;
      for (const auto& x : list(s_, path, sg_)) cur = step(x, cur);
      return cur;
    }
    throw std::invalid_argument("symploke::shapes: an unreadable path at " + nt(path));
  }

  // ---- Validation of one focus node ----
  std::vector<std::string> active_;  // shapes on the stack, to refuse recursion

  bool conforms(const Term& shape, const Term& focus) {
    std::vector<Result> r;
    validate(shape, focus, r);
    return r.empty();
  }

  void validate(const Term& shape, const Term& focus, std::vector<Result>& out) {
    if (const auto deact = object(s_, shape, SH("deactivated"), sg_); deact && deact->value == "true") return;
    const std::string key = nt(shape) + " " + nt(focus);
    if (std::find(active_.begin(), active_.end(), key) != active_.end()) throw std::invalid_argument("symploke::shapes: the shape " + nt(shape) + " refers to itself; recursion is not supported");
    active_.push_back(key);
    std::string severity = "Violation";
    if (const auto sev = object(s_, shape, SH("severity"), sg_)) severity = sev->value.substr(sev->value.find('#') + 1);
    const auto message = object(s_, shape, SH("message"), sg_);
    const auto path = object(s_, shape, SH("path"), sg_);
    std::vector<Term> values;
    std::string ptext;
    if (path) {
      const auto vs = step(*path, {focus});
      values.assign(vs.begin(), vs.end());
      ptext = path_text(*path);
    } else {
      values = {focus};
    }
    const auto fail = [&](const std::string& component, const std::optional<Term>& value, const std::string& why) {
      Result res;
      res.focus = focus;
      res.shape = shape;
      res.path = ptext;
      res.component = "sh:" + component + "ConstraintComponent";
      res.severity = severity;
      res.message = message ? message->value : why;
      if (value) res.value = *value, res.has_value = true;
      out.push_back(std::move(res));
    };
    const auto number = [&](const std::string& p) -> std::optional<long long> {
      const auto v = object(s_, shape, SH(p), sg_);
      if (!v) return std::nullopt;
      return std::stoll(v->value);
    };
    if (path) {
      if (const auto n = number("minCount"); n && static_cast<long long>(values.size()) < *n) fail("MinCount", std::nullopt, "fewer than " + std::to_string(*n) + " values");
      if (const auto n = number("maxCount"); n && static_cast<long long>(values.size()) > *n) fail("MaxCount", std::nullopt, "more than " + std::to_string(*n) + " values");
    }
    for (const auto& c : objects(s_, shape, SH("class"), sg_))
      for (const auto& v : values)
        if (v.is_literal() || !is_instance(v, c)) fail("Class", v, "not an instance of " + nt(c));
    if (const auto dt = object(s_, shape, SH("datatype"), sg_))
      for (const auto& v : values) {
        const bool ok = v.is_literal() && v.datatype == dt->value && value(v).type != Value::Type::ill_typed;
        if (!ok) fail("Datatype", v, "not a well-formed literal of " + nt(*dt));
      }
    if (const auto kind = object(s_, shape, SH("nodeKind"), sg_)) {
      const std::string k = kind->value.substr(kind->value.find('#') + 1);
      for (const auto& v : values) {
        const bool ok = (k == "IRI" && v.is_iri()) || (k == "Literal" && v.is_literal()) || (k == "BlankNode" && v.is_blank()) || (k == "BlankNodeOrIRI" && !v.is_literal()) ||
                        (k == "IRIOrLiteral" && !v.is_blank()) || (k == "BlankNodeOrLiteral" && !v.is_iri());
        if (!ok) fail("NodeKind", v, "not of the kind " + k);
      }
    }
    for (const auto& [p, cmp, comp] : std::vector<std::tuple<std::string, std::function<bool(int)>, std::string>>{
             {"minInclusive", [](int c) { return c >= 0; }, "MinInclusive"},
             {"maxInclusive", [](int c) { return c <= 0; }, "MaxInclusive"},
             {"minExclusive", [](int c) { return c > 0; }, "MinExclusive"},
             {"maxExclusive", [](int c) { return c < 0; }, "MaxExclusive"}}) {
      const auto bound = object(s_, shape, SH(p), sg_);
      if (!bound) continue;
      for (const auto& v : values) {
        bool ok = false;
        try {
          ok = cmp(query::detail::compare(v, *bound));
        } catch (const query::detail::Fail&) {
          ok = false;
        }
        if (!ok) fail(comp, v, "outside the bound " + nt(*bound));
      }
    }
    const auto length = [](const std::string& s) {
      std::size_t n = 0;
      for (const unsigned char c : s) n += (c & 0xC0) != 0x80;
      return static_cast<long long>(n);
    };
    if (const auto n = number("minLength"))
      for (const auto& v : values)
        if (v.is_blank() || length(v.value) < *n) fail("MinLength", v, "shorter than " + std::to_string(*n));
    if (const auto n = number("maxLength"))
      for (const auto& v : values)
        if (v.is_blank() || length(v.value) > *n) fail("MaxLength", v, "longer than " + std::to_string(*n));
    if (const auto pat = object(s_, shape, SH("pattern"), sg_)) {
      auto syntax = std::regex::ECMAScript;
      if (const auto flags = object(s_, shape, SH("flags"), sg_); flags && flags->value.find('i') != std::string::npos) syntax |= std::regex::icase;
      const std::regex re(pat->value, syntax);
      for (const auto& v : values)
        if (v.is_blank() || !std::regex_search(v.value, re)) fail("Pattern", v, "does not match " + pat->value);
    }
    if (const auto langs = object(s_, shape, SH("languageIn"), sg_)) {
      const auto allowed = list(s_, *langs, sg_);
      for (const auto& v : values) {
        bool ok = false;
        for (const auto& a : allowed) {
          std::string range = a.value;
          std::transform(range.begin(), range.end(), range.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
          ok = ok || (v.is_literal() && (v.language == range || v.language.rfind(range + "-", 0) == 0));
        }
        if (!ok) fail("LanguageIn", v, "not in an allowed language");
      }
    }
    if (const auto u = object(s_, shape, SH("uniqueLang"), sg_); u && u->value == "true") {
      std::map<std::string, int> count;
      for (const auto& v : values)
        if (v.is_literal() && !v.language.empty()) count[v.language]++;
      for (const auto& [lang, n] : count)
        if (n > 1) fail("UniqueLang", std::nullopt, "more than one value in the language " + lang);
    }
    if (const auto in = object(s_, shape, SH("in"), sg_)) {
      const auto allowed = list(s_, *in, sg_);
      for (const auto& v : values)
        if (std::find(allowed.begin(), allowed.end(), v) == allowed.end()) fail("In", v, "not among the allowed values");
    }
    for (const auto& hv : objects(s_, shape, SH("hasValue"), sg_))
      if (std::find(values.begin(), values.end(), hv) == values.end()) fail("HasValue", std::nullopt, "lacks the value " + nt(hv));
    const auto pair_values = [&](const Term& p) {
      std::set<Term> v;
      for (const auto& q : d_.match({focus, p, std::nullopt, dg_})) v.insert(q.o);
      return v;
    };
    for (const auto& p : objects(s_, shape, SH("equals"), sg_)) {
      const auto other = pair_values(p);
      const std::set<Term> mine(values.begin(), values.end());
      for (const auto& v : mine)
        if (!other.count(v)) fail("Equals", v, "not also a value of " + nt(p));
      for (const auto& v : other)
        if (!mine.count(v)) fail("Equals", v, "a value of " + nt(p) + " not on the path");
    }
    for (const auto& p : objects(s_, shape, SH("disjoint"), sg_)) {
      const auto other = pair_values(p);
      for (const auto& v : values)
        if (other.count(v)) fail("Disjoint", v, "also a value of " + nt(p));
    }
    for (const auto& [prop, strict] : std::vector<std::pair<std::string, bool>>{{"lessThan", true}, {"lessThanOrEquals", false}})
      for (const auto& p : objects(s_, shape, SH(prop), sg_))
        for (const auto& v : values)
          for (const auto& w : pair_values(p)) {
            bool ok = false;
            try {
              const int c = query::detail::compare(v, w);
              ok = strict ? c < 0 : c <= 0;
            } catch (const query::detail::Fail&) {
              ok = false;
            }
            if (!ok) fail(strict ? "LessThan" : "LessThanOrEquals", v, "not less than " + nt(w));
          }
    // Logic and nesting, on each value.
    for (const auto& n : objects(s_, shape, SH("not"), sg_))
      for (const auto& v : values)
        if (conforms(n, v)) fail("Not", v, "conforms to a shape it must not");
    for (const auto& a : objects(s_, shape, SH("and"), sg_)) {
      const auto members = list(s_, a, sg_);
      for (const auto& v : values)
        for (const auto& m : members)
          if (!conforms(m, v)) {
            fail("And", v, "fails a member of sh:and");
            break;
          }
    }
    for (const auto& o : objects(s_, shape, SH("or"), sg_)) {
      const auto members = list(s_, o, sg_);
      for (const auto& v : values) {
        bool any = false;
        for (const auto& m : members) any = any || conforms(m, v);
        if (!any) fail("Or", v, "conforms to no member of sh:or");
      }
    }
    for (const auto& x : objects(s_, shape, SH("xone"), sg_)) {
      const auto members = list(s_, x, sg_);
      for (const auto& v : values) {
        int n = 0;
        for (const auto& m : members) n += conforms(m, v);
        if (n != 1) fail("Xone", v, "conforms to " + std::to_string(n) + " members of sh:xone, not exactly one");
      }
    }
    for (const auto& n : objects(s_, shape, SH("node"), sg_))
      for (const auto& v : values)
        if (!conforms(n, v)) fail("Node", v, "does not conform to " + nt(n));
    for (const auto& ps : objects(s_, shape, SH("property"), sg_))
      for (const auto& v : values) validate(ps, v, out);
    if (const auto closed = object(s_, shape, SH("closed"), sg_); closed && closed->value == "true") {
      std::set<Term> allowed;
      for (const auto& ps : objects(s_, shape, SH("property"), sg_))
        if (const auto p = object(s_, ps, SH("path"), sg_); p && p->is_iri()) allowed.insert(*p);
      if (const auto ig = object(s_, shape, SH("ignoredProperties"), sg_))
        for (const auto& p : list(s_, *ig, sg_)) allowed.insert(p);
      for (const auto& v : values)
        for (const auto& q : d_.match({v, std::nullopt, std::nullopt, dg_}))
          if (!allowed.count(q.p)) fail("Closed", q.o, "the property " + nt(q.p) + " is not allowed");
    }
    active_.pop_back();
  }

 private:
  const Dataset& d_;
  Term dg_;
  const Dataset& s_;
  Term sg_;
};

}  // namespace detail

inline Report validate(const store::Dataset& data, const store::Dataset& shapes, const Term& data_graph = {}, const Term& shapes_graph = {}) {
  return detail::Validator(data, data_graph, shapes, shapes_graph).run();
}

// The report as RDF in SHACL's vocabulary (§3.6), its nodes blank.
inline std::vector<Triple> report_graph(const Report& r) {
  using detail::SH;
  std::vector<Triple> t;
  const Term report = blank("report");
  t.push_back({report, iri(vocabulary::rdf_type), iri(SH("ValidationReport"))});
  t.push_back({report, iri(SH("conforms")), literal(r.conforms ? "true" : "false", vocabulary::xsd + "boolean")});
  for (std::size_t k = 0; k < r.results.size(); k++) {
    const auto& x = r.results[k];
    const Term res = blank("result" + std::to_string(k));
    t.push_back({report, iri(SH("result")), res});
    t.push_back({res, iri(vocabulary::rdf_type), iri(SH("ValidationResult"))});
    t.push_back({res, iri(SH("focusNode")), x.focus});
    t.push_back({res, iri(SH("sourceShape")), x.shape});
    t.push_back({res, iri(SH("sourceConstraintComponent")), iri(SH(x.component.substr(3)))});
    t.push_back({res, iri(SH("resultSeverity")), iri(SH(x.severity))});
    if (x.has_value) t.push_back({res, iri(SH("value")), x.value});
    if (!x.message.empty()) t.push_back({res, iri(SH("resultMessage")), literal(x.message)});
  }
  return t;
}

}  // namespace symploke::shapes
