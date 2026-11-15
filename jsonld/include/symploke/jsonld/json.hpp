// JSON, as JSON-LD reads and writes it (RFC 8259, The JavaScript Object Notation Data Interchange Format, 2017).
//
//   - parse(text): a value, each with the line and column where it begins, so that what is wrong with a document can be said
//     where it is; text that is not JSON is refused with its line and column. Numbers keep the text they were written with, so
//     that an integer is never rounded through a double; objects keep their members in the order written.
//   - dump(value, indent): written back, compact (indent < 0) or indented.
//   - canonical(value): the JSON Canonicalization Scheme (RFC 8785, 2020): members sorted by their names' UTF-16 code units,
//     no space, strings with the fewest escapes, numbers as ECMAScript writes them (ECMA-262 §7.1.12.1, Number::toString). It
//     is the lexical form of a literal of rdf:JSON (JSON-LD 1.1, §4.2.2 and §10.2).

#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace symploke::jsonld {

class SyntaxError : public std::runtime_error {
 public:
  std::size_t line = 0, column = 0;
  SyntaxError(const std::string& what, std::size_t l, std::size_t c)
      : std::runtime_error("line " + std::to_string(l) + ", column " + std::to_string(c) + ": " + what), line(l), column(c) {}
};

struct Json {
  enum class Type : std::uint8_t { null, boolean, number, string, array, object };
  Type type = Type::null;
  bool truth = false;
  double number = 0;
  std::string lexical;  // a number as written
  std::string text;     // a string
  std::vector<Json> items;
  std::vector<std::pair<std::string, Json>> members;
  std::size_t line = 0, column = 0;

  static Json null_value() { return {}; }
  static Json boolean(bool b) { Json j; j.type = Type::boolean; j.truth = b; return j; }
  static Json string(std::string s) { Json j; j.type = Type::string; j.text = std::move(s); return j; }
  static Json array(std::vector<Json> v = {}) { Json j; j.type = Type::array; j.items = std::move(v); return j; }
  static Json object() { Json j; j.type = Type::object; return j; }
  static Json num(double x, std::string lex = {}) { Json j; j.type = Type::number; j.number = x; j.lexical = std::move(lex); return j; }

  bool is_null() const { return type == Type::null; }
  bool is_bool() const { return type == Type::boolean; }
  bool is_number() const { return type == Type::number; }
  bool is_string() const { return type == Type::string; }
  bool is_array() const { return type == Type::array; }
  bool is_object() const { return type == Type::object; }
  bool is_scalar() const { return is_bool() || is_number() || is_string(); }

  const Json* get(std::string_view k) const {
    for (const auto& [name, v] : members)
      if (name == k) return &v;
    return nullptr;
  }
  Json* get(std::string_view k) {
    for (auto& [name, v] : members)
      if (name == k) return &v;
    return nullptr;
  }
  bool has(std::string_view k) const { return get(k) != nullptr; }
  void set(const std::string& k, Json v) {
    if (Json* old = get(k)) *old = std::move(v);
    else members.emplace_back(k, std::move(v));
  }
  void erase(std::string_view k) {
    for (auto it = members.begin(); it != members.end(); ++it)
      if (it->first == k) {
        members.erase(it);
        return;
      }
  }
  std::size_t size() const { return is_array() ? items.size() : members.size(); }
};

// Equality of values: numbers by value, objects as maps whatever the order of their members, arrays in order.
inline bool same(const Json& a, const Json& b) {
  if (a.type != b.type) return false;
  switch (a.type) {
    case Json::Type::null: return true;
    case Json::Type::boolean: return a.truth == b.truth;
    case Json::Type::number: return a.number == b.number;
    case Json::Type::string: return a.text == b.text;
    case Json::Type::array:
      if (a.items.size() != b.items.size()) return false;
      for (std::size_t i = 0; i < a.items.size(); i++)
        if (!same(a.items[i], b.items[i])) return false;
      return true;
    default:
      if (a.members.size() != b.members.size()) return false;
      for (const auto& [k, v] : a.members) {
        const Json* w = b.get(k);
        if (!w || !same(v, *w)) return false;
      }
      return true;
  }
}

namespace detail {

inline void put_utf8(std::string& out, std::uint32_t c) {
  if (c < 0x80) out += static_cast<char>(c);
  else if (c < 0x800) out += static_cast<char>(0xC0 | (c >> 6)), out += static_cast<char>(0x80 | (c & 0x3F));
  else if (c < 0x10000) out += static_cast<char>(0xE0 | (c >> 12)), out += static_cast<char>(0x80 | ((c >> 6) & 0x3F)), out += static_cast<char>(0x80 | (c & 0x3F));
  else
    out += static_cast<char>(0xF0 | (c >> 18)), out += static_cast<char>(0x80 | ((c >> 12) & 0x3F)), out += static_cast<char>(0x80 | ((c >> 6) & 0x3F)),
        out += static_cast<char>(0x80 | (c & 0x3F));
}

class Parser {
 public:
  explicit Parser(std::string_view t) : s_(t) {}

  Json run() {
    if (s_.size() >= 3 && static_cast<unsigned char>(s_[0]) == 0xEF && static_cast<unsigned char>(s_[1]) == 0xBB && static_cast<unsigned char>(s_[2]) == 0xBF) step(3);
    space();
    Json v = value(0);
    space();
    if (i_ < s_.size()) fail("the document goes on after its value");
    return v;
  }

 private:
  std::string_view s_;
  std::size_t i_ = 0, line_ = 1, col_ = 1;

  [[noreturn]] void fail(const std::string& what) const { throw SyntaxError(what, line_, col_); }
  char peek() const { return i_ < s_.size() ? s_[i_] : '\0'; }
  void step(std::size_t n = 1) {
    for (std::size_t k = 0; k < n && i_ < s_.size(); k++, i_++) {
      if (s_[i_] == '\n') line_++, col_ = 1;
      else if ((static_cast<unsigned char>(s_[i_]) & 0xC0) != 0x80) col_++;
    }
  }
  void space() {
    while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r')) step();
  }
  void expect(char c, const char* what) {
    if (peek() != c) fail(std::string("expected ") + what);
    step();
  }

  Json value(int depth) {
    if (depth > 512) fail("the document nests deeper than 512 levels");
    Json v;
    v.line = line_, v.column = col_;
    const char c = peek();
    if (c == '{') {
      v.type = Json::Type::object;
      step(), space();
      if (peek() == '}') return step(), v;
      for (;;) {
        space();
        if (peek() != '"') fail("expected the name of a member, in double quotes");
        std::string k = string_body();
        space(), expect(':', "':' after the name of a member"), space();
        v.members.emplace_back(std::move(k), value(depth + 1));
        space();
        if (peek() == ',') { step(); continue; }
        expect('}', "',' or '}' in an object");
        return v;
      }
    }
    if (c == '[') {
      v.type = Json::Type::array;
      step(), space();
      if (peek() == ']') return step(), v;
      for (;;) {
        space();
        v.items.push_back(value(depth + 1));
        space();
        if (peek() == ',') { step(); continue; }
        expect(']', "',' or ']' in an array");
        return v;
      }
    }
    if (c == '"') {
      v.type = Json::Type::string;
      v.text = string_body();
      return v;
    }
    if (s_.substr(i_, 4) == "true") return step(4), v.type = Json::Type::boolean, v.truth = true, v;
    if (s_.substr(i_, 5) == "false") return step(5), v.type = Json::Type::boolean, v;
    if (s_.substr(i_, 4) == "null") return step(4), v;
    if (c == '-' || (c >= '0' && c <= '9')) return number(v);
    fail(c ? std::string("unexpected '") + c + "'" : "the document ends where a value was expected");
  }

  Json number(Json v) {
    const std::size_t start = i_;
    if (peek() == '-') step();
    if (peek() == '0') step();
    else if (peek() >= '1' && peek() <= '9') while (peek() >= '0' && peek() <= '9') step();
    else fail("a number needs a digit");
    if (peek() == '.') {
      step();
      if (!(peek() >= '0' && peek() <= '9')) fail("a digit is needed after a decimal point");
      while (peek() >= '0' && peek() <= '9') step();
    }
    if (peek() == 'e' || peek() == 'E') {
      step();
      if (peek() == '+' || peek() == '-') step();
      if (!(peek() >= '0' && peek() <= '9')) fail("a digit is needed in an exponent");
      while (peek() >= '0' && peek() <= '9') step();
    }
    v.type = Json::Type::number;
    v.lexical = std::string(s_.substr(start, i_ - start));
    const auto r = std::from_chars(v.lexical.data(), v.lexical.data() + v.lexical.size(), v.number);
    if (r.ec == std::errc::result_out_of_range) v.number = (v.lexical[0] == '-' ? -1 : 1) * HUGE_VAL;
    return v;
  }

  unsigned hex4() {
    unsigned u = 0;
    for (int k = 0; k < 4; k++) {
      const char h = peek();
      u <<= 4;
      if (h >= '0' && h <= '9') u |= static_cast<unsigned>(h - '0');
      else if (h >= 'a' && h <= 'f') u |= static_cast<unsigned>(h - 'a' + 10);
      else if (h >= 'A' && h <= 'F') u |= static_cast<unsigned>(h - 'A' + 10);
      else fail("expected four hexadecimal digits after \\u");
      step();
    }
    return u;
  }

  std::string string_body() {
    step();  // the opening quote
    std::string out;
    for (;;) {
      if (i_ >= s_.size()) fail("the document ends inside a string");
      const char c = s_[i_];
      if (c == '"') return step(), out;
      if (static_cast<unsigned char>(c) < 0x20) fail("a control character must be escaped inside a string");
      if (c != '\\') {
        out += c, step();
        continue;
      }
      step();
      const char e = peek();
      step();
      switch (e) {
        case '"': out += '"'; break;
        case '\\': out += '\\'; break;
        case '/': out += '/'; break;
        case 'b': out += '\b'; break;
        case 'f': out += '\f'; break;
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        case 'u': {
          std::uint32_t u = hex4();
          if (u >= 0xD800 && u <= 0xDBFF) {
            if (peek() != '\\') fail("a high surrogate must be followed by a low one");
            step();
            if (peek() != 'u') fail("a high surrogate must be followed by a low one");
            step();
            const std::uint32_t lo = hex4();
            if (lo < 0xDC00 || lo > 0xDFFF) fail("a high surrogate must be followed by a low one");
            u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
          } else if (u >= 0xDC00 && u <= 0xDFFF) fail("a low surrogate stands alone");
          put_utf8(out, u);
          break;
        }
        default: fail("an unknown escape in a string");
      }
    }
  }
};

inline void quote(std::string& out, const std::string& s) {
  out += '"';
  for (const char ch : s) {
    const unsigned char c = static_cast<unsigned char>(ch);
    switch (ch) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          static const char* hex = "0123456789abcdef";
          out += "\\u00";
          out += hex[c >> 4];
          out += hex[c & 15];
        } else out += ch;
    }
  }
  out += '"';
}

// ECMAScript's Number::toString for a finite double (ECMA-262 §7.1.12.1), from the shortest digits that read back as it.
inline std::string ecmascript(double x) {
  if (x == 0) return "0";
  if (!std::isfinite(x)) throw std::invalid_argument("JSON has no infinite numbers");
  char buf[64];
  const auto r = std::to_chars(buf, buf + sizeof buf, std::fabs(x), std::chars_format::scientific);
  const std::string sci(buf, r.ptr);
  const auto e = sci.find('e');
  std::string digits;
  for (std::size_t i = 0; i < e; i++)
    if (sci[i] != '.') digits += sci[i];
  const int k = static_cast<int>(digits.size());
  const int n = std::stoi(sci.substr(e + 1)) + 1;  // the decimal point stands after n digits
  std::string out = x < 0 ? "-" : "";
  if (k <= n && n <= 21) out += digits + std::string(static_cast<std::size_t>(n - k), '0');
  else if (0 < n && n <= 21) out += digits.substr(0, static_cast<std::size_t>(n)) + "." + digits.substr(static_cast<std::size_t>(n));
  else if (-6 < n && n <= 0) out += "0." + std::string(static_cast<std::size_t>(-n), '0') + digits;
  else {
    out += digits.substr(0, 1);
    if (k > 1) out += "." + digits.substr(1);
    out += (n - 1 >= 0 ? "e+" : "e-") + std::to_string(std::abs(n - 1));
  }
  return out;
}

inline std::u16string utf16(const std::string& s) {
  std::u16string out;
  for (std::size_t i = 0; i < s.size();) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    std::uint32_t u;
    std::size_t n;
    if (c < 0x80) u = c, n = 1;
    else if ((c >> 5) == 6) u = c & 0x1F, n = 2;
    else if ((c >> 4) == 14) u = c & 0x0F, n = 3;
    else u = c & 0x07, n = 4;
    for (std::size_t k = 1; k < n && i + k < s.size(); k++) u = (u << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3F);
    i += n;
    if (u >= 0x10000) {
      u -= 0x10000;
      out += static_cast<char16_t>(0xD800 + (u >> 10));
      out += static_cast<char16_t>(0xDC00 + (u & 0x3FF));
    } else out += static_cast<char16_t>(u);
  }
  return out;
}

inline void write(std::string& out, const Json& v, int indent, int depth, bool canon) {
  const auto newline = [&](int d) {
    if (indent < 0) return;
    out += '\n';
    out += std::string(static_cast<std::size_t>(indent * d), ' ');
  };
  switch (v.type) {
    case Json::Type::null: out += "null"; return;
    case Json::Type::boolean: out += v.truth ? "true" : "false"; return;
    case Json::Type::number:
      out += (!canon && !v.lexical.empty()) ? v.lexical : ecmascript(v.number);
      return;
    case Json::Type::string: quote(out, v.text); return;
    case Json::Type::array:
      out += '[';
      for (std::size_t i = 0; i < v.items.size(); i++) {
        if (i) out += ',';
        newline(depth + 1);
        write(out, v.items[i], indent, depth + 1, canon);
      }
      if (!v.items.empty()) newline(depth);
      out += ']';
      return;
    default: {
      std::vector<std::size_t> order(v.members.size());
      for (std::size_t i = 0; i < order.size(); i++) order[i] = i;
      if (canon) {
        std::vector<std::u16string> keys;
        for (const auto& m : v.members) keys.push_back(utf16(m.first));
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return keys[a] < keys[b]; });
      }
      out += '{';
      for (std::size_t i = 0; i < order.size(); i++) {
        if (i) out += ',';
        newline(depth + 1);
        quote(out, v.members[order[i]].first);
        out += indent < 0 ? ":" : ": ";
        write(out, v.members[order[i]].second, indent, depth + 1, canon);
      }
      if (!order.empty()) newline(depth);
      out += '}';
    }
  }
}

}  // namespace detail

inline Json parse(std::string_view text) { return detail::Parser(text).run(); }

inline std::string dump(const Json& v, int indent = -1) {
  std::string out;
  detail::write(out, v, indent, 0, false);
  return out;
}

inline std::string canonical(const Json& v) {
  std::string out;
  detail::write(out, v, -1, 0, true);
  return out;
}

}  // namespace symploke::jsonld
