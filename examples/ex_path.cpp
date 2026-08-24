#include "ex_common.hpp"
int main() {
  topology_fabric::TopologyRuntime rt; rt.register_builtin_providers();
  auto snap = rt.discover();
  topology_fabric::TopologyNodeId a = topology_fabric::kNullNodeId, b = topology_fabric::kNullNodeId;
  for (const auto& [id, n] : snap->nodes()) {
    if (a.is_null() && n.type == topology_fabric::NodeType::CPU_THREAD) a = id;
    else if (b.is_null() && n.type == topology_fabric::NodeType::ACCELERATOR) b = id;
  }
  if (a.is_null() || b.is_null()) { std::cout << "insufficient topology\n"; return 0; }
  auto p = topology_fabric::lowest_cost_path(*snap, a, b, topology_fabric::CostWeights{});
  ex::print_path(*snap, p);
  return 0;
}