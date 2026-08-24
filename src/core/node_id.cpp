
#include "topology_fabric/node_id.hpp"
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <cstring>

namespace topology_fabric {

namespace {

// splitmix64 finalizer.
uint64_t splitmix64(uint64_t x) noexcept {
  x += 0x9e3779b97f4a7c15ull;
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
  return x ^ (x >> 31);
}

inline void mix_in(uint64_t& state, char c) noexcept {
  state ^= static_cast<uint64_t>(static_cast<uint8_t>(c));
  state = (state << 8) | (state >> 56);
  state *= 0x100000001b3ull;
}

}  // namespace

std::string TopologyNodeId::to_hex() const {
  std::ostringstream os;
  os << std::hex << std::setfill('0') << std::setw(16) << high
     << std::setw(16) << low;
  return os.str();
}

bool TopologyNodeId::try_from_hex(std::string_view s, TopologyNodeId& out) noexcept {
  if (s.size() != 32) return false;
  uint64_t h = 0, l = 0;
  auto parse = [&](std::string_view part) -> uint64_t {
    uint64_t v = 0;
    for (char c : part) {
      uint64_t d;
      if (c >= '0' && c <= '9') d = static_cast<uint64_t>(c - '0');
      else if (c >= 'a' && c <= 'f') d = static_cast<uint64_t>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') d = static_cast<uint64_t>(c - 'A' + 10);
      else return UINT64_MAX;
      v = v * 16 + d;
    }
    return v;
  };
  h = parse(s.substr(0, 16));
  l = parse(s.substr(16, 16));
  if (h == UINT64_MAX && s.substr(0, 16) != "ffffffffffffffff") return false;
  if (l == UINT64_MAX && s.substr(16, 16) != "ffffffffffffffff") return false;
  out = TopologyNodeId(h, l);
  return true;
}

TopologyNodeId TopologyNodeId::from_hex(std::string_view s) {
  TopologyNodeId out;
  if (!try_from_hex(s, out))
    throw std::invalid_argument("TopologyNodeId: malformed 128-bit hex string");
  return out;
}

TopologyNodeId stable_hash128(std::string_view bytes, uint64_t seed_a, uint64_t seed_b) noexcept {
  uint64_t a = 0xcbf29ce484222325ull ^ seed_a;
  uint64_t b = 0x9e3779b97f4a7c15ull ^ seed_b;
  for (char c : bytes) {
    mix_in(a, c);
    mix_in(b, static_cast<char>(~static_cast<uint8_t>(c)));
  }
  a = splitmix64(a ^ (a >> 32));
  b = splitmix64(b ^ (b >> 32) ^ a);
  a ^= splitmix64(b ^ (b >> 32));
  return TopologyNodeId(a, b);
}

TopologyNodeId derive_node_id(std::string_view namespace_key, std::string_view category,
                              std::string_view native_identity) noexcept {
  std::string buf;
  buf.reserve(namespace_key.size() + category.size() + native_identity.size() + 3);
  buf.append(namespace_key.data(), namespace_key.size());
  buf.push_back('|');
  buf.append(category.data(), category.size());
  buf.push_back('|');
  buf.append(native_identity.data(), native_identity.size());
  const uint64_t na = 0x746f706f6c6f6779ull;  // "topology"
  const uint64_t nb = 0x6661627269633130ull;  // "fabric10"
  return stable_hash128(buf, na, nb);
}

}  // namespace topology_fabric
