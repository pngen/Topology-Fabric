
// TopologyFabric/value.hpp - a bounded, JSON-friendly property value.
#pragma once
#include <cstdint>
#include <string>
#include <variant>
#include <vector>
#include <map>
#include <stdexcept>

namespace topology_fabric {

// A single property value. Bounded by design; strings are validated by callers.
class PropertyValue {
 public:
  using Bytes = std::vector<uint8_t>;
  using Storage = std::variant<std::nullptr_t, bool, int64_t, uint64_t, double,
                               std::string, std::vector<std::string>, std::vector<int64_t>>;

  // Bounds enforced on constructed aggregate types.
  static constexpr size_t kMaxStringBytes = 1u << 20;   // 1 MiB per string
  static constexpr size_t kMaxVectorItems = 1u << 16;   // 65536 items

  PropertyValue() : data_(nullptr) {}
  PropertyValue(std::nullptr_t) : data_(nullptr) {}
  PropertyValue(bool b) : data_(b) {}
  PropertyValue(int64_t v) : data_(v) {}
  PropertyValue(uint64_t v) : data_(v) {}
  PropertyValue(int v) : data_(static_cast<int64_t>(v)) {}
  PropertyValue(double v) : data_(v) {}
  PropertyValue(std::string s) : data_(std::move(s)) {}
  PropertyValue(const char* s) : data_(std::string(s)) {}
  PropertyValue(std::vector<std::string> v) : data_(std::move(v)) {}
  PropertyValue(std::vector<int64_t> v) : data_(std::move(v)) {}

  static PropertyValue make_string(std::string s) { return PropertyValue(std::move(s)); }
  static PropertyValue make_string_array(std::vector<std::string> v) { return PropertyValue(std::move(v)); }
  static PropertyValue make_int_array(std::vector<int64_t> v) { return PropertyValue(std::move(v)); }

  bool is_null() const noexcept { return std::holds_alternative<std::nullptr_t>(data_); }
  bool is_bool() const noexcept { return std::holds_alternative<bool>(data_); }
  bool is_int() const noexcept { return std::holds_alternative<int64_t>(data_); }
  bool is_uint() const noexcept { return std::holds_alternative<uint64_t>(data_); }
  bool is_double() const noexcept { return std::holds_alternative<double>(data_); }
  bool is_string() const noexcept { return std::holds_alternative<std::string>(data_); }
  bool is_string_array() const noexcept { return std::holds_alternative<std::vector<std::string>>(data_); }
  bool is_int_array() const noexcept { return std::holds_alternative<std::vector<int64_t>>(data_); }

  bool as_bool() const { return std::get<bool>(data_); }
  int64_t as_int() const { return std::get<int64_t>(data_); }
  uint64_t as_uint() const { return std::get<uint64_t>(data_); }
  double as_double() const { return std::get<double>(data_); }
  const std::string& as_string() const { return std::get<std::string>(data_); }
  const std::vector<std::string>& as_string_array() const { return std::get<std::vector<std::string>>(data_); }
  const std::vector<int64_t>& as_int_array() const { return std::get<std::vector<int64_t>>(data_); }

  const Storage& storage() const noexcept { return data_; }

  bool operator==(const PropertyValue& o) const { return data_ == o.data_; }
  bool operator!=(const PropertyValue& o) const { return !(*this == o); }

 private:
  Storage data_;
};

// Ordered string->value property map (deterministic iteration for serialization).
using PropertyMap = std::map<std::string, PropertyValue>;

}  // namespace topology_fabric
