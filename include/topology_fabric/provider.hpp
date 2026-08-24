
// TopologyFabric/provider.hpp - discovery provider interface and contribution model.
#pragma once
#include <string>
#include <vector>
#include "topology_fabric/types.hpp"
#include "topology_fabric/identity.hpp"
#include "topology_fabric/value.hpp"
#include "topology_fabric/provenance.hpp"
#include "topology_fabric/capability.hpp"
#include "topology_fabric/snapshot.hpp"

namespace topology_fabric {

// Context handed to providers during discovery.
struct DiscoveryContext {
  Bounds bounds;                       // resource limits
  std::vector<std::string> warnings;   // provider-collected warnings
  int64_t started_ms = 0;              // wall-clock epoch ms
  bool allow_measurement = false;      // measurement providers honor this
};

// A node a provider wants to add to the merged graph.
struct ContributedNode {
  std::string ref;                     // provider-local stable ref (ideally a canonical key)
  NodeType type = NodeType::UNKNOWN;
  std::string category;
  std::string name;
  NativeIdentity native;
  Capability capabilities = Capability::NONE;
  PropertyMap properties;
  Provenance provenance;
  bool synthetic = false;
};

// An edge between contributed nodes, addressed by their refs.
struct ContributedEdge {
  std::string from_ref;
  std::string to_ref;
  EdgeType type = EdgeType::CONNECTED_TO;
  EdgeDirection direction = EdgeDirection::DIRECTED;
  Provenance provenance;
  Confidence confidence = Confidence::UNKNOWN;
  std::optional<int> width;
  std::optional<int> pcie_generation;
  std::optional<double> bandwidth_bytes_per_sec;
  std::optional<double> latency_ns;
  int hop_count = 0;
  std::optional<uint64_t> peer_capability;
  PropertyMap properties;
};

// Result of a single provider run.
struct Contribution {
  std::string provider;
  std::string version;
  std::vector<ContributedNode> nodes;
  std::vector<ContributedEdge> edges;
  std::vector<std::string> warnings;
  bool partial = false;
  bool success = false;
};

// A topology discovery provider. Providers observe platform facts and contribute
// them independently; a deterministic merge combines them into one graph.
class TopologyProvider {
 public:
  virtual ~TopologyProvider() = default;
  virtual std::string name() const = 0;
  virtual std::string version() const = 0;
  virtual Contribution discover(const DiscoveryContext& ctx) = 0;

  bool available() const noexcept { return available_; }
 protected:
  void mark_available(bool a) noexcept { available_ = a; }
 private:
  bool available_ = true;
};

}  // namespace topology_fabric
