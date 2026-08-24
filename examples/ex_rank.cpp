#include "ex_common.hpp"
int main() {
  topology_fabric::TopologyRuntime rt; rt.register_builtin_providers();
  auto snap = rt.discover();
  topology_fabric::TopologyNodeId src = topology_fabric::kNullNodeId;
  for (const auto& [id, n] : snap->nodes()) if (n.type == topology_fabric::NodeType::CPU_THREAD) { src = id; break; }
  if (src.is_null()) { std::cout << "no cpu\n"; return 0; }
  auto rr = topology_fabric::rank_candidates(*snap, src, topology_fabric::NodeType::ACCELERATOR, 0, topology_fabric::CostWeights{});
  for (auto& e : rr.ranked) {
    const auto* n = snap->find_node(e.id);
    std::cout << (n?n->name:"?") << " cost=" << e.cost << " " << std::string(topology_fabric::to_string(e.locality)) << "\n";
  }
  return 0;
}