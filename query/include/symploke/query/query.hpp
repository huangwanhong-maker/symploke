// Queries over a store: the core of SPARQL 1.1 (W3C Recommendation 2013), parsed into its algebra and evaluated.
//
// A query matches graph patterns against a dataset and answers with solution mappings, partial functions from variables to
// terms (SPARQL 1.1 Query Language, §18). Written here:
//
//   - forms: SELECT (with * or variables or (expression AS ?v), DISTINCT), ASK and CONSTRUCT; PREFIX and BASE;
//   - patterns: basic graph patterns in Turtle's syntax with variables (';', ',', 'a', [ … ] as fresh variables), FILTER,
//     OPTIONAL, UNION, MINUS, BIND, VALUES, GRAPH (an IRI or a variable), nested groups;
//   - property paths: an IRI, ^p, p/q, p|q, p*, p+, p? and parentheses, evaluated as reachability (§18.4: * and + are sets,
//     duplicates not counted);
//   - solution modifiers: GROUP BY with COUNT (with * and DISTINCT), SUM, AVG, MIN, MAX, SAMPLE and GROUP_CONCAT, HAVING,
//     ORDER BY (ASC, DESC), LIMIT and OFFSET;
//   - expressions: || && ! = != < > <= >= + − * /, and BOUND, IF, COALESCE, sameTerm, isIRI, isBlank, isLiteral,
//     isNumeric, STR, LANG, DATATYPE, LANGMATCHES, REGEX (ECMAScript syntax, flag i), CONTAINS, STRSTARTS, STRENDS, STRLEN,
//     UCASE, LCASE, CONCAT, IRI.
//
// Semantics follow §18: a join of compatible mappings; OPTIONAL as a left join whose FILTER sees both sides; MINUS removes
// mappings compatible with, and sharing a variable with, one on the right; an expression that errs makes FILTER false and
// leaves BIND's variable unbound; comparisons of numbers by value, of strings and dates where comparable, and an error
// otherwise; ORDER BY puts unbound before blank nodes before IRIs before literals (§15.1). Evaluation is a plan of nested
// loops, the patterns of a basic graph pattern ordered by how many of their positions are already bound; it is exact, not
// fast for every shape.
//
// Not written: SERVICE, sub-SELECT, DESCRIBE, updates, named-graph datasets given by FROM, entailment regimes (entailment.hpp
// materialises instead). A query that uses them is refused, with where.

#pragma once

#include <symploke/store/store.hpp>
#include <symploke/syntax/syntax.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace symploke::query {

using namespace symploke::terms;

class Error : public std::runtime_error {
 public:
  explicit Error(const std::string& what) : std::runtime_error("symploke::query: " + what) {}
};

using Row = std::vector<std::optional<Term>>;  // one slot per variable

struct Result {
  enum class Form { select, ask, construct } form = Form::select;
  std::vector<std::string> variables;  // select: the projected variables, without '?'
  std::vector<Row> rows;               // select
  bool truth = false;                  // ask
  std::vector<Triple> triples;         // construct
};

// ---- Algebra ------------------------------------------------------------------------------------------------------------------------

struct Expr;
using ExprPtr = std::shared_ptr<Expr>;
struct Expr {
  enum class Op { constant, variable, call, aggregate } op = Op::constant;
  Term value;                  // constant
  std::size_t var = 0;         // variable
  std::string name;            // call or aggregate: the operator or function, in upper case
  std::vector<ExprPtr> args;
  bool distinct = false, star = false;  // aggregates
  std::string separator = " ";          // GROUP_CONCAT
  std::size_t slot = 0;                 // aggregates: where the group's value is kept
};

struct PathExpr;
using PathPtr = std::shared_ptr<PathExpr>;
struct PathExpr {
  enum class Op { link, inverse, sequence, alternative, star, plus, optional } op = Op::link;
  Term iri;
  std::vector<PathPtr> parts;
};

struct Node {  // a position of a triple pattern: a term or a variable
  bool is_var = false;
  std::size_t var = 0;
  Term term;
};

struct TriplePattern {
  Node s, o;
  Node p;        // when path is null
  PathPtr path;  // a property path other than a single IRI or variable
};

struct Pattern;
using PatternPtr = std::shared_ptr<Pattern>;
struct Pattern {
  enum class Op { bgp, join, left, union_, minus, filter, bind, values, graph, empty } op = Op::empty;
  std::vector<TriplePattern> triples;  // bgp
  PatternPtr a, b;                     // join, left (b optional), union, minus, graph (a)
  ExprPtr expr;                        // filter; left's condition; bind
  std::size_t var = 0;                 // bind; graph's variable
  Node graph;                          // graph
  std::vector<std::size_t> vars;       // values
  std::vector<Row> data;               // values: rows over `vars` only
};

struct Ordering {
  ExprPtr expr;
  bool descending = false;
};

struct Query {
  Result::Form form = Result::Form::select;
  std::vector<std::string> names;  // every variable, by slot
  PatternPtr where;
  bool distinct = false, star = false;
  std::vector<std::pair<ExprPtr, std::size_t>> projection;  // expression (null: the variable itself) and its slot
  std::vector<TriplePattern> construct;
  std::vector<ExprPtr> group;  // GROUP BY
  std::vector<ExprPtr> aggregates;
  ExprPtr having;
  std::vector<Ordering> order;
  std::optional<std::size_t> limit, offset;
  bool grouped = false;
};

// ---- Parser -------------------------------------------------------------------------------------------------------------------------

namespace detail {

class Parser {
 public:
  explicit Parser(const std::string& text) : s_(text) { prefixes_ = Prefixes{}; }

  Query run() {
    skip();
    while (true) {
      if (keyword("PREFIX")) {
        skip();
        std::string p;
        while (peek() != ':' && !at_end()) p += next();
        expect(':');
        skip();
        prefixes_.map[p] = iriref();
      } else if (keyword("BASE")) {
        skip();
        base_ = iriref();
      } else break;
    }
    if (keyword("SELECT")) select();
    else if (keyword("ASK")) {
      q_.form = Result::Form::ask;
      where_clause();
    } else if (keyword("CONSTRUCT")) {
      q_.form = Result::Form::construct;
      skip();
      expect('{');
      std::vector<TriplePattern> t;
      triples_block(t, '}');
      expect('}');
      q_.construct = t;
      where_clause();
    } else {
      fail("expected SELECT, ASK or CONSTRUCT");
    }
    modifiers();
    skip();
    if (!at_end()) fail("unexpected text after the query");
    return std::move(q_);
  }

 private:
  const std::string& s_;
  std::size_t i_ = 0;
  Prefixes prefixes_;
  std::string base_;
  Query q_;
  std::map<std::string, std::size_t> slots_;
  std::size_t anonymous_ = 0;

  [[noreturn]] void fail(const std::string& what) const {
    std::size_t line = 1, col = 1;
    for (std::size_t k = 0; k < i_ && k < s_.size(); k++) s_[k] == '\n' ? (line++, col = 1) : col++;
    throw Error("line " + std::to_string(line) + ", column " + std::to_string(col) + ": " + what);
  }
  bool at_end() const { return i_ >= s_.size(); }
  char peek(std::size_t k = 0) const { return i_ + k < s_.size() ? s_[i_ + k] : '\0'; }
  char next() { return s_[i_++]; }
  void skip() {
    while (!at_end()) {
      if (std::isspace(static_cast<unsigned char>(peek()))) i_++;
      else if (peek() == '#') {
        while (!at_end() && peek() != '\n') i_++;
      } else break;
    }
  }
  void expect(char c) {
    skip();
    if (peek() != c) fail(std::string("expected '") + c + "'");
    i_++;
  }
  bool accept(char c) {
    skip();
    if (peek() == c) {
      i_++;
      return true;
    }
    return false;
  }
  bool keyword(const char* w) {
    skip();
    const std::size_t n = std::char_traits<char>::length(w);
    if (i_ + n > s_.size()) return false;
    for (std::size_t k = 0; k < n; k++)
      if (std::toupper(static_cast<unsigned char>(s_[i_ + k])) != w[k]) return false;
    const char after = i_ + n < s_.size() ? s_[i_ + n] : ' ';
    if (std::isalnum(static_cast<unsigned char>(after)) || after == '_' || after == ':') return false;
    i_ += n;
    return true;
  }
  std::size_t slot(const std::string& name) {
    const auto it = slots_.find(name);
    if (it != slots_.end()) return it->second;
    const std::size_t k = q_.names.size();
    q_.names.push_back(name);
    slots_[name] = k;
    return k;
  }
  std::string iriref() {
    skip();
    if (peek() != '<') fail("expected an IRI");
    i_++;
    std::string v;
    while (!at_end() && peek() != '>') v += next();
    if (at_end()) fail("an IRI that does not close");
    i_++;
    return base_.empty() ? v : syntax::resolve(base_, v);
  }
  std::string iri() {
    skip();
    if (peek() == '<') return iriref();
    std::string p;
    while (!at_end() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_' || peek() == '-' || peek() == '.')) p += next();
    if (peek() != ':') fail("expected an IRI or a prefixed name");
    i_++;
    std::string local;
    while (!at_end() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_' || peek() == '-' || peek() == '.' || peek() == ':' ||
                         static_cast<unsigned char>(peek()) >= 0x80)) {
      if (peek() == '.' && !(std::isalnum(static_cast<unsigned char>(peek(1))) || peek(1) == '_')) break;
      local += next();
    }
    const auto it = prefixes_.map.find(p);
    if (it == prefixes_.map.end()) fail("the prefix \"" + p + ":\" is not declared");
    return it->second + local;
  }
  std::string var_name() {
    skip();
    if (peek() != '?' && peek() != '$') fail("expected a variable");
    i_++;
    std::string v;
    while (!at_end() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) v += next();
    if (v.empty()) fail("a variable needs a name");
    return v;
  }
  Term literal() {
    skip();
    const char q = peek();
    std::string v;
    const bool long_form = peek(1) == q && peek(2) == q;
    i_ += long_form ? 3 : 1;
    while (true) {
      if (at_end()) fail("a string that does not close");
      if (long_form ? (peek() == q && peek(1) == q && peek(2) == q) : peek() == q) break;
      char c = next();
      if (c == '\\') {
        const char e = next();
        c = e == 'n' ? '\n' : e == 't' ? '\t' : e == 'r' ? '\r' : e;
      }
      v += c;
    }
    i_ += long_form ? 3 : 1;
    if (peek() == '@') {
      i_++;
      std::string tag;
      while (!at_end() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '-')) tag += next();
      return lang(v, tag);
    }
    if (peek() == '^' && peek(1) == '^') {
      i_ += 2;
      return terms::literal(v, iri());
    }
    return terms::literal(v);
  }
  Term number() {
    std::string x;
    if (peek() == '+' || peek() == '-') x += next();
    while (std::isdigit(static_cast<unsigned char>(peek()))) x += next();
    bool dot = false, ex = false;
    if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) {
      dot = true;
      x += next();
      while (std::isdigit(static_cast<unsigned char>(peek()))) x += next();
    }
    if (peek() == 'e' || peek() == 'E') {
      ex = true;
      x += next();
      if (peek() == '+' || peek() == '-') x += next();
      while (std::isdigit(static_cast<unsigned char>(peek()))) x += next();
    }
    const std::string& X = vocabulary::xsd;
    return terms::literal(x, ex ? X + "double" : dot ? X + "decimal" : X + "integer");
  }

  Node node(bool allow_literal) {
    skip();
    Node n;
    if (peek() == '?' || peek() == '$') {
      n.is_var = true;
      n.var = slot(var_name());
    } else if (peek() == '_' && peek(1) == ':') {
      i_ += 2;
      std::string label;
      while (!at_end() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) label += next();
      n.is_var = true;  // blank nodes in patterns are variables not projected
      n.var = slot("_:" + label);
    } else if (allow_literal && (peek() == '"' || peek() == '\'')) {
      n.term = literal();
    } else if (allow_literal && (std::isdigit(static_cast<unsigned char>(peek())) || peek() == '+' || peek() == '-')) {
      n.term = number();
    } else if (allow_literal && keyword("TRUE")) {
      n.term = terms::literal("true", vocabulary::xsd + "boolean");
    } else if (allow_literal && keyword("FALSE")) {
      n.term = terms::literal("false", vocabulary::xsd + "boolean");
    } else {
      n.term = terms::iri(iri());
    }
    return n;
  }

  // Property paths, by precedence: alternative, sequence, inverse and modifiers, primary.
  PathPtr path_alternative() {
    auto first = path_sequence();
    std::vector<PathPtr> parts{first};
    while (accept('|')) parts.push_back(path_sequence());
    if (parts.size() == 1) return first;
    auto p = std::make_shared<PathExpr>();
    p->op = PathExpr::Op::alternative;
    p->parts = parts;
    return p;
  }
  PathPtr path_sequence() {
    auto first = path_unary();
    std::vector<PathPtr> parts{first};
    while (true) {
      skip();
      if (peek() == '/') {
        i_++;
        parts.push_back(path_unary());
      } else break;
    }
    if (parts.size() == 1) return first;
    auto p = std::make_shared<PathExpr>();
    p->op = PathExpr::Op::sequence;
    p->parts = parts;
    return p;
  }
  PathPtr path_unary() {
    skip();
    bool inverse = false;
    if (peek() == '^') i_++, inverse = true;
    skip();
    PathPtr p;
    if (peek() == '(') {
      i_++;
      p = path_alternative();
      expect(')');
    } else {
      p = std::make_shared<PathExpr>();
      if (peek() == 'a' && !std::isalnum(static_cast<unsigned char>(peek(1))) && peek(1) != ':') {
        i_++;
        p->iri = terms::iri(vocabulary::rdf_type);
      } else {
        p->iri = terms::iri(iri());
      }
    }
    if (peek() == '*' || peek() == '+' || peek() == '?') {
      auto m = std::make_shared<PathExpr>();
      m->op = peek() == '*' ? PathExpr::Op::star : peek() == '+' ? PathExpr::Op::plus : PathExpr::Op::optional;
      m->parts = {p};
      i_++;
      p = m;
    }
    if (inverse) {
      auto m = std::make_shared<PathExpr>();
      m->op = PathExpr::Op::inverse;
      m->parts = {p};
      p = m;
    }
    return p;
  }

  void triples_block(std::vector<TriplePattern>& out, char close) {
    while (true) {
      skip();
      if (peek() == close || peek() == '\0') return;
      if (peek() == '}' || keyword_ahead()) return;
      Node s;
      if (peek() == '[') s = anonymous(out);
      else s = node(false);
      predicate_objects(s, out);
      skip();
      if (peek() == '.') i_++;
      else return;
    }
  }
  bool keyword_ahead() {
    const std::size_t save = i_;
    for (const char* k : {"FILTER", "OPTIONAL", "MINUS", "BIND", "VALUES", "GRAPH"})
      if (keyword(k)) {
        i_ = save;
        return true;
      }
    return peek() == '{';
  }
  Node anonymous(std::vector<TriplePattern>& out) {
    expect('[');
    Node n;
    n.is_var = true;
    n.var = slot("_:anon" + std::to_string(anonymous_++));
    skip();
    if (peek() != ']') predicate_objects(n, out);
    expect(']');
    return n;
  }
  void predicate_objects(const Node& s, std::vector<TriplePattern>& out) {
    while (true) {
      skip();
      TriplePattern tp;
      tp.s = s;
      if (peek() == '?' || peek() == '$') {
        tp.p.is_var = true;
        tp.p.var = slot(var_name());
      } else {
        auto path = path_alternative();
        if (path->op == PathExpr::Op::link) tp.p.term = path->iri;
        else tp.path = path;
      }
      while (true) {
        skip();
        TriplePattern t = tp;
        t.o = peek() == '[' ? anonymous(out) : node(true);
        out.push_back(t);
        if (!accept(',')) break;
      }
      skip();
      if (peek() != ';') return;
      while (accept(';')) {
      }
      skip();
      if (peek() == '.' || peek() == '}' || peek() == ']') return;
    }
  }

  PatternPtr join(PatternPtr a, PatternPtr b) {
    if (!a || a->op == Pattern::Op::empty) return b;
    auto p = std::make_shared<Pattern>();
    p->op = Pattern::Op::join;
    p->a = a, p->b = b;
    return p;
  }

  PatternPtr group() {
    expect('{');
    PatternPtr acc = std::make_shared<Pattern>();
    std::vector<ExprPtr> filters;
    while (true) {
      skip();
      if (peek() == '}') {
        i_++;
        break;
      }
      if (keyword("FILTER")) {
        filters.push_back(constraint());
      } else if (keyword("OPTIONAL")) {
        auto right = group();
        auto p = std::make_shared<Pattern>();
        p->op = Pattern::Op::left;
        p->a = acc, p->b = right;
        if (right->op == Pattern::Op::filter) p->b = right->a, p->expr = right->expr;  // the right side's filter sees both sides
        acc = p;
      } else if (keyword("MINUS")) {
        auto p = std::make_shared<Pattern>();
        p->op = Pattern::Op::minus;
        p->a = acc, p->b = group();
        acc = p;
      } else if (keyword("BIND")) {
        expect('(');
        auto e = expression();
        if (!keyword("AS")) fail("expected AS in BIND");
        auto p = std::make_shared<Pattern>();
        p->op = Pattern::Op::bind;
        p->a = acc, p->expr = e, p->var = slot(var_name());
        expect(')');
        acc = p;
      } else if (keyword("VALUES")) {
        acc = join(acc, values());
      } else if (keyword("GRAPH")) {
        auto p = std::make_shared<Pattern>();
        p->op = Pattern::Op::graph;
        p->graph = node(false);
        p->a = group();
        acc = join(acc, p);
      } else if (keyword("SERVICE") || keyword("SELECT")) {
        fail("SERVICE and sub-queries are not supported");
      } else if (peek() == '{') {
        auto left = group();
        while (keyword("UNION")) {
          auto u = std::make_shared<Pattern>();
          u->op = Pattern::Op::union_;
          u->a = left, u->b = group();
          left = u;
        }
        acc = join(acc, left);
      } else {
        auto b = std::make_shared<Pattern>();
        b->op = Pattern::Op::bgp;
        triples_block(b->triples, '}');
        if (b->triples.empty()) fail("expected a triple pattern or a keyword");
        acc = join(acc, b);
      }
      skip();
      if (peek() == '.') i_++;
    }
    for (const auto& f : filters) {
      auto p = std::make_shared<Pattern>();
      p->op = Pattern::Op::filter;
      p->a = acc, p->expr = f;
      acc = p;
    }
    return acc;
  }

  PatternPtr values() {
    auto p = std::make_shared<Pattern>();
    p->op = Pattern::Op::values;
    skip();
    bool tuple = false;
    if (peek() == '(') {
      tuple = true;
      i_++;
      skip();
      while (peek() != ')') p->vars.push_back(slot(var_name())), skip();
      i_++;
    } else {
      p->vars.push_back(slot(var_name()));
    }
    expect('{');
    while (true) {
      skip();
      if (peek() == '}') {
        i_++;
        break;
      }
      Row row;
      const auto one = [&]() -> std::optional<Term> {
        if (keyword("UNDEF")) return std::nullopt;
        const Node n = node(true);
        if (n.is_var) fail("VALUES holds terms");
        return n.term;
      };
      if (tuple) {
        expect('(');
        for (std::size_t k = 0; k < p->vars.size(); k++) row.push_back(one());
        expect(')');
      } else {
        row.push_back(one());
      }
      p->data.push_back(row);
    }
    return p;
  }

  void where_clause() {
    keyword("WHERE");
    q_.where = group();
  }

  void select() {
    if (keyword("DISTINCT")) q_.distinct = true;
    else keyword("REDUCED");
    skip();
    if (accept('*')) q_.star = true;
    else {
      while (true) {
        skip();
        if (peek() == '?' || peek() == '$') q_.projection.push_back({nullptr, slot(var_name())});
        else if (peek() == '(') {
          i_++;
          auto e = expression();
          if (!keyword("AS")) fail("expected AS");
          q_.projection.push_back({e, slot(var_name())});
          expect(')');
        } else break;
      }
      if (q_.projection.empty()) fail("SELECT needs * or variables");
    }
    where_clause();
  }

  void modifiers() {
    if (keyword("GROUP")) {
      if (!keyword("BY")) fail("expected BY");
      q_.grouped = true;
      while (true) {
        skip();
        if (peek() == '?' || peek() == '$' || peek() == '(') q_.group.push_back(peek() == '(' ? (i_++, [&] { auto e = expression(); expect(')'); return e; }()) : primary());
        else break;
      }
    }
    if (keyword("HAVING")) q_.having = constraint();
    if (keyword("ORDER")) {
      if (!keyword("BY")) fail("expected BY");
      while (true) {
        skip();
        if (keyword("ASC")) {
          expect('(');
          q_.order.push_back({expression(), false});
          expect(')');
        } else if (keyword("DESC")) {
          expect('(');
          q_.order.push_back({expression(), true});
          expect(')');
        } else if (peek() == '?' || peek() == '$' || peek() == '(') {
          q_.order.push_back({peek() == '(' ? (i_++, [&] { auto e = expression(); expect(')'); return e; }()) : primary(), false});
        } else break;
      }
    }
    for (int k = 0; k < 2; k++) {
      if (keyword("LIMIT")) q_.limit = integer();
      if (keyword("OFFSET")) q_.offset = integer();
    }
  }
  std::size_t integer() {
    skip();
    std::string x;
    while (std::isdigit(static_cast<unsigned char>(peek()))) x += next();
    if (x.empty()) fail("expected a number");
    return std::stoul(x);
  }

  ExprPtr constraint() {
    skip();
    if (peek() == '(') {
      i_++;
      auto e = expression();
      expect(')');
      return e;
    }
    return primary();  // a function call
  }

  ExprPtr make_call(const std::string& name, std::vector<ExprPtr> args) {
    auto e = std::make_shared<Expr>();
    e->op = Expr::Op::call;
    e->name = name;
    e->args = std::move(args);
    return e;
  }

  ExprPtr expression() { return disjunction(); }
  ExprPtr disjunction() {
    auto a = conjunction();
    while (true) {
      skip();
      if (peek() == '|' && peek(1) == '|') i_ += 2, a = make_call("||", {a, conjunction()});
      else return a;
    }
  }
  ExprPtr conjunction() {
    auto a = relational();
    while (true) {
      skip();
      if (peek() == '&' && peek(1) == '&') i_ += 2, a = make_call("&&", {a, relational()});
      else return a;
    }
  }
  ExprPtr relational() {
    auto a = additive();
    skip();
    for (const char* op : {"<=", ">=", "!=", "=", "<", ">"}) {
      const std::size_t n = std::char_traits<char>::length(op);
      if (s_.compare(i_, n, op) == 0) {
        i_ += n;
        return make_call(op, {a, additive()});
      }
    }
    return a;
  }
  ExprPtr additive() {
    auto a = multiplicative();
    while (true) {
      skip();
      if (peek() == '+' || (peek() == '-' && !std::isdigit(static_cast<unsigned char>(peek(1))))) {
        const std::string op(1, next());
        a = make_call(op, {a, multiplicative()});
      } else if (peek() == '-') {
        i_++;
        a = make_call("-", {a, multiplicative()});
      } else return a;
    }
  }
  ExprPtr multiplicative() {
    auto a = unary();
    while (true) {
      skip();
      if (peek() == '*' || peek() == '/') {
        const std::string op(1, next());
        a = make_call(op, {a, unary()});
      } else return a;
    }
  }
  ExprPtr unary() {
    skip();
    if (peek() == '!') {
      i_++;
      return make_call("!", {unary()});
    }
    if (peek() == '-' && !std::isdigit(static_cast<unsigned char>(peek(1)))) {
      i_++;
      return make_call("NEG", {unary()});
    }
    return primary();
  }
  ExprPtr primary() {
    skip();
    auto e = std::make_shared<Expr>();
    if (peek() == '(') {
      i_++;
      auto inner = expression();
      expect(')');
      return inner;
    }
    if (peek() == '?' || peek() == '$') {
      e->op = Expr::Op::variable;
      e->var = slot(var_name());
      return e;
    }
    if (peek() == '"' || peek() == '\'') {
      e->value = literal();
      return e;
    }
    if (std::isdigit(static_cast<unsigned char>(peek())) || ((peek() == '-' || peek() == '+') && std::isdigit(static_cast<unsigned char>(peek(1))))) {
      e->value = number();
      return e;
    }
    if (keyword("TRUE")) {
      e->value = terms::literal("true", vocabulary::xsd + "boolean");
      return e;
    }
    if (keyword("FALSE")) {
      e->value = terms::literal("false", vocabulary::xsd + "boolean");
      return e;
    }
    if (peek() == '<') {
      e->value = terms::iri(iriref());
      return e;
    }
    // A function, an aggregate or a prefixed name.
    const std::size_t save = i_;
    std::string name;
    while (!at_end() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) name += next();
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    skip();
    if (peek() == '(' && !name.empty()) {
      i_++;
      static const std::set<std::string> aggregates = {"COUNT", "SUM", "AVG", "MIN", "MAX", "SAMPLE", "GROUP_CONCAT"};
      if (aggregates.count(upper)) {
        e->op = Expr::Op::aggregate;
        e->name = upper;
        if (keyword("DISTINCT")) e->distinct = true;
        skip();
        if (peek() == '*') {
          i_++;
          e->star = true;
        } else {
          e->args.push_back(expression());
        }
        skip();
        if (peek() == ';') {
          i_++;
          if (!keyword("SEPARATOR")) fail("expected SEPARATOR");
          expect('=');
          e->separator = literal().value;
        }
        expect(')');
        e->slot = q_.aggregates.size();
        q_.aggregates.push_back(e);
        return e;
      }
      std::vector<ExprPtr> args;
      skip();
      if (peek() != ')') {
        args.push_back(expression());
        while (accept(',')) args.push_back(expression());
      }
      expect(')');
      static const std::set<std::string> known = {"BOUND", "IF", "COALESCE", "SAMETERM", "ISIRI", "ISURI", "ISBLANK", "ISLITERAL", "ISNUMERIC", "STR", "LANG", "DATATYPE",
                                                  "LANGMATCHES", "REGEX", "CONTAINS", "STRSTARTS", "STRENDS", "STRLEN", "UCASE", "LCASE", "CONCAT", "IRI", "URI"};
      if (!known.count(upper)) fail("the function " + name + " is not supported");
      return make_call(upper == "URI" ? "IRI" : upper == "ISURI" ? "ISIRI" : upper, args);
    }
    i_ = save;
    e->value = terms::iri(iri());
    return e;
  }
};

}  // namespace detail

inline Query parse(const std::string& text) { return detail::Parser(text).run(); }

// ---- Evaluation -------------------------------------------------------------------------------------------------------------------------

namespace detail {

using Solutions = std::vector<Row>;

inline bool compatible(const Row& a, const Row& b) {
  for (std::size_t k = 0; k < a.size(); k++)
    if (a[k] && b[k] && *a[k] != *b[k]) return false;
  return true;
}
inline Row merge(Row a, const Row& b) {
  for (std::size_t k = 0; k < a.size(); k++)
    if (!a[k] && b[k]) a[k] = b[k];
  return a;
}

struct Fail {};  // an expression's error

inline Term boolean(bool b) { return literal(b ? "true" : "false", vocabulary::xsd + "boolean"); }

inline bool numeric(const Value& v) { return v.type == Value::Type::integer || v.type == Value::Type::decimal || v.type == Value::Type::floating; }

// The effective boolean value (§17.2.2).
inline bool ebv(const Term& t) {
  if (!t.is_literal()) throw Fail{};
  const auto v = value(t);
  switch (v.type) {
    case Value::Type::boolean: return v.truth;
    case Value::Type::string:
    case Value::Type::lang_string: return !t.value.empty();
    case Value::Type::integer:
    case Value::Type::decimal:
    case Value::Type::floating: return v.number != 0 && !std::isnan(v.number);
    default: throw Fail{};
  }
}

// Comparison for = < etc.: −1, 0, 1; Fail when the terms are not comparable.
inline int compare(const Term& a, const Term& b) {
  if (a.is_literal() && b.is_literal()) {
    const auto x = value(a), y = value(b);
    if (numeric(x) && numeric(y)) return x.number < y.number ? -1 : x.number > y.number ? 1 : 0;
    if ((x.type == Value::Type::string && y.type == Value::Type::string) || (x.type == Value::Type::lang_string && y.type == Value::Type::lang_string && a.language == b.language))
      return a.value < b.value ? -1 : a.value > b.value ? 1 : 0;
    if (x.type == Value::Type::boolean && y.type == Value::Type::boolean) return static_cast<int>(x.truth) - static_cast<int>(y.truth);
    if (x.type == y.type && (x.type == Value::Type::date || x.type == Value::Type::datetime) && x.zoned == y.zoned) return x.number < y.number ? -1 : x.number > y.number ? 1 : 0;
    if (a == b) return 0;
    throw Fail{};
  }
  if (a == b) return 0;
  throw Fail{};
}

inline Term number_term(double v, bool integral) {
  if (integral && std::abs(v) < 9e15 && v == std::floor(v)) return literal(std::to_string(static_cast<long long>(v)), vocabulary::xsd + "integer");
  char buf[64];
  std::snprintf(buf, sizeof buf, "%.17g", v);
  return literal(buf, vocabulary::xsd + (integral ? "decimal" : "double"));
}

class Evaluator {
 public:
  Evaluator(const store::Dataset& d, const Query& q) : d_(d), q_(q), width_(q.names.size()) {}

  Solutions eval(const PatternPtr& p, const Term& graph) {
    switch (p->op) {
      case Pattern::Op::empty: return {Row(width_)};
      case Pattern::Op::bgp: return bgp(p->triples, graph);
      case Pattern::Op::join: {
        const auto a = eval(p->a, graph), b = eval(p->b, graph);
        Solutions out;
        for (const auto& x : a)
          for (const auto& y : b)
            if (compatible(x, y)) out.push_back(merge(x, y));
        return out;
      }
      case Pattern::Op::left: {
        const auto a = eval(p->a, graph), b = eval(p->b, graph);
        Solutions out;
        for (const auto& x : a) {
          bool any = false;
          for (const auto& y : b)
            if (compatible(x, y)) {
              Row m = merge(x, y);
              if (p->expr && !test(p->expr, m)) continue;
              out.push_back(std::move(m));
              any = true;
            }
          if (!any) out.push_back(x);
        }
        return out;
      }
      case Pattern::Op::union_: {
        auto a = eval(p->a, graph);
        const auto b = eval(p->b, graph);
        a.insert(a.end(), b.begin(), b.end());
        return a;
      }
      case Pattern::Op::minus: {
        const auto a = eval(p->a, graph), b = eval(p->b, graph);
        Solutions out;
        for (const auto& x : a) {
          bool removed = false;
          for (const auto& y : b) {
            bool shared = false;
            for (std::size_t k = 0; k < width_; k++) shared = shared || (x[k] && y[k]);
            if (shared && compatible(x, y)) {
              removed = true;
              break;
            }
          }
          if (!removed) out.push_back(x);
        }
        return out;
      }
      case Pattern::Op::filter: {
        Solutions out;
        for (auto& x : eval(p->a, graph))
          if (test(p->expr, x)) out.push_back(std::move(x));
        return out;
      }
      case Pattern::Op::bind: {
        Solutions out = eval(p->a, graph);
        for (auto& x : out) {
          if (x[p->var]) throw Error("BIND assigns ?" + q_.names[p->var] + ", already bound");
          try {
            x[p->var] = value_of(p->expr, x);
          } catch (const Fail&) {
          }
        }
        return out;
      }
      case Pattern::Op::values: {
        Solutions out;
        for (const auto& r : p->data) {
          Row x(width_);
          for (std::size_t k = 0; k < p->vars.size(); k++) x[p->vars[k]] = r[k];
          out.push_back(std::move(x));
        }
        return out;
      }
      case Pattern::Op::graph: {
        Solutions out;
        if (!p->graph.is_var) return eval(p->a, p->graph.term);
        for (const auto& g : d_.graphs()) {
          if (g.empty()) continue;
          for (auto x : eval(p->a, g)) {
            if (x[p->graph.var] && *x[p->graph.var] != g) continue;
            x[p->graph.var] = g;
            out.push_back(std::move(x));
          }
        }
        return out;
      }
    }
    return {};
  }

  bool test(const ExprPtr& e, const Row& r) {
    try {
      return ebv(value_of(e, r));
    } catch (const Fail&) {
      return false;
    }
  }

  Term value_of(const ExprPtr& e, const Row& r) {
    switch (e->op) {
      case Expr::Op::constant: return e->value;
      case Expr::Op::variable:
        if (!r[e->var]) throw Fail{};
        return *r[e->var];
      case Expr::Op::aggregate:
        if (!group_values_ || e->slot >= group_values_->size() || !(*group_values_)[e->slot]) throw Fail{};
        return *(*group_values_)[e->slot];
      case Expr::Op::call: return call(e, r);
    }
    throw Fail{};
  }

  const std::vector<std::optional<Term>>* group_values_ = nullptr;

  Term call(const ExprPtr& e, const Row& r) {
    const std::string& f = e->name;
    const auto arg = [&](std::size_t k) { return value_of(e->args.at(k), r); };
    if (f == "||") {
      std::optional<bool> a, b;
      try { a = ebv(arg(0)); } catch (const Fail&) {}
      if (a && *a) return boolean(true);
      try { b = ebv(arg(1)); } catch (const Fail&) {}
      if (b && *b) return boolean(true);
      if (a && b) return boolean(false);
      throw Fail{};
    }
    if (f == "&&") {
      std::optional<bool> a, b;
      try { a = ebv(arg(0)); } catch (const Fail&) {}
      if (a && !*a) return boolean(false);
      try { b = ebv(arg(1)); } catch (const Fail&) {}
      if (b && !*b) return boolean(false);
      if (a && b) return boolean(true);
      throw Fail{};
    }
    if (f == "!") return boolean(!ebv(arg(0)));
    if (f == "=") return boolean(compare(arg(0), arg(1)) == 0);
    if (f == "!=") return boolean(compare(arg(0), arg(1)) != 0);
    if (f == "<") return boolean(compare(arg(0), arg(1)) < 0);
    if (f == ">") return boolean(compare(arg(0), arg(1)) > 0);
    if (f == "<=") return boolean(compare(arg(0), arg(1)) <= 0);
    if (f == ">=") return boolean(compare(arg(0), arg(1)) >= 0);
    if (f == "+" || f == "-" || f == "*" || f == "/" || f == "NEG") {
      const auto a = value(arg(0));
      if (!numeric(a)) throw Fail{};
      if (f == "NEG") return number_term(-a.number, a.type == Value::Type::integer);
      const auto b = value(arg(1));
      if (!numeric(b)) throw Fail{};
      const bool integral = a.type == Value::Type::integer && b.type == Value::Type::integer;
      if (f == "+") return number_term(a.number + b.number, integral);
      if (f == "-") return number_term(a.number - b.number, integral);
      if (f == "*") return number_term(a.number * b.number, integral);
      if (b.number == 0 && a.type != Value::Type::floating && b.type != Value::Type::floating) throw Fail{};
      return number_term(a.number / b.number, false);
    }
    if (f == "BOUND") {
      if (e->args.size() != 1 || e->args[0]->op != Expr::Op::variable) throw Error("BOUND takes a variable");
      return boolean(r[e->args[0]->var].has_value());
    }
    if (f == "IF") return ebv(arg(0)) ? arg(1) : arg(2);
    if (f == "COALESCE") {
      for (std::size_t k = 0; k < e->args.size(); k++) try {
          return arg(k);
        } catch (const Fail&) {
        }
      throw Fail{};
    }
    if (f == "SAMETERM") return boolean(arg(0) == arg(1));
    if (f == "ISIRI") return boolean(arg(0).is_iri());
    if (f == "ISBLANK") return boolean(arg(0).is_blank());
    if (f == "ISLITERAL") return boolean(arg(0).is_literal());
    if (f == "ISNUMERIC") return boolean(numeric(value(arg(0))));
    if (f == "STR") {
      const auto t = arg(0);
      if (t.is_blank()) throw Fail{};
      return literal(t.value);
    }
    if (f == "LANG") {
      const auto t = arg(0);
      if (!t.is_literal()) throw Fail{};
      return literal(t.language);
    }
    if (f == "DATATYPE") {
      const auto t = arg(0);
      if (!t.is_literal()) throw Fail{};
      return iri(t.datatype);
    }
    if (f == "IRI") {
      const auto t = arg(0);
      if (t.is_iri()) return t;
      if (!t.is_literal()) throw Fail{};
      return iri(t.value);
    }
    const auto text = [&](std::size_t k) {
      const auto t = arg(k);
      if (!t.is_literal() || (t.datatype != vocabulary::xsd_string && t.datatype != vocabulary::lang_string)) throw Fail{};
      return t;
    };
    if (f == "LANGMATCHES") {
      std::string tag = arg(0).value, range = arg(1).value;
      std::transform(tag.begin(), tag.end(), tag.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      std::transform(range.begin(), range.end(), range.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      if (range == "*") return boolean(!tag.empty());
      return boolean(tag == range || (tag.size() > range.size() && tag.compare(0, range.size(), range) == 0 && tag[range.size()] == '-'));
    }
    if (f == "REGEX") {
      const auto t = text(0);
      const std::string flags = e->args.size() > 2 ? arg(2).value : "";
      auto syntax = std::regex::ECMAScript;
      if (flags.find('i') != std::string::npos) syntax |= std::regex::icase;
      try {
        return boolean(std::regex_search(t.value, std::regex(arg(1).value, syntax)));
      } catch (const std::regex_error&) {
        throw Fail{};
      }
    }
    if (f == "CONTAINS") return boolean(text(0).value.find(text(1).value) != std::string::npos);
    if (f == "STRSTARTS") return boolean(text(0).value.rfind(text(1).value, 0) == 0);
    if (f == "STRENDS") {
      const auto a = text(0).value, b = text(1).value;
      return boolean(a.size() >= b.size() && a.compare(a.size() - b.size(), b.size(), b) == 0);
    }
    if (f == "STRLEN") {
      const auto s = text(0).value;
      std::size_t n = 0;
      for (const unsigned char c : s) n += (c & 0xC0) != 0x80;
      return literal(std::to_string(n), vocabulary::xsd + "integer");
    }
    if (f == "UCASE" || f == "LCASE") {
      auto t = text(0);
      for (auto& c : t.value) c = static_cast<char>(f == "UCASE" ? std::toupper(static_cast<unsigned char>(c)) : std::tolower(static_cast<unsigned char>(c)));
      return t;
    }
    if (f == "CONCAT") {
      std::string s;
      for (std::size_t k = 0; k < e->args.size(); k++) s += text(k).value;
      return literal(s);
    }
    throw Error("the function " + f + " is not supported");
  }

  // Nodes reachable from `from` by a path (forward), the set semantics of §18.4.
  std::set<Term> reach(const PathPtr& p, const Term& from, const Term& graph, bool forward) {
    std::set<Term> out;
    using O = PathExpr::Op;
    switch (p->op) {
      case O::link:
        for (const auto& q : d_.match({forward ? std::optional<Term>(from) : std::nullopt, p->iri, forward ? std::nullopt : std::optional<Term>(from), graph}))
          out.insert(forward ? q.o : q.s);
        return out;
      case O::inverse: return reach(p->parts[0], from, graph, !forward);
      case O::sequence: {
        std::set<Term> frontier{from};
        const std::size_t n = p->parts.size();
        for (std::size_t k = 0; k < n; k++) {
          const auto& part = p->parts[forward ? k : n - 1 - k];
          std::set<Term> next;
          for (const auto& t : frontier)
            for (const auto& u : reach(part, t, graph, forward)) next.insert(u);
          frontier = std::move(next);
        }
        return frontier;
      }
      case O::alternative:
        for (const auto& part : p->parts)
          for (const auto& u : reach(part, from, graph, forward)) out.insert(u);
        return out;
      case O::optional:
        out = reach(p->parts[0], from, graph, forward);
        out.insert(from);
        return out;
      case O::star:
      case O::plus: {
        std::set<Term> seen;
        std::vector<Term> stack;
        if (p->op == O::star) seen.insert(from), stack.push_back(from);
        else
          for (const auto& u : reach(p->parts[0], from, graph, forward))
            if (seen.insert(u).second) stack.push_back(u);
        while (!stack.empty()) {
          const Term t = stack.back();
          stack.pop_back();
          for (const auto& u : reach(p->parts[0], t, graph, forward))
            if (seen.insert(u).second) stack.push_back(u);
        }
        return seen;
      }
    }
    return out;
  }

  // Every subject and object of the graph (the nodes a path's start may range over).
  std::set<Term> nodes(const Term& graph) {
    std::set<Term> n;
    for (const auto& q : d_.match({std::nullopt, std::nullopt, std::nullopt, graph})) n.insert(q.s), n.insert(q.o);
    return n;
  }

  Solutions bgp(std::vector<TriplePattern> patterns, const Term& graph) {
    Solutions current{Row(width_)};
    std::vector<bool> done(patterns.size(), false);
    for (std::size_t step = 0; step < patterns.size(); step++) {
      // The pattern with the most positions bound by the variables bound so far.
      std::set<std::size_t> bound;
      if (!current.empty())
        for (std::size_t k = 0; k < width_; k++)
          if (current[0][k]) bound.insert(k);
      std::size_t best = patterns.size();
      int score = -1;
      for (std::size_t k = 0; k < patterns.size(); k++) {
        if (done[k]) continue;
        const auto& t = patterns[k];
        const auto fixed = [&](const Node& n) { return !n.is_var || bound.count(n.var); };
        const int sc = fixed(t.s) * 2 + (t.path ? 0 : fixed(t.p)) + fixed(t.o) * 2;
        if (sc > score) score = sc, best = k;
      }
      done[best] = true;
      const auto& t = patterns[best];
      Solutions next;
      for (const auto& row : current) {
        const auto resolve = [&](const Node& n) -> std::optional<Term> {
          if (!n.is_var) return n.term;
          return row[n.var];
        };
        const auto s = resolve(t.s), o = resolve(t.o);
        if (t.path) {
          std::vector<std::pair<Term, Term>> pairs;
          if (s) {
            for (const auto& u : reach(t.path, *s, graph, true))
              if (!o || u == *o) pairs.push_back({*s, u});
          } else if (o) {
            for (const auto& u : reach(t.path, *o, graph, false)) pairs.push_back({u, *o});
          } else {
            for (const auto& start : nodes(graph))
              for (const auto& u : reach(t.path, start, graph, true)) pairs.push_back({start, u});
          }
          for (const auto& [a, b] : pairs) {
            Row x = row;
            if (t.s.is_var) x[t.s.var] = a;
            if (t.o.is_var) {
              if (x[t.o.var] && *x[t.o.var] != b) continue;
              x[t.o.var] = b;
            }
            next.push_back(std::move(x));
          }
          continue;
        }
        const auto p = resolve(t.p);
        for (const auto& q : d_.match({s, p, o, graph})) {
          Row x = row;
          bool ok = true;
          const auto assign = [&](const Node& n, const Term& v) {
            if (!n.is_var) return;
            if (x[n.var] && *x[n.var] != v) ok = false;
            else x[n.var] = v;
          };
          assign(t.s, q.s);
          assign(t.p, q.p);
          assign(t.o, q.o);
          if (ok) next.push_back(std::move(x));
        }
      }
      current = std::move(next);
      if (current.empty()) break;
    }
    return current;
  }

 private:
  const store::Dataset& d_;
  const Query& q_;
  std::size_t width_;
};

// The order of §15.1: unbound, blank nodes, IRIs, literals; literals by value where comparable, else by their text.
inline bool order_less(const std::optional<Term>& a, const std::optional<Term>& b) {
  if (!a || !b) return !a && b;
  const auto rank = [](const Term& t) { return t.is_blank() ? 0 : t.is_iri() ? 1 : 2; };
  if (rank(*a) != rank(*b)) return rank(*a) < rank(*b);
  if (a->is_literal()) try {
      return compare(*a, *b) < 0;
    } catch (const Fail&) {
    }
  return *a < *b;
}

}  // namespace detail

inline Result run(const store::Dataset& d, const Query& q) {
  detail::Evaluator ev(d, q);
  auto solutions = ev.eval(q.where, Term{});
  Result r;
  r.form = q.form;
  if (q.form == Result::Form::ask) {
    r.truth = !solutions.empty();
    return r;
  }
  if (q.form == Result::Form::construct) {
    std::set<Triple> seen;
    std::size_t row_number = 0;
    for (const auto& row : solutions) {
      std::map<std::size_t, Term> fresh;  // blank nodes of the template, new for each solution
      for (const auto& t : q.construct) {
        const auto get = [&](const Node& n) -> std::optional<Term> {
          if (!n.is_var) return n.term;
          if (q.names[n.var].rfind("_:", 0) == 0) {
            auto it = fresh.find(n.var);
            if (it == fresh.end()) it = fresh.emplace(n.var, blank("c" + std::to_string(row_number) + "_" + std::to_string(n.var))).first;
            return it->second;
          }
          return row[n.var];
        };
        const auto s = get(t.s), p = get(t.p), o = get(t.o);
        if (!s || !p || !o || t.path) continue;
        const Triple tr{*s, *p, *o};
        if (fault(tr).empty() && seen.insert(tr).second) r.triples.push_back(tr);
      }
      row_number++;
    }
    return r;
  }
  // Grouping and aggregates.
  const bool grouped = q.grouped || !q.aggregates.empty();
  std::vector<std::pair<detail::Solutions, std::vector<std::optional<Term>>>> groups;  // members, aggregate values
  if (grouped) {
    std::map<std::vector<std::optional<Term>>, std::size_t> index;
    for (const auto& row : solutions) {
      std::vector<std::optional<Term>> key;
      for (const auto& g : q.group) {
        try {
          key.push_back(ev.value_of(g, row));
        } catch (const detail::Fail&) {
          key.push_back(std::nullopt);
        }
      }
      const auto it = index.find(key);
      if (it == index.end()) {
        index[key] = groups.size();
        groups.push_back({{row}, {}});
      } else {
        groups[it->second].first.push_back(row);
      }
    }
    if (groups.empty() && q.group.empty()) groups.push_back({{}, {}});  // one group, empty, when there is no GROUP BY
    for (auto& [members, values] : groups) {
      values.assign(q.aggregates.size(), std::nullopt);
      for (const auto& a : q.aggregates) {
        std::vector<Term> xs;
        std::set<Term> distinct;
        for (const auto& m : members) {
          if (a->star) {
            xs.push_back(Term{});
            continue;
          }
          try {
            const Term v = ev.value_of(a->args[0], m);
            if (a->distinct && !distinct.insert(v).second) continue;
            xs.push_back(v);
          } catch (const detail::Fail&) {
          }
        }
        std::optional<Term> out;
        if (a->name == "COUNT") {
          std::size_t n = a->star && a->distinct ? std::set<Row>(members.begin(), members.end()).size() : xs.size();
          out = literal(std::to_string(n), vocabulary::xsd + "integer");
        } else if (a->name == "SUM" || a->name == "AVG") {
          double s = 0;
          bool integral = true, ok = true;
          for (const auto& x : xs) {
            const auto v = value(x);
            if (!detail::numeric(v)) ok = false;
            s += v.number;
            integral = integral && v.type == Value::Type::integer;
          }
          if (ok) {
            if (a->name == "SUM") out = detail::number_term(s, integral);
            else out = xs.empty() ? literal("0", vocabulary::xsd + "integer") : detail::number_term(s / static_cast<double>(xs.size()), false);
          }
        } else if (a->name == "MIN" || a->name == "MAX") {
          for (const auto& x : xs)
            if (!out || (a->name == "MIN" ? detail::order_less(x, out) : detail::order_less(out, x))) out = x;
        } else if (a->name == "SAMPLE") {
          if (!xs.empty()) out = xs.front();
        } else if (a->name == "GROUP_CONCAT") {
          std::string s;
          for (std::size_t k = 0; k < xs.size(); k++) s += (k ? a->separator : "") + xs[k].value;
          out = literal(s);
        }
        values[a->slot] = out;
      }
    }
  } else {
    for (auto& row : solutions) groups.push_back({{row}, {}});
  }
  // Rows: one per group (grouped) or per solution; HAVING; projection expressions.
  std::vector<Row> rows;
  for (auto& [members, values] : groups) {
    ev.group_values_ = grouped ? &values : nullptr;
    Row base = members.empty() ? Row(q.names.size()) : members.front();
    if (grouped) {
      // Only grouping variables keep their values in a group's row.
      Row kept(q.names.size());
      for (const auto& g : q.group)
        if (g->op == Expr::Op::variable) kept[g->var] = base[g->var];
      base = kept;
    }
    if (q.having && !ev.test(q.having, base)) continue;
    for (const auto& [e, slot] : q.projection)
      if (e) try {
          base[slot] = ev.value_of(e, base);
        } catch (const detail::Fail&) {
        }
    rows.push_back(std::move(base));
  }
  // ORDER BY.
  if (!q.order.empty()) {
    std::vector<std::vector<std::optional<Term>>> keys;
    for (const auto& row : rows) {
      std::vector<std::optional<Term>> k;
      for (const auto& o : q.order) try {
          k.push_back(ev.value_of(o.expr, row));
        } catch (const detail::Fail&) {
          k.push_back(std::nullopt);
        }
      keys.push_back(std::move(k));
    }
    std::vector<std::size_t> idx(rows.size());
    for (std::size_t k = 0; k < idx.size(); k++) idx[k] = k;
    std::stable_sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) {
      for (std::size_t k = 0; k < q.order.size(); k++) {
        if (detail::order_less(keys[a][k], keys[b][k])) return !q.order[k].descending;
        if (detail::order_less(keys[b][k], keys[a][k])) return q.order[k].descending;
      }
      return false;
    });
    std::vector<Row> sorted;
    for (const auto k : idx) sorted.push_back(rows[k]);
    rows = std::move(sorted);
  }
  // Projection.
  std::vector<std::size_t> slots;
  if (q.star) {
    for (std::size_t k = 0; k < q.names.size(); k++)
      if (q.names[k].rfind("_:", 0) != 0) slots.push_back(k);
  } else {
    for (const auto& [e, slot] : q.projection) slots.push_back(slot);
  }
  for (const auto k : slots) r.variables.push_back(q.names[k]);
  std::set<Row> seen;
  std::size_t skipped = 0;
  for (const auto& row : rows) {
    Row out;
    for (const auto k : slots) out.push_back(row[k]);
    if (q.distinct && !seen.insert(out).second) continue;
    if (q.offset && skipped < *q.offset) {
      skipped++;
      continue;
    }
    if (q.limit && r.rows.size() >= *q.limit) break;
    r.rows.push_back(std::move(out));
  }
  return r;
}

inline Result run(const store::Dataset& d, const std::string& text) { return run(d, parse(text)); }

}  // namespace symploke::query
