
// TopologyFabric/diff.hpp - structured topology snapshot diffing.
#pragma once
#include <vector>
#include <string>
#include "topology_fabric/types.hpp"
#include "topology_fabric/snapshot.hpp"
#include "topology_fabric/node_id.hpp"

namespace topology_fabric {

struct DiffEvent {
  DiffEventKind kind = DiffEventKind::PROPERTY_CHANGED;
  TopologyNodeId node;       // primary node (null when structural on edge only)
  TopologyNodeId node2;      // secondary node (edge target)
  std::string key;           // property/attribute name
  std::string before;        // human-readable before
  std::string after;         // human-readable after
  bool on_edge = false;      // event refers to an edge property
};

struct TopologyDiff {
  uint64_t generation_before = 0;
  uint64_t generation_after = 0;
  bool material_change = false;
  std::vector<DiffEvent> events;
};

// Compare two snapshots. Deterministic event ordering.
TopologyDiff compare_snapshots(const TopologySnapshot& before, const TopologySnapshot& after);

// True if a diff (or a single event) represents a material topology change.
bool is_material_change(const TopologyDiff& diff);
bool is_material_event(const DiffEvent& e);

}  // namespace topology_fabric
