
// TopologyFabric/merge.hpp - deterministic merge of provider contributions.
#pragma once
#include <vector>
#include <string>
#include "topology_fabric/provider.hpp"
#include "topology_fabric/node.hpp"
#include "topology_fabric/edge.hpp"
#include "topology_fabric/snapshot.hpp"

namespace topology_fabric {

// Merged graph materialized from provider contributions. Endpoints of edges are
// resolved via provider refs / canonical merge keys.
struct MergedGraph {
  std::unordered_map<TopologyNodeId, TopologyNode> nodes;
  std::vector<TopologyEdge> edges;
  std::vector<std::string> warnings;
  std::vector<std::string> conflicts;
  size_t merged_count = 0;      // number of input nodes collapsed into merged nodes
};

// Merge all contributions into a single graph structure. Deterministic for the
// same contributions. Never silently overwrites authority (recorded in conflicts).
MergedGraph merge_contributions(const std::vector<Contribution>& contributions,
                                const Bounds& bounds);

}  // namespace topology_fabric
