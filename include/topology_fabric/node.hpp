
// TopologyFabric/node.hpp - a discovered topology resource node.
#pragma once
#include <string>
#include "topology_fabric/types.hpp"
#include "topology_fabric/node_id.hpp"
#include "topology_fabric/identity.hpp"
#include "topology_fabric/value.hpp"
#include "topology_fabric/provenance.hpp"
#include "topology_fabric/capability.hpp"

namespace topology_fabric {

// A single resource in the topology graph. Every discovered resource is a node.
struct TopologyNode {
  TopologyNodeId id;
  NodeType type = NodeType::UNKNOWN;
  std::string category;       // extension/unknown category name
  std::string name;
  std::string display_name;
  NativeIdentity native;
  Capability capabilities = Capability::NONE;
  PropertyMap properties;
  Provenance provenance;
  Confidence confidence = Confidence::UNKNOWN;
  bool synthetic = false;     // imported or synthetic topologies are clearly marked

  bool has_capability(Capability c) const noexcept { return has(capabilities, c); }
};

}  // namespace topology_fabric
