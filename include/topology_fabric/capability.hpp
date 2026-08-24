
// TopologyFabric/capability.hpp - explicit capability bitmask.
#pragma once
#include <cstdint>
#include <string>

namespace topology_fabric {

enum class Capability : uint64_t {
  NONE              = 0,
  CPU_ADDRESSABLE   = 1ull << 0,
  DEVICE_ADDRESSABLE= 1ull << 1,
  DMA_CAPABLE       = 1ull << 2,
  PEER_ACCESS       = 1ull << 3,
  DIRECT_TRANSFER   = 1ull << 4,
  STAGED_TRANSFER   = 1ull << 5,
  SHARED_MEMORY     = 1ull << 6,
  PERSISTENT        = 1ull << 7,
  COHERENT          = 1ull << 8,
  NUMA_LOCAL        = 1ull << 9,
  PCI_CONNECTED     = 1ull << 10,
  NETWORK_CONNECTED = 1ull << 11,
  STORAGE_BACKED    = 1ull << 12,
};

inline constexpr Capability operator|(Capability a, Capability b) noexcept {
  return static_cast<Capability>(static_cast<uint64_t>(a) | static_cast<uint64_t>(b));
}
inline constexpr Capability operator&(Capability a, Capability b) noexcept {
  return static_cast<Capability>(static_cast<uint64_t>(a) & static_cast<uint64_t>(b));
}
inline constexpr Capability operator~(Capability a) noexcept {
  return static_cast<Capability>(~static_cast<uint64_t>(a));
}
inline constexpr bool has(Capability set, Capability flag) noexcept {
  return (static_cast<uint64_t>(set) & static_cast<uint64_t>(flag)) != 0;
}
inline constexpr void add(Capability& set, Capability flag) noexcept { set = set | flag; }

inline uint64_t to_uint(Capability c) noexcept { return static_cast<uint64_t>(c); }
inline Capability capability_from_uint(uint64_t v) noexcept { return static_cast<Capability>(v); }

}  // namespace topology_fabric