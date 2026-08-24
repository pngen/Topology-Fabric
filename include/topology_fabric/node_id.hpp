
// TopologyFabric/node_id.hpp - opaque, stable, 128-bit topology node identity.
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <functional>

namespace topology_fabric {

// A globally-stable opaque 128-bit identifier for runtime topology graph objects.
// Identifiers are derived deterministically (namespace + category + native
// identity), never from transient enumeration order.
struct TopologyNodeId {
  uint64_t high = 0;
  uint64_t low = 0;

  constexpr TopologyNodeId() noexcept = default;
  constexpr TopologyNodeId(uint64_t h, uint64_t l) noexcept : high(h), low(l) {}

  constexpr bool is_null() const noexcept { return high == 0 && low == 0; }
  constexpr explicit operator bool() const noexcept { return !is_null(); }

  bool operator==(const TopologyNodeId& o) const noexcept { return high == o.high && low == o.low; }
  bool operator!=(const TopologyNodeId& o) const noexcept { return !(*this == o); }
  bool operator<(const TopologyNodeId& o) const noexcept {
    return high < o.high || (high == o.high && low < o.low);
  }
  bool operator>(const TopologyNodeId& o) const noexcept { return o < *this; }
  bool operator<=(const TopologyNodeId& o) const noexcept { return !(o < *this); }
  bool operator>=(const TopologyNodeId& o) const noexcept { return !(*this < o); }

  // 32 hex characters (16 bytes), canonical lowercase.
  std::string to_hex() const;
  static bool try_from_hex(std::string_view s, TopologyNodeId& out) noexcept;
  static TopologyNodeId from_hex(std::string_view s);  // throws on malformed
};

// Deterministic 128-bit derivation from an arbitrary byte span.
// Stable across runs/platforms for identical bytes.
TopologyNodeId derive_node_id(std::string_view namespace_key, std::string_view category,
                              std::string_view native_identity) noexcept;

// Convenience helper for building an identity from a seed text directly.
TopologyNodeId stable_hash128(std::string_view bytes, uint64_t seed_a, uint64_t seed_b) noexcept;

// Null sentinel.
inline constexpr TopologyNodeId kNullNodeId{0, 0};

}  // namespace topology_fabric

namespace std {
template <>
struct hash<topology_fabric::TopologyNodeId> {
  size_t operator()(const topology_fabric::TopologyNodeId& id) const noexcept {
    // FNV-style mix; good enough for hash-table keys.
    uint64_t h = 1469598103934665603ull ^ id.low;
    h = (h ^ (id.low >> 32)) * 1099511628211ull;
    h ^= id.high;
    h = (h ^ (id.high >> 32)) * 1099511628211ull;
    return static_cast<size_t>(h);
  }
};
}  // namespace std
