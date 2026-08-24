// TopologyFabric/json.hpp - a small, bounded, dependency-free JSON value.
#pragma once
#include <cstdint>
#include <string>
#include <variant>
#include <vector>
#include <map>
#include <stdexcept>
#include <memory>
#include <cmath>
#include <algorithm>

namespace topology_fabric::json {

class Value;
using Array = std::vector<Value>;
using Object = std::map<std::string, Value>;

class Value {
 public:
  enum class Kind { Null, Bool, Int, Uint, Double, String, Array, Object };

  using Storage = std::variant<std::nullptr_t, bool, int64_t, uint64_t, double,
                               std::string, std::shared_ptr<Array>, std::shared_ptr<Object>>;

  Value() : data_(nullptr) {}
  Value(std::nullptr_t) : data_(nullptr) {}
  Value(bool b) : data_(b) {}
  Value(int64_t v) : data_(v) {}
  Value(uint64_t v) : data_(v) {}
  Value(int v) : data_(static_cast<int64_t>(v)) {}
  Value(double v) : data_(v) {}
  Value(std::string s) : data_(std::move(s)) {}
  Value(const char* s) : data_(std::string(s)) {}
  Value(Array a) : data_(std::make_shared<Array>(std::move(a))) {}
  Value(Object o) : data_(std::make_shared<Object>(std::move(o))) {}

  static Value array() { return Value(Array{}); }
  static Value object() { return Value(Object{}); }

  Kind kind() const noexcept {
    switch (data_.index()) {
      case 0: return Kind::Null;
      case 1: return Kind::Bool;
      case 2: return Kind::Int;
      case 3: return Kind::Uint;
      case 4: return Kind::Double;
      case 5: return Kind::String;
      case 6: return Kind::Array;
      default: return Kind::Object;
    }
  }
  bool is_null() const noexcept { return data_.index() == 0; }
  bool is_bool() const noexcept { return data_.index() == 1; }
  bool is_int() const noexcept { return data_.index() == 2; }
  bool is_uint() const noexcept { return data_.index() == 3; }
  bool is_double() const noexcept { return data_.index() == 4; }
  bool is_string() const noexcept { return data_.index() == 5; }
  bool is_array() const noexcept { return data_.index() == 6; }
  bool is_object() const noexcept { return data_.index() == 7; }
  bool is_number() const noexcept { return is_int() || is_uint() || is_double(); }

  bool as_bool() const { return std::get<bool>(data_); }
  int64_t as_int() const { return std::get<int64_t>(data_); }
  uint64_t as_uint() const { return std::get<uint64_t>(data_); }
  double as_double() const { return std::get<double>(data_); }
  const std::string& as_string() const { return std::get<std::string>(data_); }
  const Array& as_array() const { return *std::get<std::shared_ptr<Array>>(data_); }
  Array& as_array_mut() { return *std::get<std::shared_ptr<Array>>(data_); }
  const Object& as_object() const { return *std::get<std::shared_ptr<Object>>(data_); }
  Object& as_object_mut() { return *std::get<std::shared_ptr<Object>>(data_); }

  Value& operator[](const std::string& k) { auto& o = as_object_mut(); return o[k]; }
  const Value* find(const std::string& k) const {
    if (!is_object()) return nullptr;
    auto& o = as_object(); auto it = o.find(k); return it == o.end() ? nullptr : &it->second;
  }
  void set(const std::string& k, Value v) { as_object_mut()[k] = std::move(v); }

  // Numeric access (double) regardless of int/uint/double storage.
  double number() const {
    switch (kind()) {
      case Kind::Int: return static_cast<double>(as_int());
      case Kind::Uint: return static_cast<double>(as_uint());
      case Kind::Double: return as_double();
      default: return 0.0;
    }
  }
  int64_t integer() const {
    switch (kind()) {
      case Kind::Int: return as_int();
      case Kind::Uint: return static_cast<int64_t>(as_uint());
      case Kind::Double: return static_cast<int64_t>(as_double());
      default: return 0;
    }
  }
  const Storage& storage() const noexcept { return data_; }

  bool operator==(const Value& o) const { return data_ == o.data_; }

 private:
  Storage data_;
};

// --- Parser (bounded) -----------------------------------------------------
enum class ParseError : uint8_t {
  None, UnexpectedChar, UnexpectedEnd, TrailGarbage, DepthExceeded, SizeExceeded,
  InvalidNumber, InvalidEscape, InvalidUtf8
};
struct ParseResult {
  Value value;
  ParseError error = ParseError::None;
  size_t consumed = 0;
  std::string message;
  bool ok() const { return error == ParseError::None; }
};

struct ParseOptions {
  size_t max_depth = 128;
  size_t max_nodes = 4u << 20;       // bound total value count
  size_t max_string_bytes = 1u << 24; // 16 MiB per string
  bool allow_nan_inf = false;
};

namespace detail {
inline void skip_ws(const char* p, size_t len, size_t& i) {
  while (i < len && (p[i] == ' ' || p[i] == '\t' || p[i] == '\n' || p[i] == '\r')) ++i;
}
inline bool is_digit(char c) { return c >= '0' && c <= '9'; }

class Parser {
 public:
  Parser(const char* data, size_t len, ParseOptions opt)
      : p_(data), len_(len), opt_(opt) {}

  ParseResult parse() {
    Value v;
    auto r = parse_value(v, 0);
    if (r != ParseError::None) return {Value{}, r, pos_, msg_};
    skip_ws(p_, len_, pos_);
    if (pos_ != len_) return {Value{}, ParseError::TrailGarbage, pos_, "trailing garbage after JSON value"};
    return {std::move(v), ParseError::None, pos_, {}};
  }

 private:
  const char* p_;
  size_t len_;
  ParseOptions opt_;
  size_t pos_ = 0;
  size_t nodes_ = 0;
  std::string msg_;

  bool fail(ParseError e, const std::string& m) { err_ = e; msg_ = m; return false; }

  ParseError parse_value(Value& out, size_t depth) {
    if (depth > opt_.max_depth) return ParseError::DepthExceeded;
    if (++nodes_ > opt_.max_nodes) return ParseError::SizeExceeded;
    skip_ws(p_, len_, pos_);
    if (pos_ >= len_) return ParseError::UnexpectedEnd;
    char c = p_[pos_];
    switch (c) {
      case '{': return parse_object(out, depth);
      case '[': return parse_array(out, depth);
      case '"': { std::string s; if (!parse_string(s)) return err_; out = Value(std::move(s)); return ParseError::None; }
      case 't': if (match("true")) { out = Value(true); return ParseError::None; } return ParseError::UnexpectedChar;
      case 'f': if (match("false")) { out = Value(false); return ParseError::None; } return ParseError::UnexpectedChar;
      case 'n': if (match("null")) { out = Value(nullptr); return ParseError::None; } return ParseError::UnexpectedChar;
      default:
        if (c == '-' || is_digit(c) || c == '+' || c == 'I' || c == 'N') return parse_number(out);
        return ParseError::UnexpectedChar;
    }
  }

  bool match(const char* word) {
    size_t n = 0; while (word[n]) ++n;
    if (pos_ + n > len_) return false;
    for (size_t i = 0; i < n; ++i) if (p_[pos_ + i] != word[i]) return false;
    pos_ += n; return true;
  }

  bool parse_string(std::string& out) {
    if (p_[pos_] != '"') return false;
    ++pos_;
    std::string buf;
    while (pos_ < len_) {
      char c = p_[pos_];
      if (c == '"') { ++pos_; out = std::move(buf); return true; }
      if (c == '\\') {
        ++pos_;
        if (pos_ >= len_) return false;
        char e = p_[pos_];
        switch (e) {
          case '"': buf.push_back('"'); ++pos_; break;
          case '\\': buf.push_back('\\'); ++pos_; break;
          case '/': buf.push_back('/'); ++pos_; break;
          case 'b': buf.push_back('\b'); ++pos_; break;
          case 'f': buf.push_back('\f'); ++pos_; break;
          case 'n': buf.push_back('\n'); ++pos_; break;
          case 'r': buf.push_back('\r'); ++pos_; break;
          case 't': buf.push_back('\t'); ++pos_; break;
          case 'u': {
            if (pos_ + 5 > len_) return false;
            uint32_t cp = 0;
            for (int k = 0; k < 4; ++k) { char h = p_[pos_ + 1 + k]; uint32_t d; if (h>='0'&&h<='9') d=h-'0'; else if (h>='a'&&h<='f') d=h-'a'+10; else if (h>='A'&&h<='F') d=h-'A'+10; else return false; cp = cp*16 + d; }
            pos_ += 5;
            append_utf8(buf, cp);
            break;
          }
          default: return false;
        }
        if (buf.size() > opt_.max_string_bytes) return false;
      } else {
        if (static_cast<unsigned char>(c) >= 0x80) { return false; } // non-ASCII must be escaped in our strict writer; tolerate else
        buf.push_back(c); ++pos_;
        if (buf.size() > opt_.max_string_bytes) return false;
      }
    }
    return false;
  }

  static void append_utf8(std::string& s, uint32_t cp) {
    if (cp <= 0x7F) s.push_back(static_cast<char>(cp));
    else if (cp <= 0x7FF) { s.push_back(static_cast<char>(0xC0 | (cp >> 6))); s.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
    else if (cp <= 0xFFFF) { s.push_back(static_cast<char>(0xE0 | (cp >> 12))); s.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); s.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
    else { s.push_back(static_cast<char>(0xF0 | (cp >> 18))); s.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F))); s.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); s.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
  }

  ParseError parse_number(Value& out) {
    size_t start = pos_;
    if (p_[pos_] == '+' || p_[pos_] == 'I' || p_[pos_] == 'N') {
      bool neg = false;
      if (p_[pos_] == '-') { neg = true; ++pos_; }
      if (pos_ + 3 <= len_ && (match("NaN") || match("nan"))) { if (opt_.allow_nan_inf) { out = Value(neg ? -std::numeric_limits<double>::quiet_NaN() : std::numeric_limits<double>::quiet_NaN()); return ParseError::None; } return ParseError::InvalidNumber; }
      if (pos_ + 3 <= len_ && (match("Inf") || match("inf"))) { if (opt_.allow_nan_inf) { out = Value(neg ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity()); return ParseError::None; } return ParseError::InvalidNumber; }
      return ParseError::InvalidNumber;
    }
    if (p_[pos_] == '-') ++pos_;
    bool isDouble = false;
    while (pos_ < len_ && is_digit(p_[pos_])) ++pos_;
    if (pos_ < len_ && p_[pos_] == '.') { isDouble = true; ++pos_; while (pos_ < len_ && is_digit(p_[pos_])) ++pos_; }
    if (pos_ < len_ && (p_[pos_] == 'e' || p_[pos_] == 'E')) { isDouble = true; ++pos_; if (pos_ < len_ && (p_[pos_]=='+'||p_[pos_]=='-')) ++pos_; while (pos_ < len_ && is_digit(p_[pos_])) ++pos_; }
    if (pos_ == start) return ParseError::InvalidNumber;
    std::string tok(p_ + start, pos_ - start);
    if (isDouble) {
      char* end = nullptr; double d = std::strtod(tok.c_str(), &end);
      if (end != tok.c_str() + tok.size()) return ParseError::InvalidNumber;
      out = Value(d);
    } else {
      char* end = nullptr; long long ll = std::strtoll(tok.c_str(), &end, 10);
      if (end != tok.c_str() + tok.size()) return ParseError::InvalidNumber;
      if (tok[0] == '-') out = Value(static_cast<int64_t>(ll));
      else out = Value(static_cast<uint64_t>(ll));
    }
    return ParseError::None;
  }

  ParseError parse_array(Value& out, size_t depth) {
    ++pos_; // '['
    Array arr;
    skip_ws(p_, len_, pos_);
    if (pos_ < len_ && p_[pos_] == ']') { ++pos_; out = Value(std::move(arr)); return ParseError::None; }
    while (true) {
      skip_ws(p_, len_, pos_);
      if (pos_ >= len_) return ParseError::UnexpectedEnd;
      Value child;
      auto r = parse_value(child, depth + 1);
      if (r != ParseError::None) return r;
      arr.push_back(std::move(child));
      skip_ws(p_, len_, pos_);
      if (pos_ >= len_) return ParseError::UnexpectedEnd;
      char c = p_[pos_];
      if (c == ',') { ++pos_; continue; }
      if (c == ']') { ++pos_; out = Value(std::move(arr)); return ParseError::None; }
      return ParseError::UnexpectedChar;
    }
  }

  ParseError parse_object(Value& out, size_t depth) {
    ++pos_; // '{'
    Object obj;
    skip_ws(p_, len_, pos_);
    if (pos_ < len_ && p_[pos_] == '}') { ++pos_; out = Value(std::move(obj)); return ParseError::None; }
    while (true) {
      skip_ws(p_, len_, pos_);
      if (pos_ >= len_ || p_[pos_] != '"') return ParseError::UnexpectedChar;
      std::string key;
      if (!parse_string(key)) return err_;
      skip_ws(p_, len_, pos_);
      if (pos_ >= len_ || p_[pos_] != ':') return ParseError::UnexpectedChar;
      ++pos_;
      skip_ws(p_, len_, pos_);
      Value child;
      auto r = parse_value(child, depth + 1);
      if (r != ParseError::None) return r;
      obj[std::move(key)] = std::move(child);
      skip_ws(p_, len_, pos_);
      if (pos_ >= len_) return ParseError::UnexpectedEnd;
      char c = p_[pos_];
      if (c == ',') { ++pos_; continue; }
      if (c == '}') { ++pos_; out = Value(std::move(obj)); return ParseError::None; }
      return ParseError::UnexpectedChar;
    }
  }

  ParseError err_ = ParseError::None;
};

}  // namespace detail

inline ParseResult parse(const std::string& text, ParseOptions opt = {}) {
  detail::Parser parser(text.data(), text.size(), opt);
  return parser.parse();
}

// --- Serializer -----------------------------------------------------------
enum class WriteMode { Compact, Pretty };
inline void write_value(const Value& v, std::string& out, WriteMode mode, int depth) {
  auto indent = [&](int d) {
    if (mode == WriteMode::Compact) return;
    out.push_back('\n');
    for (int i = 0; i < d; ++i) out.append("  ");
  };
  switch (v.kind()) {
    case Value::Kind::Null: out += "null"; break;
    case Value::Kind::Bool: out += v.as_bool() ? "true" : "false"; break;
    case Value::Kind::Int: out += std::to_string(v.as_int()); break;
    case Value::Kind::Uint: out += std::to_string(v.as_uint()); break;
    case Value::Kind::Double: {
      double d = v.as_double();
      if (std::isnan(d)) { out += "null"; break; }
      if (std::isinf(d)) { if (d > 0) out += "1e9999"; else out += "-1e9999"; break; }
      std::string s = std::to_string(d);
      // strip trailing zeros
      size_t dot = s.find('.');
      if (dot != std::string::npos) { while (!s.empty() && s.back() == '0') s.pop_back(); if (!s.empty() && s.back() == '.') s.pop_back(); }
      out += s; break;
    }
    case Value::Kind::String: {
      out.push_back('"');
      for (char c : v.as_string()) {
        switch (c) {
          case '"': out += "\\\""; break;
          case '\\': out += "\\\\"; break;
          case '\n': out += "\\n"; break;
          case '\r': out += "\\r"; break;
          case '\t': out += "\\t"; break;
          case '\b': out += "\\b"; break;
          case '\f': out += "\\f"; break;
          default: if (static_cast<unsigned char>(c) < 0x20) { out += "\\u00"; out.push_back("0123456789abcdef"[(c >> 4) & 0xF]); out.push_back("0123456789abcdef"[c & 0xF]); } else out.push_back(c);
        }
      }
      out.push_back('"'); break;
    }
    case Value::Kind::Array: {
      out.push_back('[');
      const auto& a = v.as_array();
      for (size_t i = 0; i < a.size(); ++i) {
        if (i) out.push_back(',');
        indent(depth + 1);
        write_value(a[i], out, mode, depth + 1);
      }
      if (!a.empty()) indent(depth);
      out.push_back(']'); break;
    }
    case Value::Kind::Object: {
      out.push_back('{');
      const auto& o = v.as_object();
      bool first = true;
      for (const auto& [k, val] : o) {
        if (!first) out.push_back(',');
        first = false;
        indent(depth + 1);
        write_value(Value(k), out, mode, depth + 1);
        out += ":";
        if (mode == WriteMode::Pretty) out.push_back(' ');
        write_value(val, out, mode, depth + 1);
      }
      if (!first) indent(depth);
      out.push_back('}'); break;
    }
  }
}

inline std::string dump(const Value& v, WriteMode mode = WriteMode::Compact) {
  std::string out;
  write_value(v, out, mode, 0);
  return out;
}

}  // namespace topology_fabric::json