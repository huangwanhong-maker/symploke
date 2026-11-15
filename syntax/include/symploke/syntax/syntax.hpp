// Documents of RDF: N-Triples, N-Quads, Turtle and TriG, read and written.
//
//   - N-Triples and N-Quads: one statement a line, terms written out in full (RDF 1.1 N-Triples and N-Quads, W3C
//     Recommendations 2014). The reader accepts exactly their grammars; the writer writes one statement a line with the
//     escapes of terms.hpp.
//   - Turtle and TriG (RDF 1.1 Turtle and TriG, W3C Recommendations 2014): prefixes and a base (@prefix, @base, and SPARQL's
//     PREFIX and BASE), relative IRIs resolved against the base by RFC 3986 §5.2, prefixed names with their escapes, 'a',
//     predicate and object lists, blank-node property lists [ … ], collections ( … ) as rdf:first and rdf:rest, short and
//     long strings with every escape, language tags and datatypes, integers, decimals, doubles and booleans; TriG adds
//     graphs, { … } for the default graph and GRAPH name { … } or name { … } for a named one.
//   - Blank nodes keep the labels a document gives them; those it leaves anonymous ([] and collections) are given labels
//     genid0, genid1, … that no label of the document takes.
//   - A document that departs from its grammar is refused, with the line, the column and what was expected. Nothing is
//     guessed and nothing is skipped.
//   - The writers group a graph's statements by subject in the order they first appear, with predicate and object lists,
//     'a' for rdf:type, IRIs compacted by the prefixes given, and numbers and booleans in their short forms where the lexical
//     form allows; what they write reads back as the same statements.

#pragma once

#include <symploke/terms/terms.hpp>

#include <cctype>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace symploke::syntax {

using namespace symploke::terms;

class Error : public std::runtime_error {
 public:
  std::size_t line = 0, column = 0;
  Error(const std::string& what, std::size_t l, std::size_t c)
      : std::runtime_error("line " + std::to_string(l) + ", column " + std::to_string(c) + ": " + what), line(l), column(c) {}
};

struct Document {
  std::vector<Quad> quads;  // in the order read; g empty in the default graph
  Prefixes prefixes;
  std::string base;
};

// ---- IRI references -----------------------------------------------------------------------------------------------------------

namespace detail {
inline std::string remove_dot_segments(std::string in) {
  std::string out;
  while (!in.empty()) {
    if (in.rfind("../", 0) == 0) in.erase(0, 3);
    else if (in.rfind("./", 0) == 0) in.erase(0, 2);
    else if (in.rfind("/./", 0) == 0) in.replace(0, 3, "/");
    else if (in == "/.") in = "/";
    else if (in.rfind("/../", 0) == 0 || in == "/..") {
      in = in == "/.." ? "/" : in.substr(3);
      const auto slash = out.rfind('/');
      out.erase(slash == std::string::npos ? 0 : slash);
    } else if (in == "." || in == "..") in.clear();
    else {
      std::size_t next = in.find('/', in[0] == '/' ? 1 : 0);
      if (next == std::string::npos) next = in.size();
      out += in.substr(0, next);
      in.erase(0, next);
    }
  }
  return out;
}
struct Parts {
  std::string scheme, authority, path, query, fragment;
  bool has_scheme = false, has_authority = false, has_query = false, has_fragment = false;
};
inline Parts split(const std::string& s) {
  Parts p;
  std::size_t i = 0;
  const auto colon = s.find(':');
  const auto first_special = s.find_first_of("/?#");
  if (colon != std::string::npos && (first_special == std::string::npos || colon < first_special) && colon > 0) {
    p.scheme = s.substr(0, colon), p.has_scheme = true, i = colon + 1;
  }
  if (s.compare(i, 2, "//") == 0) {
    const auto end = s.find_first_of("/?#", i + 2);
    p.authority = s.substr(i + 2, (end == std::string::npos ? s.size() : end) - i - 2), p.has_authority = true;
    i = end == std::string::npos ? s.size() : end;
  }
  const auto q = s.find_first_of("?#", i);
  p.path = s.substr(i, (q == std::string::npos ? s.size() : q) - i);
  i = q == std::string::npos ? s.size() : q;
  if (i < s.size() && s[i] == '?') {
    const auto h = s.find('#', i);
    p.query = s.substr(i + 1, (h == std::string::npos ? s.size() : h) - i - 1), p.has_query = true;
    i = h == std::string::npos ? s.size() : h;
  }
  if (i < s.size() && s[i] == '#') p.fragment = s.substr(i + 1), p.has_fragment = true;
  return p;
}
inline std::string join(const Parts& p) {
  std::string s;
  if (p.has_scheme) s += p.scheme + ":";
  if (p.has_authority) s += "//" + p.authority;
  s += p.path;
  if (p.has_query) s += "?" + p.query;
  if (p.has_fragment) s += "#" + p.fragment;
  return s;
}
}  // namespace detail

// A reference resolved against a base (RFC 3986, §5.2.2).
inline std::string resolve(const std::string& base, const std::string& reference) {
  const auto r = detail::split(reference);
  if (r.has_scheme) {
    auto t = r;
    t.path = detail::remove_dot_segments(r.path);
    return detail::join(t);
  }
  const auto b = detail::split(base);
  detail::Parts t;
  t.scheme = b.scheme, t.has_scheme = b.has_scheme;
  if (r.has_authority) {
    t.authority = r.authority, t.has_authority = true, t.path = detail::remove_dot_segments(r.path), t.query = r.query, t.has_query = r.has_query;
  } else {
    t.authority = b.authority, t.has_authority = b.has_authority;
    if (r.path.empty()) {
      t.path = b.path;
      t.query = r.has_query ? r.query : b.query, t.has_query = r.has_query || b.has_query;
    } else {
      if (r.path[0] == '/') {
        t.path = detail::remove_dot_segments(r.path);
      } else {
        std::string merged;
        if (b.has_authority && b.path.empty()) merged = "/" + r.path;
        else {
          const auto slash = b.path.rfind('/');
          merged = (slash == std::string::npos ? "" : b.path.substr(0, slash + 1)) + r.path;
        }
        t.path = detail::remove_dot_segments(merged);
      }
      t.query = r.query, t.has_query = r.has_query;
    }
  }
  t.fragment = r.fragment, t.has_fragment = r.has_fragment;
  return detail::join(t);
}

// ---- The reader ------------------------------------------------------------------------------------------------------------------------

namespace detail {

inline void utf8(std::string& out, std::uint32_t cp) {
  if (cp < 0x80) out += static_cast<char>(cp);
  else if (cp < 0x800) out += static_cast<char>(0xC0 | (cp >> 6)), out += static_cast<char>(0x80 | (cp & 0x3F));
  else if (cp < 0x10000) out += static_cast<char>(0xE0 | (cp >> 12)), out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)), out += static_cast<char>(0x80 | (cp & 0x3F));
  else out += static_cast<char>(0xF0 | (cp >> 18)), out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)), out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)), out += static_cast<char>(0x80 | (cp & 0x3F));
}

enum class Mode { ntriples, nquads, turtle, trig };

class Reader {
 public:
  Reader(const std::string& text, Mode mode, std::string base) : s_(text), mode_(mode) { doc_.base = std::move(base); }

  Document run() {
    if (mode_ == Mode::ntriples || mode_ == Mode::nquads) {
      lines();
    } else {
      skip();
      while (i_ < s_.size()) {
        statement();
        skip();
      }
    }
    // Labels for anonymous nodes that no label of the document takes.
    std::map<std::string, std::string> rename;
    std::size_t next = 0;
    for (const auto& g : generated_) {
      std::string label;
      do label = "genid" + std::to_string(next++);
      while (used_.count(label));
      rename[g] = label;
    }
    if (!rename.empty())
      for (auto& q : doc_.quads)
        for (Term* t : {&q.s, &q.o, &q.g})
          if (t->is_blank()) {
            const auto it = rename.find(t->value);
            if (it != rename.end()) t->value = it->second;
          }
    return std::move(doc_);
  }

 private:
  const std::string& s_;
  Mode mode_;
  std::size_t i_ = 0, line_ = 1, line_start_ = 0;
  Document doc_;
  std::set<std::string> used_;           // labels written in the document
  std::vector<std::string> generated_;   // placeholders of anonymous nodes, in order
  Term graph_;                           // the graph being read (TriG)

  [[noreturn]] void fail(const std::string& what) const { throw Error(what, line_, i_ - line_start_ + 1); }
  bool at_end() const { return i_ >= s_.size(); }
  char peek(std::size_t k = 0) const { return i_ + k < s_.size() ? s_[i_ + k] : '\0'; }
  void advance() {
    if (s_[i_] == '\n') line_++, line_start_ = i_ + 1;
    i_++;
  }
  void skip() {
    while (!at_end()) {
      const char c = peek();
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') advance();
      else if (c == '#') {
        while (!at_end() && peek() != '\n') advance();
      } else break;
    }
  }
  void expect(char c, const char* what) {
    skip();
    if (peek() != c) fail(std::string("expected ") + what);
    advance();
  }
  bool word(const char* w, bool case_insensitive) {
    const std::size_t n = std::char_traits<char>::length(w);
    if (i_ + n > s_.size()) return false;
    for (std::size_t k = 0; k < n; k++) {
      const char a = s_[i_ + k], b = w[k];
      if (case_insensitive ? std::tolower(static_cast<unsigned char>(a)) != std::tolower(static_cast<unsigned char>(b)) : a != b) return false;
    }
    const char after = i_ + n < s_.size() ? s_[i_ + n] : ' ';
    if (std::isalnum(static_cast<unsigned char>(after)) || after == '_' || after == ':' || after == '-') return false;
    for (std::size_t k = 0; k < n; k++) advance();
    return true;
  }

  std::uint32_t hex(std::size_t digits) {
    std::uint32_t v = 0;
    for (std::size_t k = 0; k < digits; k++) {
      const char c = peek();
      if (!std::isxdigit(static_cast<unsigned char>(c))) fail("expected a hexadecimal digit in a \\u or \\U escape");
      v = v * 16 + static_cast<std::uint32_t>(std::isdigit(static_cast<unsigned char>(c)) ? c - '0' : std::tolower(static_cast<unsigned char>(c)) - 'a' + 10);
      advance();
    }
    return v;
  }

  // <…>, resolved against the base (absolute required in N-Triples and N-Quads).
  std::string iriref() {
    if (peek() != '<') fail("expected an IRI in < >");
    advance();
    std::string out;
    while (true) {
      if (at_end()) fail("an IRI that does not close");
      const char c = peek();
      if (c == '>') {
        advance();
        break;
      }
      if (c == '\\') {
        advance();
        if (peek() == 'u') advance(), utf8(out, hex(4));
        else if (peek() == 'U') advance(), utf8(out, hex(8));
        else fail("only \\u and \\U escapes may appear in an IRI");
        continue;
      }
      if (static_cast<unsigned char>(c) <= 0x20 || c == '<' || c == '"' || c == '{' || c == '}' || c == '|' || c == '^' || c == '`') fail(std::string("the character '") + c + "' cannot appear in an IRI");
      out += c;
      advance();
    }
    if (mode_ == Mode::ntriples || mode_ == Mode::nquads) {
      if (!valid_iri(out)) fail("<" + out + "> is not an absolute IRI");
      return out;
    }
    const std::string r = doc_.base.empty() ? out : resolve(doc_.base, out);
    if (!valid_iri(r)) fail("<" + out + "> resolves to no absolute IRI (is a base set?)");
    return r;
  }

  static bool pn_chars_base(unsigned char c) { return std::isalpha(c) || c >= 0x80; }
  static bool pn_chars_u(unsigned char c) { return pn_chars_base(c) || c == '_'; }
  static bool pn_chars(unsigned char c) { return pn_chars_u(c) || c == '-' || std::isdigit(c); }

  std::string blank_label() {
    // _: already consumed.
    std::string label;
    const unsigned char first = static_cast<unsigned char>(peek());
    if (!(pn_chars_u(first) || std::isdigit(first))) fail("a blank node's label begins with a letter, a digit or _");
    while (!at_end()) {
      const unsigned char c = static_cast<unsigned char>(peek());
      if (pn_chars(c) || (c == '.' && i_ + 1 < s_.size() && (pn_chars(static_cast<unsigned char>(s_[i_ + 1])) || s_[i_ + 1] == '.'))) {
        label += static_cast<char>(c);
        advance();
      } else break;
    }
    if (!label.empty() && label.back() == '.') fail("a blank node's label cannot end with a dot");
    used_.insert(label);
    return label;
  }

  std::string fresh() {
    const std::string g = "\x01" + std::to_string(generated_.size());  // a placeholder no document can write
    generated_.push_back(g);
    return g;
  }

  std::string string_body() {
    const char q = peek();
    const bool long_form = peek(1) == q && peek(2) == q;
    if (long_form && (mode_ == Mode::ntriples || mode_ == Mode::nquads)) fail("long strings are not N-Triples");
    if (q == '\'' && (mode_ == Mode::ntriples || mode_ == Mode::nquads)) fail("single quotes are not N-Triples");
    for (int k = 0; k < (long_form ? 3 : 1); k++) advance();
    std::string out;
    while (true) {
      if (at_end()) fail("a string that does not close");
      const char c = peek();
      if (long_form) {
        if (c == q && peek(1) == q && peek(2) == q && peek(3) != q) {
          advance(), advance(), advance();
          break;
        }
      } else if (c == q) {
        advance();
        break;
      } else if (c == '\n' || c == '\r') {
        fail("a short string cannot hold a line break");
      }
      if (c == '\\') {
        advance();
        const char e = peek();
        switch (e) {
          case 't': out += '\t'; break;
          case 'b': out += '\b'; break;
          case 'n': out += '\n'; break;
          case 'r': out += '\r'; break;
          case 'f': out += '\f'; break;
          case '"': out += '"'; break;
          case '\'': out += '\''; break;
          case '\\': out += '\\'; break;
          case 'u': advance(), utf8(out, hex(4)); continue;
          case 'U': advance(), utf8(out, hex(8)); continue;
          default: fail(std::string("\\") + e + " is not an escape");
        }
        advance();
        continue;
      }
      out += c;
      advance();
    }
    return out;
  }

  Term literal_rest(std::string lexical) {
    if (peek() == '@') {
      advance();
      std::string tag;
      while (!at_end() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '-')) tag += peek(), advance();
      if (!valid_language(tag)) fail("@" + tag + " is not a language tag");
      return lang(std::move(lexical), tag);
    }
    if (peek() == '^' && peek(1) == '^') {
      advance(), advance();
      return literal(std::move(lexical), mode_ == Mode::ntriples || mode_ == Mode::nquads ? iriref() : iri_term());
    }
    return literal(std::move(lexical));
  }

  // Turtle: an IRI or a prefixed name, as a string.
  std::string iri_term() {
    if (peek() == '<') return iriref();
    std::string prefix;
    while (!at_end() && peek() != ':') {
      const unsigned char c = static_cast<unsigned char>(peek());
      if (prefix.empty() ? !pn_chars_base(c) : !(pn_chars(c) || c == '.')) fail("expected an IRI or a prefixed name");
      prefix += static_cast<char>(c);
      advance();
    }
    if (at_end()) fail("expected an IRI or a prefixed name");
    if (!prefix.empty() && prefix.back() == '.') fail("a prefix cannot end with a dot");
    advance();  // ':'
    std::string local;
    while (!at_end()) {
      const unsigned char c = static_cast<unsigned char>(peek());
      const bool first = local.empty();
      if (c == '\\') {
        const char e = peek(1);
        if (std::string("_~.-!$&'()*+,;=/?#@%").find(e) == std::string::npos) fail(std::string("\\") + e + " is not a local name's escape");
        advance(), advance();
        local += e;
        continue;
      }
      if (c == '%') {
        if (!std::isxdigit(static_cast<unsigned char>(peek(1))) || !std::isxdigit(static_cast<unsigned char>(peek(2)))) fail("% begins two hexadecimal digits");
        local += s_.substr(i_, 3);
        advance(), advance(), advance();
        continue;
      }
      if (pn_chars_u(c) || c == ':' || std::isdigit(c) || (!first && (c == '-' || c == '.'))) {
        if (c == '.') {
          // A dot ends the name unless more of the name follows it.
          std::size_t k = i_;
          while (k < s_.size() && s_[k] == '.') k++;
          if (k >= s_.size() || !(pn_chars(static_cast<unsigned char>(s_[k])) || s_[k] == ':' || s_[k] == '%' || s_[k] == '\\')) break;
        }
        local += static_cast<char>(c);
        advance();
      } else break;
    }
    const auto it = doc_.prefixes.map.find(prefix);
    if (it == doc_.prefixes.map.end()) fail("the prefix \"" + prefix + ":\" is not declared");
    return it->second + local;
  }

  Term numeric() {
    std::string x;
    if (peek() == '+' || peek() == '-') x += peek(), advance();
    while (std::isdigit(static_cast<unsigned char>(peek()))) x += peek(), advance();
    bool dot = false, exponent = false;
    if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) {
      dot = true;
      x += '.', advance();
      while (std::isdigit(static_cast<unsigned char>(peek()))) x += peek(), advance();
    }
    if (peek() == 'e' || peek() == 'E') {
      exponent = true;
      x += peek(), advance();
      if (peek() == '+' || peek() == '-') x += peek(), advance();
      if (!std::isdigit(static_cast<unsigned char>(peek()))) fail("an exponent needs digits");
      while (std::isdigit(static_cast<unsigned char>(peek()))) x += peek(), advance();
    }
    if (x.empty() || x == "+" || x == "-") fail("expected a number");
    const std::string& X = vocabulary::xsd;
    return literal(x, exponent ? X + "double" : dot ? X + "decimal" : X + "integer");
  }

  // ---- N-Triples and N-Quads ----
  Term nt_subject() {
    if (peek() == '<') return iri(iriref());
    if (peek() == '_' && peek(1) == ':') return advance(), advance(), blank(blank_label());
    fail("expected an IRI or a blank node");
  }
  Term nt_object() {
    if (peek() == '"') return literal_rest(string_body());
    return nt_subject();
  }
  void inline_space() {
    while (peek() == ' ' || peek() == '\t') advance();
  }
  void lines() {
    while (!at_end()) {
      inline_space();
      if (peek() == '#' || peek() == '\n' || peek() == '\r' || at_end()) {
        while (!at_end() && peek() != '\n') advance();
        if (!at_end()) advance();
        continue;
      }
      Quad q;
      q.s = nt_subject();
      inline_space();
      if (peek() != '<') fail("a predicate is an IRI");
      q.p = iri(iriref());
      inline_space();
      q.o = nt_object();
      inline_space();
      if (mode_ == Mode::nquads && peek() != '.') {
        q.g = nt_subject();
        inline_space();
      }
      if (peek() != '.') fail("expected . at the end of a statement");
      advance();
      inline_space();
      if (peek() == '#')
        while (!at_end() && peek() != '\n') advance();
      if (!at_end() && peek() != '\n' && peek() != '\r') fail("one statement a line");
      doc_.quads.push_back(std::move(q));
    }
  }

  // ---- Turtle and TriG ----
  void emit(const Term& s, const Term& p, const Term& o) { doc_.quads.push_back({s, p, o, graph_}); }

  bool directive() {
    if (peek() == '@') {
      advance();
      if (word("prefix", false)) {
        prefix_decl();
        expect('.', ". after @prefix");
      } else if (word("base", false)) {
        skip();
        doc_.base = iriref();
        expect('.', ". after @base");
      } else {
        fail("expected @prefix or @base");
      }
      return true;
    }
    if (word("PREFIX", true)) {
      prefix_decl();
      return true;
    }
    if (word("BASE", true)) {
      skip();
      doc_.base = iriref();
      return true;
    }
    return false;
  }
  void prefix_decl() {
    skip();
    std::string p;
    while (!at_end() && peek() != ':') {
      const unsigned char c = static_cast<unsigned char>(peek());
      if (p.empty() ? !pn_chars_base(c) : !(pn_chars(c) || c == '.')) fail("a prefix is a name followed by :");
      p += static_cast<char>(c);
      advance();
    }
    if (at_end()) fail("a prefix is a name followed by :");
    advance();
    skip();
    doc_.prefixes.map[p] = iriref();
  }

  void statement() {
    if (directive()) return;
    if (mode_ == Mode::trig) {
      if (word("GRAPH", true)) {
        skip();
        const Term name = label_or_subject();
        skip();
        wrapped(name);
        return;
      }
      if (peek() == '{') {
        wrapped(Term{});
        return;
      }
      // A label followed by { is a graph; otherwise triples of the default graph.
      if (peek() != '[' && peek() != '(') {
        const std::size_t save = i_, save_line = line_, save_start = line_start_;
        const Term name = label_or_subject();
        skip();
        if (peek() == '{') {
          wrapped(name);
          return;
        }
        i_ = save, line_ = save_line, line_start_ = save_start;
      }
    }
    triples();
    expect('.', ". at the end of a statement");
  }

  Term label_or_subject() {
    if (peek() == '_' && peek(1) == ':') return advance(), advance(), blank(blank_label());
    if (peek() == '[' ) {
      advance();
      skip();
      if (peek() != ']') fail("a graph's name is an IRI or a blank node");
      advance();
      return blank(fresh());
    }
    return iri(iri_term());
  }

  void wrapped(const Term& name) {
    if (peek() != '{') fail("expected { to open a graph");
    advance();
    const Term outer = graph_;
    graph_ = name;
    skip();
    while (peek() != '}') {
      if (at_end()) fail("a graph that does not close");
      triples();
      skip();
      if (peek() == '.') {
        advance();
        skip();
      } else if (peek() != '}') {
        fail("expected . or }");
      }
    }
    advance();
    graph_ = outer;
  }

  void triples() {
    skip();
    if (peek() == '[') {
      const Term s = property_list_node();
      skip();
      if (peek() != '.' && peek() != '}') predicate_objects(s);
      return;
    }
    const Term s = subject();
    skip();
    predicate_objects(s);
  }

  Term subject() {
    if (peek() == '_' && peek(1) == ':') return advance(), advance(), blank(blank_label());
    if (peek() == '(') return collection();
    if (peek() == '"' || peek() == '\'' || std::isdigit(static_cast<unsigned char>(peek()))) fail("a literal cannot be a subject");
    return iri(iri_term());
  }

  void predicate_objects(const Term& s) {
    while (true) {
      skip();
      Term p;
      if (peek() == 'a' && word("a", false)) p = iri(vocabulary::rdf_type);
      else if (peek() == '[' || peek() == '"' || peek() == '(' || peek() == '_') fail("a predicate is an IRI");
      else p = iri(iri_term());
      while (true) {
        skip();
        emit(s, p, object());
        skip();
        if (peek() == ',') {
          advance();
          continue;
        }
        break;
      }
      skip();
      if (peek() != ';') return;
      while (peek() == ';') {
        advance();
        skip();
      }
      if (peek() == '.' || peek() == ']' || peek() == '}') return;
    }
  }

  Term object() {
    const char c = peek();
    if (c == '<') return iri(iriref());
    if (c == '_' && peek(1) == ':') return advance(), advance(), blank(blank_label());
    if (c == '[') return property_list_node();
    if (c == '(') return collection();
    if (c == '"' || c == '\'') return literal_rest(string_body());
    if (std::isdigit(static_cast<unsigned char>(c)) || c == '+' || c == '-' || (c == '.' && std::isdigit(static_cast<unsigned char>(peek(1))))) return numeric();
    if (word("true", false)) return literal("true", vocabulary::xsd + "boolean");
    if (word("false", false)) return literal("false", vocabulary::xsd + "boolean");
    return iri(iri_term());
  }

  Term property_list_node() {
    advance();  // [
    const Term node = blank(fresh());
    skip();
    if (peek() == ']') {
      advance();
      return node;
    }
    predicate_objects(node);
    skip();
    if (peek() != ']') fail("expected ] to close a blank node's properties");
    advance();
    return node;
  }

  Term collection() {
    advance();  // (
    std::vector<Term> items;
    skip();
    while (peek() != ')') {
      if (at_end()) fail("a collection that does not close");
      items.push_back(object());
      skip();
    }
    advance();
    if (items.empty()) return iri(vocabulary::rdf_nil);
    std::vector<Term> cells;
    for (std::size_t k = 0; k < items.size(); k++) cells.push_back(blank(fresh()));
    for (std::size_t k = 0; k < items.size(); k++) {
      emit(cells[k], iri(vocabulary::rdf_first), items[k]);
      emit(cells[k], iri(vocabulary::rdf_rest), k + 1 < items.size() ? cells[k + 1] : iri(vocabulary::rdf_nil));
    }
    return cells[0];
  }
};

}  // namespace detail

inline Document read_ntriples(const std::string& text) { return detail::Reader(text, detail::Mode::ntriples, "").run(); }
inline Document read_nquads(const std::string& text) { return detail::Reader(text, detail::Mode::nquads, "").run(); }
inline Document read_turtle(const std::string& text, const std::string& base = "") { return detail::Reader(text, detail::Mode::turtle, base).run(); }
inline Document read_trig(const std::string& text, const std::string& base = "") { return detail::Reader(text, detail::Mode::trig, base).run(); }

// ---- The writers ----------------------------------------------------------------------------------------------------------------------

inline std::string write_nquads(const std::vector<Quad>& quads) {
  std::string out;
  for (const auto& q : quads) {
    out += nt(q.s) + " " + nt(q.p) + " " + nt(q.o);
    if (!q.g.empty()) out += " " + nt(q.g);
    out += " .\n";
  }
  return out;
}

inline std::string write_ntriples(const std::vector<Triple>& triples) {
  std::string out;
  for (const auto& t : triples) out += nt(t.s) + " " + nt(t.p) + " " + nt(t.o) + " .\n";
  return out;
}

namespace detail {
inline std::string turtle_term(const Term& t, const Prefixes& prefixes) {
  const std::string& X = vocabulary::xsd;
  if (t.is_iri()) {
    if (const auto c = prefixes.compact(t.value)) return *c;
    return nt(t);
  }
  if (t.is_literal()) {
    if (t.datatype == X + "integer" && terms::detail::is_integer(t.value)) return t.value;
    if (t.datatype == X + "decimal" && terms::detail::is_decimal(t.value) && t.value.find('.') != std::string::npos && std::isdigit(static_cast<unsigned char>(t.value.back()))) return t.value;
    if (t.datatype == X + "boolean" && (t.value == "true" || t.value == "false")) return t.value;
    if (t.datatype == X + "double" && t.value.find_first_of("eE") != std::string::npos && terms::detail::is_double(t.value)) return t.value;
    if (!t.datatype.empty() && t.datatype != vocabulary::xsd_string && t.datatype != vocabulary::lang_string) {
      const std::string s = "\"" + escape(t.value, false) + "\"^^";
      if (const auto c = prefixes.compact(t.datatype)) return s + *c;
      return s + "<" + escape(t.datatype, true) + ">";
    }
  }
  return nt(t);
}

inline std::string turtle_body(const std::vector<Triple>& triples, const Prefixes& prefixes, const std::string& indent) {
  std::vector<Term> subjects;
  std::map<Term, std::vector<std::pair<Term, std::vector<Term>>>> by;
  for (const auto& t : triples) {
    auto& list = by[t.s];
    if (list.empty()) subjects.push_back(t.s);
    auto it = std::find_if(list.begin(), list.end(), [&](const auto& e) { return e.first == t.p; });
    if (it == list.end()) list.push_back({t.p, {t.o}});
    else it->second.push_back(t.o);
  }
  std::string out;
  for (const auto& s : subjects) {
    out += indent + turtle_term(s, prefixes);
    const auto& list = by[s];
    for (std::size_t k = 0; k < list.size(); k++) {
      out += k == 0 ? " " : " ;\n" + indent + "    ";
      out += list[k].first.value == vocabulary::rdf_type ? "a" : turtle_term(list[k].first, prefixes);
      for (std::size_t j = 0; j < list[k].second.size(); j++) out += (j ? ", " : " ") + turtle_term(list[k].second[j], prefixes);
    }
    out += " .\n";
  }
  return out;
}

inline std::string prologue(const Prefixes& prefixes) {
  std::string out;
  for (const auto& [p, ns] : prefixes.map) out += "@prefix " + p + ": <" + escape(ns, true) + "> .\n";
  return out.empty() ? out : out + "\n";
}
}  // namespace detail

inline std::string write_turtle(const std::vector<Triple>& triples, const Prefixes& prefixes = {}) {
  return detail::prologue(prefixes) + detail::turtle_body(triples, prefixes, "");
}

inline std::string write_trig(const std::vector<Quad>& quads, const Prefixes& prefixes = {}) {
  std::vector<Term> graphs;
  std::map<Term, std::vector<Triple>> by;
  for (const auto& q : quads) {
    auto& list = by[q.g];
    if (list.empty()) graphs.push_back(q.g);
    list.push_back(q.triple());
  }
  std::string out = detail::prologue(prefixes);
  for (const auto& g : graphs) {
    if (g.empty()) out += detail::turtle_body(by[g], prefixes, "");
    else out += detail::turtle_term(g, prefixes) + " {\n" + detail::turtle_body(by[g], prefixes, "    ") + "}\n";
  }
  return out;
}

}  // namespace symploke::syntax
