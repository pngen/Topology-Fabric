
// TopologyFabric/edge.hpp - an explicit relationship between topology nodes.
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include "topology_fabric/types.hpp"
#include "topology_fabric/node_id.hpp"
#include "topology_fabric/value.hpp"
#include "topology_fabric/provenance.hpp"

namespace topology_fabric {

// A directed or undirected relationship between two nodes. For undirected edges
// the source/target are stored in a canonical order (source <= target).
struct TopologyEdge {
  TopologyNodeId source;
  TopologyNodeId target;
  EdgeType type = EdgeType::CONNECTED_TO;
  EdgeDirection direction = EdgeDirection::DIRECTED;

  Provenance provenance;
  Confidence confidence = Confidence::UNKNOWN;

  // Link metadata (only set when known; never fabricated).
  std::optional<int> width;                     // lanes
  std::optional<int> pcie_generation;           // 3,4,5,...
  std::optional<double> bandwidth_bytes_per_sec;
  std::optional<double> latency_ns;
  int hop_count = 0;
  std::optional<double> locality_score;
  bool accessible = true;                       // edge represents an accessible path
  std::optional<uint64_t> peer_capability;      // raw capability bits from vendor
  uint32_t policy_version = 0;

  PropertyMap properties;

  // Canonical identity used for diffing/merge (order-insensitive for undirected).
  struct Key {
    TopologyNodeId source, target;
    EdgeType type;
    bool operator==(const Key& o) const noexcept {
      return source == o.source && target == o.target && type == o.type;
    }
  };
  Key key() const;
};

}  // namespace topology_fabric

namespace std {
template <>
struct hash<topology_fabric::TopologyEdge::Key> {
  size_t operator()(const topology_fabric::TopologyEdge::Key& k) const noexcept {
    size_t h = std::hash<topology_fabric::TopologyNodeId>{}(k.source);
    h ^= std::hash<topology_fabric::TopologyNodeId>{}(k.target) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    h ^= static_cast<size_t>(static_cast<uint8_t>(k.type)) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    return h;
  }
};
}  // namespace std
