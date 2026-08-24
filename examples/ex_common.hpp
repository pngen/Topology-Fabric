// examples/ex_common.hpp - small helpers shared by Topology Fabric examples.
#pragma once
#include "topology_fabric/runtime.hpp"
#include "topology_fabric/query.hpp"
#include "topology_fabric/serialization.hpp"
#include <iostream>
#include <string>

namespace ex {
using namespace topology_fabric;

inline TopologyNodeId find_node(const TopologySnapshot& s, const std::string& name) {
  for (const auto& [id, n] : s.nodes()) if (n.name == name || n.display_name == name) return id;
  return kNullNodeId;
}

inline void print_path(const TopologySnapshot& s, const TopologyPath& p) {
  if (!p.found) { std::cout << "no path\n"; return; }
  std::cout << "class=" << std::string(to_string(p.path_class))
            << " locality=" << std::string(to_string(p.locality))
            << " hops=" << p.hop_count << " cost=" << p.total_cost << "\n";
}
}  // namespace ex