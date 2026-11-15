// JSON-LD 1.1: linked data written as JSON (JSON-LD 1.1, W3C Recommendation 2020; JSON-LD 1.1 Processing Algorithms and API,
// W3C Recommendation 2020, cited below as "API" with its section numbers).
//
//   - read(text, options): a document's statements. The document is expanded (API §5.1, with context processing §4.1, term
//     definitions §4.2, IRI expansion §5.2 and value expansion §5.3.2), and the expanded form turned into RDF (API §8.1–8.3).
//     Every keyword of the language is handled: contexts embedded, remote and imported, @vocab and @base, default languages
//     and directions, compact IRIs and prefixes, type coercion to IRIs, vocabulary terms, datatypes and @json, containers of
//     every kind (@list, @set, @language, @index with property-valued indexes, @id, @type, @graph and their combinations),
//     reverse properties, @nest, @included, scoped contexts on properties and on types with @propagate, and protected terms.
//   - Remote contexts are never fetched: a caller gives them, by IRI, in options.documents. One that is not given is refused
//     with the error "loading remote context failed", naming it in Error::missing, so that the edge may fetch it and ask again.
//   - Errors carry the API's error codes (§9.4.2), and the line and column of the value at fault.
//   - Numbers become xsd:integer or xsd:double by the API's rule (§8.3): a number with no fractional part below 10^21 is an
//     integer; doubles take the canonical form 1.1E0. Native values and @json literals are written as the API says.
//   - IRIs that remain relative after expansion, predicates that are blank nodes and language tags that are not well formed
//     yield no statement, as the API directs (§8.1–8.3); nothing else is dropped.
//   - from_rdf(quads): the expanded form of a dataset (API §8.4, with native types and rdf:type kept as @type), lists that are
//     well formed turned into @list.
//   - write(quads, prefixes): a document compacted against a context of the prefixes given: compact IRIs where a prefix fits,
//     strings plain, single values out of their arrays, one node as the document or several in @graph. It is what the API's
//     compaction (§6) gives for such a context, and it reads back as the same statements.
//
// The processing mode is json-ld-1.1. Framing, flattening and @direction's rdfDirection options are not here; a direction
// is kept in expansion and dropped in RDF, as the API's default (rdfDirection null) does.

#pragma once

#include <symploke/jsonld/json.hpp>
#include <symploke/syntax/syntax.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace symploke::jsonld {

using namespace symploke::terms;

class Error : public syntax::Error {
 public:
  std::string code;     // the API's error code (§9.4.2)
  std::string missing;  // for "loading remote context failed": the IRI of the context that was not given
  Error(const std::string& c, const std::string& detail, std::size_t l, std::size_t col, std::string m = {})
      : syntax::Error(c + (detail.empty() ? "" : ": " + detail), l, col), code(c), missing(std::move(m)) {}
};

struct Options {
  std::string base;                              // the document's own IRI; relative IRIs resolve against it
  std::map<std::string, std::string> documents;  // remote contexts by IRI, as their JSON text
};

namespace detail {

inline const std::string rdf_ns = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
inline const std::string xsd_ns = "http://www.w3.org/2001/XMLSchema#";

inline bool keyword(std::string_view s) {
  static const std::set<std::string, std::less<>> k = {"@base",  "@container", "@context", "@direction", "@graph",   "@id",
                                                       "@import", "@included", "@index",   "@json",      "@language", "@list",
                                                       "@nest",   "@none",     "@prefix",  "@propagate", "@protected", "@reverse",
                                                       "@set",    "@type",     "@value",   "@version",   "@vocab"};
  return k.count(s) > 0;
}

// "@" followed by one or more ALPHA: the form keywords take, reserved for future ones and ignored (API §4.2.2, §5.2.2).
inline bool keyword_form(std::string_view s) {
  if (s.size() < 2 || s[0] != '@') return false;
  for (std::size_t i = 1; i < s.size(); i++)
    if (!std::isalpha(static_cast<unsigned char>(s[i]))) return false;
  return true;
}

// An absolute IRI: a scheme (RFC 3986 §3.1) and a colon.
inline bool absolute(std::string_view s) {
  const auto colon = s.find(':');
  if (colon == std::string_view::npos || colon == 0 || !std::isalpha(static_cast<unsigned char>(s[0]))) return false;
  for (std::size_t i = 1; i < colon; i++) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if (!(std::isalnum(c) || c == '+' || c == '-' || c == '.')) return false;
  }
  return true;
}

inline bool blank_id(std::string_view s) { return s.size() >= 2 && s[0] == '_' && s[1] == ':'; }

inline bool gen_delim_end(std::string_view s) { return !s.empty() && std::string_view(":/?#[]@").find(s.back()) != std::string_view::npos; }

inline std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

inline std::string resolve(const std::optional<std::string>& base, const std::string& ref) {
  if (!base || base->empty() || absolute(ref)) return ref;
  return syntax::resolve(*base, ref);
}

struct TermDef {
  std::optional<std::string> iri;  // empty: the term maps to nothing (null)
  bool prefix = false, protect = false, reverse = false, simple = false;
  std::optional<std::string> type;  // @id, @vocab, @json, @none or a datatype IRI
  bool has_language = false, has_direction = false;
  std::optional<std::string> language, direction;  // with has_*: null when empty
  std::set<std::string> container;
  bool has_context = false;
  Json context;
  std::string context_base;
  std::optional<std::string> index, nest;

  bool same(const TermDef& o) const {
    return iri == o.iri && prefix == o.prefix && reverse == o.reverse && type == o.type && has_language == o.has_language && language == o.language &&
           has_direction == o.has_direction && direction == o.direction && container == o.container && has_context == o.has_context &&
           (!has_context || jsonld::same(context, o.context)) && index == o.index && nest == o.nest;
  }
};

struct Context {
  std::optional<std::string> base, vocab, language, direction;
  std::string original_base;
  std::map<std::string, TermDef> terms;
  std::shared_ptr<Context> previous;

  const TermDef* find(const std::optional<std::string>& t) const {
    if (!t) return nullptr;
    const auto it = terms.find(*t);
    return it == terms.end() ? nullptr : &it->second;
  }
  bool has_protected() const {
    for (const auto& [k, d] : terms)
      if (d.protect) return true;
    return false;
  }
};

[[noreturn]] inline void fail(const std::string& code, const std::string& detail, const Json& at) { throw Error(code, detail, at.line, at.column); }

inline Json as_array(const Json& v) { return v.is_array() ? v : Json::array({v}); }
inline bool value_object(const Json& v) { return v.is_object() && v.has("@value"); }
inline bool list_object(const Json& v) { return v.is_object() && v.has("@list"); }
inline bool graph_object(const Json& v) {
  if (!v.is_object() || !v.has("@graph")) return false;
  for (const auto& [k, x] : v.members)
    if (k != "@graph" && k != "@id" && k != "@index") return false;
  return true;
}
inline bool node_object(const Json& v) { return v.is_object() && !v.has("@value") && !v.has("@list") && !v.has("@set"); }

// Appends a value, or each value of an array, to the array under key (made if absent).
inline void add(Json& obj, const std::string& key, const Json& v) {
  if (!obj.has(key)) obj.set(key, Json::array());
  Json* cur = obj.get(key);
  if (!cur->is_array()) *cur = Json::array({*cur});
  if (v.is_array()) cur->items.insert(cur->items.end(), v.items.begin(), v.items.end());
  else cur->items.push_back(v);
}

inline std::vector<std::string> sorted_keys(const Json& obj) {
  std::vector<std::string> k;
  for (const auto& m : obj.members) k.push_back(m.first);
  std::sort(k.begin(), k.end());
  return k;
}

class Processor {
 public:
  explicit Processor(const Options& o) : opt_(o) {}

  Context initial() const {
    Context c;
    if (!opt_.base.empty()) c.base = opt_.base;
    c.original_base = opt_.base;
    return c;
  }

  // ---- Context processing (API §4.1.2) ----------------------------------------------------------------------------------------
  Context process(const Context& active, const Json& local, const std::string& base_url, std::vector<std::string> remote = {}, bool override_protected = false,
                  bool propagate = true, bool validate_scoped = true) const {
    Context result = active;
    if (local.is_object())
      if (const Json* p = local.get("@propagate")) {
        if (!p->is_bool()) fail("invalid @propagate value", "", *p);
        propagate = p->truth;
      }
    if (!propagate && !result.previous) result.previous = std::make_shared<Context>(active);
    std::vector<const Json*> items;
    if (local.is_array())
      for (const auto& x : local.items) items.push_back(&x);
    else items.push_back(&local);
    for (const Json* item : items) {
      const Json& ctx = *item;
      if (ctx.is_null()) {
        if (!override_protected && result.has_protected()) fail("invalid context nullification", "a context with protected terms cannot be cleared", ctx);
        Context fresh;
        if (!active.original_base.empty()) fresh.base = active.original_base;
        fresh.original_base = active.original_base;
        if (!propagate) fresh.previous = std::make_shared<Context>(result);
        result = std::move(fresh);
        continue;
      }
      if (ctx.is_string()) {
        const std::string url = resolve(base_url.empty() ? std::optional<std::string>{} : std::optional<std::string>{base_url}, ctx.text);
        if (!validate_scoped && std::find(remote.begin(), remote.end(), url) != remote.end()) continue;
        if (remote.size() > 32) fail("context overflow", url, ctx);
        remote.push_back(url);
        const Json doc = load(url, ctx);
        const Json* c = doc.is_object() ? doc.get("@context") : nullptr;
        if (!c) fail("invalid remote context", url + " has no @context", ctx);
        result = process(result, *c, url, remote, false, true, validate_scoped);
        continue;
      }
      if (!ctx.is_object()) fail("invalid local context", "a context is an object, a string or null", ctx);
      Json context = ctx;
      if (const Json* v = context.get("@version"))
        if (!(v->is_number() && v->number == 1.1)) fail("invalid @version value", "", *v);
      if (const Json* imp = context.get("@import")) {
        if (!imp->is_string()) fail("invalid @import value", "", *imp);
        const std::string url = resolve(base_url.empty() ? std::optional<std::string>{} : std::optional<std::string>{base_url}, imp->text);
        const Json doc = load(url, *imp);
        const Json* ic = doc.is_object() ? doc.get("@context") : nullptr;
        if (!ic || !ic->is_object()) fail("invalid remote context", url, *imp);
        if (ic->has("@import")) fail("invalid context entry", "an imported context may not import another", *imp);
        Json merged = *ic;
        for (const auto& [k, v] : context.members)
          if (k != "@import") merged.set(k, v);
        context = std::move(merged);
      }
      if (const Json* b = context.get("@base"); b && remote.empty()) {
        if (b->is_null()) result.base.reset();
        else if (b->is_string()) {
          if (absolute(b->text)) result.base = b->text;
          else if (result.base) result.base = resolve(result.base, b->text);
          else fail("invalid base IRI", "a relative @base needs a base to resolve against", *b);
        } else fail("invalid base IRI", "", *b);
      }
      if (const Json* v = context.get("@vocab")) {
        if (v->is_null()) result.vocab.reset();
        else if (v->is_string()) {
          const auto e = iri(result, v->text, true, true);
          if (!e || !(absolute(*e) || blank_id(*e) || e->empty())) fail("invalid vocab mapping", v->text, *v);
          result.vocab = *e;
        } else fail("invalid vocab mapping", "", *v);
      }
      if (const Json* l = context.get("@language")) {
        if (l->is_null()) result.language.reset();
        else if (l->is_string()) result.language = lower(l->text);
        else fail("invalid default language", "", *l);
      }
      if (const Json* d = context.get("@direction")) {
        if (d->is_null()) result.direction.reset();
        else if (d->is_string() && (d->text == "ltr" || d->text == "rtl")) result.direction = d->text;
        else fail("invalid base direction", "", *d);
      }
      bool protect = false;
      if (const Json* pr = context.get("@protected")) {
        if (!pr->is_bool()) fail("invalid @protected value", "", *pr);
        protect = pr->truth;
      }
      std::map<std::string, bool> defined;
      Defining d{&context, &defined, base_url, protect, override_protected, remote, validate_scoped};
      static const std::set<std::string> skip = {"@base", "@direction", "@import", "@language", "@propagate", "@protected", "@version", "@vocab"};
      for (const auto& [k, v] : context.members)
        if (!skip.count(k)) define(result, k, d);
    }
    return result;
  }

  // ---- Expansion (API §5.1.2) ---------------------------------------------------------------------------------------------------
  Json expand_document(const Json& doc) const {
    const Context active = initial();
    Json r = expand(active, std::nullopt, doc, opt_.base);
    if (r.is_object() && r.members.size() == 1 && r.has("@graph")) r = *r.get("@graph");
    if (r.is_null()) r = Json::array();
    return as_array(r);
  }

  Json expand(const Context& active_in, const std::optional<std::string>& ap, const Json& element, const std::string& base_url, bool from_map = false,
              bool inside_list = false) const {
    if (element.is_null()) return Json::null_value();
    const TermDef* ptd = active_in.find(ap);
    if (element.is_scalar()) {
      if (!ap || *ap == "@graph") return Json::null_value();
      if (ptd && ptd->has_context) return value_expansion(process(active_in, ptd->context, ptd->context_base), ap, element);
      return value_expansion(active_in, ap, element);
    }
    if (element.is_array()) {
      Json result = Json::array();
      const bool list = (ptd && ptd->container.count("@list")) || inside_list;
      for (const auto& item : element.items) {
        Json e = expand(active_in, ap, item, base_url, from_map, inside_list);
        if (list && e.is_array()) {
          Json l = Json::object();
          l.set("@list", e);
          e = std::move(l);
        }
        if (e.is_array()) result.items.insert(result.items.end(), e.items.begin(), e.items.end());
        else if (!e.is_null()) result.items.push_back(std::move(e));
      }
      return result;
    }
    // A map.
    Context active = active_in;
    if (active.previous && !from_map) {
      bool has_value = false, only_id = element.members.size() == 1;
      for (const auto& [k, v] : element.members) {
        const auto e = iri(active, k, false, true);
        if (e && *e == "@value") has_value = true;
        if (!e || *e != "@id") only_id = false;
      }
      if (!has_value && !only_id) active = *active.previous;
    }
    if (ptd && ptd->has_context) active = process(active, ptd->context, ptd->context_base, {}, true);
    if (const Json* c = element.get("@context")) active = process(active, *c, base_url);
    const Context type_scoped = active;
    const std::vector<std::string> keys = sorted_keys(element);
    std::string input_type;
    bool seen_type = false;
    for (const auto& k : keys) {
      const auto e = iri(active, k, false, true);
      if (!e || *e != "@type") continue;
      const Json& v = *element.get(k);
      std::vector<std::string> terms;
      for (const auto& t : as_array(v).items)
        if (t.is_string()) terms.push_back(t.text);
      std::sort(terms.begin(), terms.end());
      for (const auto& t : terms)
        if (const TermDef* td = type_scoped.find(t); td && td->has_context) active = process(active, td->context, td->context_base, {}, false, false);
      if (!seen_type && !terms.empty()) {
        const Json arr = as_array(v);
        if (!arr.items.empty() && arr.items.back().is_string()) {
          const auto last = iri(active, arr.items.back().text, false, true);
          if (last) input_type = *last;
        }
      }
      seen_type = true;
    }
    Json result = Json::object();
    members(active, type_scoped, ap, element, base_url, input_type, result);

    // Value objects, sets and lists, and what is dropped (steps 15–19).
    if (result.has("@value")) {
      for (const auto& [k, v] : result.members)
        if (k != "@direction" && k != "@index" && k != "@language" && k != "@type" && k != "@value") fail("invalid value object", "a value object cannot have " + k, element);
      if (result.has("@type") && (result.has("@language") || result.has("@direction"))) fail("invalid value object", "a value object has a type or a language, not both", element);
      const Json& t = result.get("@type") ? *result.get("@type") : Json{};
      if (t.is_string() && t.text == "@json") {
      } else if (result.get("@value")->is_null()) return Json::null_value();
      else if (!result.get("@value")->is_string() && result.has("@language")) fail("invalid language-tagged value", "only a string can have a language", element);
      else if (result.has("@type") && !(t.is_string() && absolute(t.text))) fail("invalid typed value", "a value's type must be an IRI", element);
    } else if (Json* t = result.get("@type"); t && !t->is_array()) {
      *t = Json::array({*t});
    } else if (result.has("@set") || result.has("@list")) {
      for (const auto& [k, v] : result.members)
        if (k != "@set" && k != "@list" && k != "@index") fail("invalid set or list object", "a set or list object holds only @index beside it", element);
      if (result.has("@set")) return *result.get("@set");
    }
    if (result.members.size() == 1 && result.has("@language")) return Json::null_value();
    if (!ap || *ap == "@graph") {
      if (result.members.empty() || result.has("@value") || result.has("@list")) return Json::null_value();
      if (result.members.size() == 1 && result.has("@id")) return Json::null_value();
    }
    return result;
  }

  // ---- IRI expansion (API §5.2.2) -----------------------------------------------------------------------------------------------
  std::optional<std::string> iri(const Context& active, const std::string& value, bool relative, bool vocab) const {
    return iri_core(const_cast<Context&>(active), value, relative, vocab, nullptr);  // without a definition under way, nothing is changed
  }

 private:
  const Options& opt_;

  struct Defining {
    const Json* local;
    std::map<std::string, bool>* defined;
    std::string base_url;
    bool protect, override_protected;
    std::vector<std::string> remote;
    bool validate_scoped;
  };

  Json load(const std::string& url, const Json& at) const {
    const auto it = opt_.documents.find(url);
    if (it == opt_.documents.end()) throw Error("loading remote context failed", url + " was not given", at.line, at.column, url);
    try {
      return parse(it->second);
    } catch (const SyntaxError& e) {
      throw Error("loading remote context failed", url + ": " + e.what(), at.line, at.column, url);
    }
  }

  std::optional<std::string> iri_core(Context& active, const std::string& value, bool relative, bool vocab, Defining* d) const {
    if (keyword(value)) return value;
    if (keyword_form(value)) return std::nullopt;
    if (d && d->local->has(value)) {
      const auto f = d->defined->find(value);
      if (f == d->defined->end() || !f->second) define(active, value, *d);
    }
    if (const auto f = active.terms.find(value); f != active.terms.end() && f->second.iri && keyword(*f->second.iri)) return f->second.iri;
    if (vocab)
      if (const auto f = active.terms.find(value); f != active.terms.end()) return f->second.iri;
    const auto colon = value.find(':', 1);
    if (colon != std::string::npos) {
      const std::string prefix = value.substr(0, colon), suffix = value.substr(colon + 1);
      if (prefix == "_" || suffix.rfind("//", 0) == 0) return value;
      if (d && d->local->has(prefix)) {
        const auto f = d->defined->find(prefix);
        if (f == d->defined->end() || !f->second) define(active, prefix, *d);
      }
      if (const auto f = active.terms.find(prefix); f != active.terms.end() && f->second.iri && f->second.prefix) return *f->second.iri + suffix;
      if (absolute(value)) return value;
    }
    if (vocab && active.vocab) return *active.vocab + value;
    if (relative) return resolve(active.base, value);
    return value;
  }

  // ---- Term definitions (API §4.2.2) --------------------------------------------------------------------------------------------
  void define(Context& active, const std::string& term, Defining& d) const {
    const Json& at = *d.local->get(term);
    if (const auto it = d.defined->find(term); it != d.defined->end()) {
      if (it->second) return;
      fail("cyclic IRI mapping", term, at);
    }
    if (term.empty()) fail("invalid term definition", "a term cannot be empty", at);
    (*d.defined)[term] = false;
    if (term == "@type") {
      if (!at.is_object()) fail("keyword redefinition", term, at);
      for (const auto& [k, v] : at.members)
        if (!((k == "@container" && v.is_string() && v.text == "@set") || k == "@protected")) fail("keyword redefinition", "@type may only be given @container @set and @protected", at);
    } else if (keyword(term)) fail("keyword redefinition", term, at);
    else if (keyword_form(term)) {
      (*d.defined)[term] = true;
      return;
    }
    std::optional<TermDef> previous;
    if (const auto f = active.terms.find(term); f != active.terms.end()) previous = f->second, active.terms.erase(f);
    Json value;
    bool simple = false;
    if (at.is_null()) value = Json::object(), value.set("@id", Json::null_value());
    else if (at.is_string()) value = Json::object(), value.set("@id", at), simple = true;
    else if (at.is_object()) value = at;
    else fail("invalid term definition", term, at);
    TermDef def;
    def.simple = simple;
    def.protect = d.protect;
    if (const Json* p = value.get("@protected")) {
      if (!p->is_bool()) fail("invalid @protected value", term, *p);
      def.protect = p->truth;
    }
    if (const Json* t = value.get("@type")) {
      if (!t->is_string()) fail("invalid type mapping", term, *t);
      const auto e = iri_core(active, t->text, false, true, &d);
      if (!e || !(*e == "@id" || *e == "@vocab" || *e == "@json" || *e == "@none" || absolute(*e))) fail("invalid type mapping", t->text, *t);
      def.type = *e;
    }
    if (const Json* r = value.get("@reverse")) {
      if (value.has("@id") || value.has("@nest")) fail("invalid reverse property", term, at);
      if (!r->is_string()) fail("invalid IRI mapping", term, *r);
      if (keyword_form(r->text)) {
        (*d.defined)[term] = true;
        return;
      }
      const auto e = iri_core(active, r->text, false, true, &d);
      if (!e || !(absolute(*e) || blank_id(*e))) fail("invalid IRI mapping", r->text, *r);
      def.iri = *e;
      if (const Json* c = value.get("@container")) {
        if (!(c->is_null() || (c->is_string() && (c->text == "@set" || c->text == "@index")))) fail("invalid reverse property", "a reverse property's container is @set or @index", *c);
        if (c->is_string()) def.container.insert(c->text);
      }
      def.reverse = true;
      active.terms[term] = def;
      (*d.defined)[term] = true;
      return;
    }
    if (const Json* id = value.get("@id"); id && !(id->is_string() && id->text == term)) {
      if (id->is_null()) def.iri.reset();
      else {
        if (!id->is_string()) fail("invalid IRI mapping", term, *id);
        if (!keyword(id->text) && keyword_form(id->text)) {
          (*d.defined)[term] = true;
          return;
        }
        const auto e = iri_core(active, id->text, false, true, &d);
        if (!e || !(keyword(*e) || absolute(*e) || blank_id(*e))) fail("invalid IRI mapping", id->text, *id);
        if (*e == "@context") fail("invalid keyword alias", term, *id);
        def.iri = *e;
        bool inner_colon = false;
        for (std::size_t i = 1; i + 1 < term.size(); i++)
          if (term[i] == ':') inner_colon = true;
        if (inner_colon || term.find('/') != std::string::npos) {
          (*d.defined)[term] = true;
          const auto again = iri_core(active, term, false, true, &d);
          if (!again || *again != *e) fail("invalid IRI mapping", "the term " + term + " would expand to another IRI than its @id", *id);
        } else if (simple && (gen_delim_end(*e) || blank_id(*e))) def.prefix = true;
      }
    } else if (const auto colon = term.find(':', 1); colon != std::string::npos) {
      const std::string prefix = term.substr(0, colon), suffix = term.substr(colon + 1);
      if (d.local->has(prefix)) define(active, prefix, d);
      if (const auto f = active.terms.find(prefix); f != active.terms.end() && f->second.iri) def.iri = *f->second.iri + suffix;
      else def.iri = term;
    } else if (term.find('/') != std::string::npos) {
      const auto e = iri_core(active, term, false, true, &d);
      if (!e || !absolute(*e)) fail("invalid IRI mapping", term, at);
      def.iri = *e;
    } else if (term == "@type") def.iri = "@type";
    else if (active.vocab) def.iri = *active.vocab + term;
    else fail("invalid IRI mapping", "the term " + term + " has no IRI and there is no @vocab", at);
    if (const Json* c = value.get("@container")) {
      std::set<std::string> s;
      if (c->is_string()) s.insert(c->text);
      else if (c->is_array()) {
        for (const auto& x : c->items) {
          if (!x.is_string()) fail("invalid container mapping", term, *c);
          s.insert(x.text);
        }
      } else fail("invalid container mapping", term, *c);
      static const std::set<std::string> allowed = {"@graph", "@id", "@index", "@language", "@list", "@set", "@type"};
      bool ok = true;
      for (const auto& x : s) ok = ok && allowed.count(x);
      if (s.count("@list")) ok = ok && s.size() == 1;
      else if (s.count("@graph")) {
        for (const auto& x : s) ok = ok && (x == "@graph" || x == "@id" || x == "@index" || x == "@set");
        ok = ok && !(s.count("@id") && s.count("@index"));
      } else ok = ok && s.size() - s.count("@set") <= 1;
      if (!ok || s.empty()) fail("invalid container mapping", term, *c);
      def.container = s;
      if (s.count("@type")) {
        if (!def.type) def.type = "@id";
        else if (*def.type != "@id" && *def.type != "@vocab") fail("invalid type mapping", "a type map's terms are @id or @vocab", *c);
      }
    }
    if (const Json* ix = value.get("@index")) {
      if (!def.container.count("@index")) fail("invalid term definition", "@index needs an @index container", *ix);
      if (!ix->is_string()) fail("invalid term definition", term, *ix);
      const auto e = iri_core(active, ix->text, false, true, &d);
      if (!e || keyword(*e)) fail("invalid term definition", "@index must name a property", *ix);
      def.index = ix->text;
    }
    if (const Json* lc = value.get("@context")) {
      try {
        process(active, *lc, d.base_url, d.remote, true, true, false);
      } catch (const Error& e) {
        if (e.code == "loading remote context failed") throw;
        fail("invalid scoped context", e.what(), *lc);
      }
      def.has_context = true;
      def.context = *lc;
      def.context_base = d.base_url;
    }
    if (const Json* l = value.get("@language"); l && !value.has("@type")) {
      if (!(l->is_null() || l->is_string())) fail("invalid language mapping", term, *l);
      def.has_language = true;
      if (l->is_string()) def.language = lower(l->text);
    }
    if (const Json* dir = value.get("@direction"); dir && !value.has("@type")) {
      if (!(dir->is_null() || (dir->is_string() && (dir->text == "ltr" || dir->text == "rtl")))) fail("invalid base direction", term, *dir);
      def.has_direction = true;
      if (dir->is_string()) def.direction = dir->text;
    }
    if (const Json* n = value.get("@nest")) {
      if (!n->is_string() || (keyword(n->text) && n->text != "@nest")) fail("invalid @nest value", term, *n);
      def.nest = n->text;
    }
    if (const Json* p = value.get("@prefix")) {
      if (term.find(':') != std::string::npos || term.find('/') != std::string::npos) fail("invalid term definition", "a compact IRI or an IRI cannot be a prefix", *p);
      if (!p->is_bool()) fail("invalid @prefix value", term, *p);
      def.prefix = p->truth;
      if (def.prefix && def.iri && keyword(*def.iri)) fail("invalid term definition", "a keyword cannot be a prefix", *p);
    }
    static const std::set<std::string> entries = {"@container", "@context", "@direction", "@id", "@index", "@language", "@nest", "@prefix", "@protected", "@reverse", "@type"};
    for (const auto& [k, v] : value.members)
      if (!entries.count(k)) fail("invalid term definition", "the entry " + k + " in the term " + term, v);
    if (!d.override_protected && previous && previous->protect) {
      if (!previous->same(def)) fail("protected term redefinition", term, at);
      def = *previous;
    }
    active.terms[term] = def;
    (*d.defined)[term] = true;
  }

  // ---- Value expansion (API §5.3.2) ---------------------------------------------------------------------------------------------
  Json value_expansion(const Context& active, const std::optional<std::string>& ap, const Json& value) const {
    const TermDef* td = active.find(ap);
    if (td && td->type && value.is_string()) {
      if (*td->type == "@id" || *td->type == "@vocab") {
        const auto e = iri(active, value.text, true, *td->type == "@vocab");
        Json r = Json::object();
        r.set("@id", Json::string(e ? *e : value.text));
        return r;
      }
    }
    Json r = Json::object();
    r.set("@value", value);
    if (td && td->type && *td->type != "@id" && *td->type != "@vocab" && *td->type != "@none") r.set("@type", Json::string(*td->type));
    else if (value.is_string()) {
      const std::optional<std::string> lang = td && td->has_language ? td->language : active.language;
      const std::optional<std::string> dir = td && td->has_direction ? td->direction : active.direction;
      if (lang) r.set("@language", Json::string(*lang));
      if (dir) r.set("@direction", Json::string(*dir));
    }
    return r;
  }

  // The entries of a map expanded into result (API §5.1.2, steps 13 and 14).
  void members(const Context& active, const Context& type_scoped, const std::optional<std::string>& ap, const Json& element, const std::string& base_url,
               const std::string& input_type, Json& result) const {
    std::vector<std::string> nests;
    for (const auto& k : sorted_keys(element)) {
      const Json& value = *element.get(k);
      if (k == "@context") continue;
      const auto ep = iri(active, k, false, true);
      if (!ep || (ep->find(':') == std::string::npos && !keyword(*ep))) continue;
      if (keyword(*ep)) {
        const std::string& kw = *ep;
        if (ap && *ap == "@reverse") fail("invalid reverse property map", "a reverse map holds properties, not " + kw, value);
        if (result.has(kw) && kw != "@included" && kw != "@type") fail("colliding keywords", kw, value);
        Json ev;
        if (kw == "@id") {
          if (!value.is_string()) fail("invalid @id value", "", value);
          const auto e = iri(active, value.text, true, false);
          if (!e) continue;
          ev = Json::string(*e);
        } else if (kw == "@type") {
          if (value.is_string()) {
            const auto e = iri(type_scoped, value.text, true, true);
            if (!e) continue;
            ev = Json::string(*e);
          } else if (value.is_array()) {
            ev = Json::array();
            for (const auto& t : value.items) {
              if (!t.is_string()) fail("invalid type value", "", t);
              if (const auto e = iri(type_scoped, t.text, true, true)) ev.items.push_back(Json::string(*e));
            }
          } else fail("invalid type value", "", value);
          if (const Json* old = result.get("@type")) {
            Json joined = as_array(*old);
            for (const auto& x : as_array(ev).items) joined.items.push_back(x);
            ev = std::move(joined);
          }
        } else if (kw == "@graph") {
          ev = as_array(expand(active, std::string("@graph"), value, base_url));
        } else if (kw == "@included") {
          ev = as_array(expand(active, std::nullopt, value, base_url));
          for (const auto& x : ev.items)
            if (!node_object(x)) fail("invalid @included value", "@included holds node objects", value);
          if (const Json* old = result.get("@included")) {
            Json joined = *old;
            joined.items.insert(joined.items.end(), ev.items.begin(), ev.items.end());
            ev = std::move(joined);
          }
        } else if (kw == "@value") {
          if (input_type == "@json") ev = value;
          else {
            if (!(value.is_scalar() || value.is_null())) fail("invalid value object value", "", value);
            ev = value;
            if (value.is_null()) {
              result.set("@value", Json::null_value());
              continue;
            }
          }
        } else if (kw == "@language") {
          if (value.is_null()) continue;  // a value object's language may be null, which says it has none (as pyld reads it)
          if (!value.is_string()) fail("invalid language-tagged string", "", value);
          ev = value;
        } else if (kw == "@direction") {
          if (!(value.is_string() && (value.text == "ltr" || value.text == "rtl"))) fail("invalid base direction", "", value);
          ev = value;
        } else if (kw == "@index") {
          if (!value.is_string()) fail("invalid @index value", "", value);
          ev = value;
        } else if (kw == "@list") {
          if (!ap || *ap == "@graph") continue;
          ev = as_array(expand(active, ap, value, base_url, false, true));
        } else if (kw == "@set") {
          ev = expand(active, ap, value, base_url);
        } else if (kw == "@reverse") {
          if (!value.is_object()) fail("invalid @reverse value", "", value);
          const Json rev = expand(active, std::string("@reverse"), value, base_url);
          if (const Json* back = rev.get("@reverse"))
            for (const auto& [p, items] : back->members) add(result, p, items);
          bool others = false;
          for (const auto& [p, items] : rev.members) others = others || p != "@reverse";
          if (others) {
            if (!result.has("@reverse")) result.set("@reverse", Json::object());
            for (const auto& [p, items] : rev.members) {
              if (p == "@reverse") continue;
              for (const auto& item : as_array(items).items) {
                if (value_object(item) || list_object(item)) fail("invalid reverse property value", p, value);
                add(*result.get("@reverse"), p, item);
              }
            }
          }
          continue;
        } else if (kw == "@nest") {
          nests.push_back(k);
          continue;
        } else continue;
        if (!(ev.is_null() && kw == "@value" && input_type != "@json")) result.set(kw, ev);
        continue;
      }
      const TermDef* ktd = active.find(k);
      const std::set<std::string> none;
      const std::set<std::string>& container = ktd ? ktd->container : none;
      Json ev;
      if (ktd && ktd->type && *ktd->type == "@json") {
        ev = Json::object();
        ev.set("@value", value);
        ev.set("@type", Json::string("@json"));
      } else if (container.count("@language") && value.is_object()) {
        ev = Json::array();
        const std::optional<std::string> dir = ktd && ktd->has_direction ? ktd->direction : active.direction;
        for (const auto& language : sorted_keys(value)) {
          const auto expanded_language = iri(active, language, false, true);
          for (const auto& item : as_array(*value.get(language)).items) {
            if (item.is_null()) continue;
            if (!item.is_string()) fail("invalid language map value", language, item);
            Json v = Json::object();
            v.set("@value", item);
            if (language != "@none" && !(expanded_language && *expanded_language == "@none")) v.set("@language", Json::string(lower(language)));
            if (dir) v.set("@direction", Json::string(*dir));
            ev.items.push_back(std::move(v));
          }
        }
      } else if ((container.count("@index") || container.count("@type") || container.count("@id")) && value.is_object()) {
        ev = Json::array();
        const std::string index_key = ktd && ktd->index ? *ktd->index : "@index";
        for (const auto& index : sorted_keys(value)) {
          Context map_ctx = active;
          if (container.count("@id") || container.count("@type")) map_ctx = active.previous ? *active.previous : active;
          if (container.count("@type"))
            if (const TermDef* itd = map_ctx.find(index); itd && itd->has_context) map_ctx = process(map_ctx, itd->context, itd->context_base);
          const auto expanded_index = iri(active, index, false, true);
          const bool none_index = expanded_index && *expanded_index == "@none";
          const Json items = as_array(expand(map_ctx, k, as_array(*value.get(index)), base_url, true));
          for (Json item : items.items) {
            if (container.count("@graph") && !graph_object(item)) {
              Json g = Json::object();
              g.set("@graph", as_array(item));
              item = std::move(g);
            }
            if (container.count("@index") && index_key != "@index" && !none_index) {
              const Json re = value_expansion(active, index_key, Json::string(index));
              const auto eik = iri(active, index_key, false, true);
              Json vals = Json::array({re});
              if (const Json* old = item.get(*eik))
                for (const auto& x : as_array(*old).items) vals.items.push_back(x);
              item.set(*eik, vals);
              if (value_object(item) && item.members.size() > 1) fail("invalid value object", "a value under a property-valued index", item);
            } else if (container.count("@index") && !item.has("@index") && !none_index) {
              item.set("@index", Json::string(index));
            } else if (container.count("@id") && !item.has("@id") && !none_index) {
              const auto e = iri(active, index, true, false);
              item.set("@id", Json::string(e ? *e : index));
            } else if (container.count("@type") && !none_index) {
              Json types = Json::array({Json::string(expanded_index ? *expanded_index : index)});
              if (const Json* old = item.get("@type"))
                for (const auto& x : as_array(*old).items) types.items.push_back(x);
              item.set("@type", types);
            }
            ev.items.push_back(std::move(item));
          }
        }
      } else ev = expand(active, k, value, base_url);
      if (ev.is_null()) continue;
      if (container.count("@list") && !list_object(ev)) {
        Json l = Json::object();
        l.set("@list", as_array(ev));
        ev = std::move(l);
      }
      if (container.count("@graph") && !container.count("@id") && !container.count("@index")) {
        Json arr = Json::array();
        for (const auto& item : as_array(ev).items) {
          Json g = Json::object();
          g.set("@graph", as_array(item));
          arr.items.push_back(std::move(g));
        }
        ev = std::move(arr);
      }
      if (ktd && ktd->reverse) {
        if (!result.has("@reverse")) result.set("@reverse", Json::object());
        for (const auto& item : as_array(ev).items) {
          if (value_object(item) || list_object(item)) fail("invalid reverse property value", k, value);
          add(*result.get("@reverse"), *ep, item);
        }
      } else add(result, *ep, ev);
    }
    for (const auto& nk : nests) {
      for (const auto& n : as_array(*element.get(nk)).items) {
        if (!n.is_object()) fail("invalid @nest value", "", n);
        for (const auto& [k, v] : n.members) {
          const auto e = iri(active, k, false, true);
          if (e && *e == "@value") fail("invalid @nest value", "a nested object cannot be a value", n);
        }
        members(active, type_scoped, ap, n, base_url, input_type, result);
      }
    }
  }
};

// ---- To RDF (API §8.1–8.3) --------------------------------------------------------------------------------------------------------

inline std::string canonical_double(double x) {
  char buf[64];
  std::snprintf(buf, sizeof buf, "%1.15E", x);
  std::string s(buf);
  const auto e = s.find('E');
  std::string mantissa = s.substr(0, e);
  const int exponent = std::stoi(s.substr(e + 1));
  while (mantissa.size() > 1 && mantissa.back() == '0' && mantissa[mantissa.size() - 2] != '.') mantissa.pop_back();
  return mantissa + "E" + std::to_string(exponent);
}

inline std::string integer_text(const Json& v) {
  const bool plain = !v.lexical.empty() && v.lexical.find_first_of(".eE") == std::string::npos;
  if (plain) return v.lexical == "-0" ? "0" : v.lexical;
  char buf[64];
  std::snprintf(buf, sizeof buf, "%.0f", v.number);
  const std::string s(buf);
  return s == "-0" ? "0" : s;
}

class Emitter {
 public:
  explicit Emitter(std::vector<Quad>& out) : out_(out) {}

  void collect(const Json& v) {
    if (v.is_array())
      for (const auto& x : v.items) collect(x);
    else if (v.is_object())
      for (const auto& [k, x] : v.members) {
        if ((k == "@id" || k == "@type") && x.is_string() && blank_id(x.text)) used_.insert(x.text.substr(2));
        if (k == "@type" && x.is_array())
          for (const auto& t : x.items)
            if (t.is_string() && blank_id(t.text)) used_.insert(t.text.substr(2));
        collect(x);
      }
  }

  void top(const Json& item, const Term& graph) {
    if (node_object(item)) node(item, graph);
  }

 private:
  std::vector<Quad>& out_;
  std::set<std::string> used_;
  std::size_t next_ = 0;

  Term fresh() {
    std::string l;
    do l = "b" + std::to_string(next_++);
    while (used_.count(l));
    used_.insert(l);
    return blank(l);
  }

  static std::optional<Term> resource(const std::string& id) {
    if (blank_id(id)) return blank(id.substr(2));
    if (absolute(id) && valid_iri(id)) return iri(id);
    return std::nullopt;
  }

  void put(const Term& s, const Term& p, const Term& o, const Term& g) { out_.push_back({s, p, o, g}); }

  std::optional<Term> node(const Json& n, const Term& graph) {
    std::optional<Term> subject;
    const Json* id = n.get("@id");
    if (id) subject = resource(id->text);
    else subject = fresh();
    if (const Json* types = n.get("@type"))
      for (const auto& t : as_array(*types).items)
        if (const auto o = t.is_string() ? resource(t.text) : std::nullopt; subject && o) put(*subject, iri(rdf_ns + "type"), *o, graph);
    for (const auto& [k, v] : n.members) {
      if (keyword(k)) continue;
      const std::optional<Term> p = absolute(k) && valid_iri(k) ? std::optional<Term>(iri(k)) : std::nullopt;
      for (const auto& item : as_array(v).items) {
        const auto o = object(item, graph);
        if (subject && p && o) put(*subject, *p, *o, graph);
      }
    }
    if (const Json* rev = n.get("@reverse"))
      for (const auto& [k, v] : rev->members) {
        const std::optional<Term> p = absolute(k) && valid_iri(k) ? std::optional<Term>(iri(k)) : std::nullopt;
        for (const auto& item : as_array(v).items) {
          const auto o = node(item, graph);
          if (subject && p && o) put(*o, *p, *subject, graph);
        }
      }
    if (const Json* g = n.get("@graph"))
      if (subject)
        for (const auto& item : as_array(*g).items) top(item, *subject);
    if (const Json* inc = n.get("@included"))
      for (const auto& item : as_array(*inc).items) top(item, graph);
    return subject;
  }

  std::optional<Term> object(const Json& item, const Term& graph) {
    if (value_object(item)) return literal_of(item);
    if (list_object(item)) return list(*item.get("@list"), graph);
    if (item.is_object()) return node(item, graph);
    return std::nullopt;
  }

  Term list(const Json& arr, const Term& graph) {
    const Json a = as_array(arr);
    if (a.items.empty()) return iri(rdf_ns + "nil");
    std::vector<Term> cells;
    for (std::size_t i = 0; i < a.items.size(); i++) cells.push_back(fresh());
    for (std::size_t i = 0; i < a.items.size(); i++) {
      if (const auto o = object(a.items[i], graph)) put(cells[i], iri(rdf_ns + "first"), *o, graph);
      put(cells[i], iri(rdf_ns + "rest"), i + 1 < cells.size() ? cells[i + 1] : iri(rdf_ns + "nil"), graph);
    }
    return cells[0];
  }

  static std::optional<Term> literal_of(const Json& item) {
    const Json& v = *item.get("@value");
    const Json* t = item.get("@type");
    std::string dt = t && t->is_string() ? t->text : "";
    if (dt == "@json") return literal(canonical(v), rdf_ns + "JSON");
    if (!dt.empty() && !(absolute(dt) && valid_iri(dt))) return std::nullopt;
    if (v.is_bool()) return literal(v.truth ? "true" : "false", dt.empty() ? xsd_ns + "boolean" : dt);
    if (v.is_number()) {
      if (std::fmod(v.number, 1.0) != 0 || std::fabs(v.number) >= 1e21 || dt == xsd_ns + "double") return literal(canonical_double(v.number), dt.empty() ? xsd_ns + "double" : dt);
      return literal(integer_text(v), dt.empty() ? xsd_ns + "integer" : dt);
    }
    if (!v.is_string()) return std::nullopt;
    if (dt.empty())
      if (const Json* l = item.get("@language")) {
        if (!valid_language(l->text)) return std::nullopt;
        return lang(v.text, l->text);
      }
    return literal(v.text, dt.empty() ? vocabulary::xsd_string : dt);
  }
};

// ---- From RDF (API §8.4) ------------------------------------------------------------------------------------------------------------

inline std::string id_of(const Term& t) { return t.is_blank() ? "_:" + t.value : t.value; }

inline Json object_of(const Term& o) {
  Json v = Json::object();
  if (!o.is_literal()) {
    v.set("@id", Json::string(id_of(o)));
    return v;
  }
  if (o.datatype == rdf_ns + "JSON") {
    try {
      v.set("@value", parse(o.value));
      v.set("@type", Json::string("@json"));
      return v;
    } catch (const SyntaxError&) {
    }
  }
  v.set("@value", Json::string(o.value));
  if (o.datatype == vocabulary::lang_string) v.set("@language", Json::string(o.language));
  else if (o.datatype != vocabulary::xsd_string) v.set("@type", Json::string(o.datatype));
  return v;
}

}  // namespace detail

inline Json from_rdf(const std::vector<Quad>& quads) {
  using namespace detail;
  struct Usage {
    std::string subject, property;
    std::size_t index;
  };
  struct Rec {
    Json node;
    std::vector<Usage> usages;
  };
  std::map<std::string, std::map<std::string, Rec>> graphs;  // "" the default graph
  const auto record = [&](std::map<std::string, Rec>& g, const std::string& id) -> Rec& {
    auto it = g.find(id);
    if (it == g.end()) {
      Rec r;
      r.node = Json::object();
      r.node.set("@id", Json::string(id));
      it = g.emplace(id, std::move(r)).first;
    }
    return it->second;
  };
  graphs[""];
  for (const auto& q : quads) {
    const std::string gk = q.g.empty() ? "" : id_of(q.g);
    auto& g = graphs[gk];
    if (!gk.empty()) record(graphs[""], gk);
    const std::string s = id_of(q.s);
    record(g, s);
    if (!q.o.is_literal()) record(g, id_of(q.o));
    Json& node = record(g, s).node;
    if (q.p.value == rdf_ns + "type" && !q.o.is_literal()) {
      if (!node.has("@type")) node.set("@type", Json::array());
      Json* types = node.get("@type");
      const Json t = Json::string(id_of(q.o));
      bool there = false;
      for (const auto& x : types->items) there = there || same(x, t);
      if (!there) types->items.push_back(t);
      continue;
    }
    const Json v = object_of(q.o);
    if (!node.has(q.p.value)) node.set(q.p.value, Json::array());
    Json* vals = node.get(q.p.value);
    std::size_t at = vals->items.size();
    bool there = false;
    for (std::size_t i = 0; i < vals->items.size(); i++)
      if (same(vals->items[i], v)) there = true, at = i;
    if (!there) vals->items.push_back(v);
    if (!q.o.is_literal() && !there) record(g, id_of(q.o)).usages.push_back({s, q.p.value, at});
  }
  const std::string first = rdf_ns + "first", rest = rdf_ns + "rest", nil = rdf_ns + "nil";
  for (auto& [gk, g] : graphs) {
    const auto nit = g.find(nil);
    if (nit == g.end()) continue;
    const std::vector<Usage> nil_usages = nit->second.usages;
    for (const auto& u : nil_usages) {
      std::string node_id = u.subject, property = u.property;
      std::size_t index = u.index;
      std::vector<Json> list;
      std::vector<std::string> list_nodes;
      for (;;) {
        if (property != rest || !blank_id(node_id)) break;
        const Rec& r = g.at(node_id);
        const Json* f = r.node.get(first);
        const Json* rs = r.node.get(rest);
        bool well = r.usages.size() == 1 && f && rs && f->items.size() == 1 && rs->items.size() == 1;
        for (const auto& [k, x] : r.node.members) {
          if (k == "@id" || k == first || k == rest) continue;
          if (k == "@type" && x.items.size() == 1 && x.items[0].text == rdf_ns + "List") continue;
          well = false;
        }
        if (!well) break;
        list.push_back(f->items[0]);
        list_nodes.push_back(node_id);
        const Usage next = r.usages[0];
        node_id = next.subject, property = next.property, index = next.index;
      }
      Rec& holder = g.at(node_id);
      Json* head = &holder.node.get(property)->items[index];
      if (property == first) {
        if (node_id == nil) continue;
        if (list.empty()) continue;
        // The list is nested inside another: keep its head node and make the rest of it a list.
        const std::string head_id = head->get("@id") ? head->get("@id")->text : "";
        if (!g.count(head_id)) continue;
        head = &g.at(head_id).node.get(rest)->items[0];
        list.pop_back();
        list_nodes.pop_back();
      }
      if (list.empty() && property != rest) {
      }
      head->erase("@id");
      std::reverse(list.begin(), list.end());
      head->set("@list", Json::array(list));
      for (const auto& n : list_nodes) g.erase(n);
    }
  }
  Json result = Json::array();
  auto& def = graphs[""];
  for (auto& [s, r] : def) {
    Json node = r.node;
    if (const auto gi = graphs.find(s); gi != graphs.end() && !s.empty()) {
      Json content = Json::array();
      for (auto& [t, x] : gi->second)
        if (x.node.members.size() > 1) content.items.push_back(x.node);
      node.set("@graph", content);
    }
    if (node.members.size() > 1) result.items.push_back(std::move(node));
  }
  return result;
}

inline syntax::Document read(const std::string& text, const Options& options = {}) {
  using namespace detail;
  Json doc;
  try {
    doc = parse(text);
  } catch (const SyntaxError& e) {
    throw Error("loading document failed", std::string(e.what()).substr(std::string(e.what()).find(": ") + 2), e.line, e.column);
  }
  const Processor P(options);
  const Json expanded = P.expand_document(doc);
  syntax::Document out;
  Emitter E(out.quads);
  E.collect(expanded);
  for (const auto& item : expanded.items) E.top(item, default_graph());
  out.base = options.base;
  if (doc.is_object())
    if (const Json* c = doc.get("@context")) {
      const Context ctx = P.process(P.initial(), *c, options.base);
      for (const auto& [term, d] : ctx.terms)
        if (d.prefix && d.iri && absolute(*d.iri) && !keyword(*d.iri)) out.prefixes.map[term] = *d.iri;
    }
  return out;
}

namespace detail {

// IRI compaction (API §6.2.2) against a context whose terms are prefixes: a term equal to the IRI in the vocabulary's
// positions, else the shortest compact IRI (the least of the shortest), else the IRI.
inline std::string compact_iri(const std::map<std::string, std::string>& prefixes, const std::string& var, bool vocab) {
  if (keyword(var)) return var;
  if (vocab) {
    std::string best;
    for (const auto& [term, ns] : prefixes)
      if (ns == var && (best.empty() || term.size() < best.size() || (term.size() == best.size() && term < best))) best = term;
    if (!best.empty()) return best;
  }
  std::string best;
  for (const auto& [term, ns] : prefixes) {
    if (!gen_delim_end(ns) || var.size() <= ns.size() || var.compare(0, ns.size(), ns) != 0) continue;
    const std::string c = term + ":" + var.substr(ns.size());
    if (prefixes.count(c)) continue;
    if (best.empty() || c.size() < best.size() || (c.size() == best.size() && c < best)) best = c;
  }
  return best.empty() ? var : best;
}

inline Json compact_item(const std::map<std::string, std::string>& prefixes, const Json& v);

inline Json compact_node(const std::map<std::string, std::string>& prefixes, const Json& node) {
  Json out = Json::object();
  if (const Json* id = node.get("@id")) out.set("@id", Json::string(compact_iri(prefixes, id->text, false)));
  if (const Json* types = node.get("@type")) {
    Json ts = Json::array();
    for (const auto& t : types->items) ts.items.push_back(Json::string(compact_iri(prefixes, t.text, true)));
    out.set("@type", ts.items.size() == 1 ? ts.items[0] : ts);
  }
  std::vector<std::pair<std::string, const Json*>> props;
  for (const auto& [k, v] : node.members)
    if (!keyword(k)) props.emplace_back(compact_iri(prefixes, k, true), &v);
  std::sort(props.begin(), props.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  for (const auto& [k, v] : props) {
    Json vals = Json::array();
    for (const auto& item : v->items) vals.items.push_back(compact_item(prefixes, item));
    out.set(k, vals.items.size() == 1 ? vals.items[0] : vals);
  }
  if (const Json* g = node.get("@graph")) {
    Json content = Json::array();
    for (const auto& n : g->items) content.items.push_back(compact_node(prefixes, n));
    out.set("@graph", content);
  }
  return out;
}

inline Json compact_item(const std::map<std::string, std::string>& prefixes, const Json& v) {
  if (const Json* l = v.get("@list")) {
    Json arr = Json::array();
    for (const auto& x : l->items) arr.items.push_back(compact_item(prefixes, x));
    Json o = Json::object();
    o.set("@list", arr);
    return o;
  }
  if (const Json* val = v.get("@value")) {
    if (v.members.size() == 1 && val->is_string()) return *val;
    Json o = Json::object();
    o.set("@value", *val);
    if (const Json* t = v.get("@type")) o.set("@type", Json::string(compact_iri(prefixes, t->text, true)));
    if (const Json* l = v.get("@language")) o.set("@language", *l);
    return o;
  }
  if (v.members.size() == 1 && v.has("@id")) {
    Json o = Json::object();
    o.set("@id", Json::string(compact_iri(prefixes, v.get("@id")->text, false)));
    return o;
  }
  return compact_node(prefixes, v);
}

}  // namespace detail

// A dataset written as JSON-LD, compacted against a context of the prefixes given; `indent` < 0 writes it on one line.
inline std::string write(const std::vector<Quad>& quads, const Prefixes& prefixes = {}, int indent = 2) {
  const Json expanded = from_rdf(quads);
  std::map<std::string, std::string> used;
  for (const auto& [p, ns] : prefixes.map)
    if (!p.empty()) used[p] = ns;
  Json context = Json::object();
  for (const auto& [p, ns] : used) context.set(p, Json::string(ns));
  Json doc = Json::object();
  if (!used.empty()) doc.set("@context", context);
  if (expanded.items.size() == 1) {
    const Json node = detail::compact_node(used, expanded.items[0]);
    for (const auto& m : node.members) doc.members.push_back(m);
  } else {
    Json g = Json::array();
    for (const auto& n : expanded.items) g.items.push_back(detail::compact_node(used, n));
    doc.set("@graph", g);
  }
  return dump(doc, indent) + "\n";
}

}  // namespace symploke::jsonld
