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
  auto ex = topology_fabric::explain(*snap, a, b, topology_fabric::CostWeights{});
  std::cout << "summary: " << ex.summary << "\n";
  for (auto& f : ex.factors) std::cout << "  " << f.name << "=" << f.value << " (" << f.note << ")\n";
  return 0;
}