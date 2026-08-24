#include "ex_common.hpp"
int main() {
  topology_fabric::TopologyRuntime rt; rt.register_builtin_providers();
  auto snap = rt.discover();
  topology_fabric::TopologyNodeId gpu = topology_fabric::kNullNodeId, cpu = topology_fabric::kNullNodeId;
  for (const auto& [id, n] : snap->nodes()) {
    if (gpu.is_null() && n.type == topology_fabric::NodeType::ACCELERATOR) gpu = id;
    if (cpu.is_null() && n.type == topology_fabric::NodeType::CPU_THREAD) cpu = id;
  }
  if (gpu.is_null() || cpu.is_null()) { std::cout << "no accelerator/cpu\n"; return 0; }
  auto loc = topology_fabric::locality_between(*snap, cpu, gpu);
  auto cls = topology_fabric::classify_path(*snap, cpu, gpu);
  std::cout << "accelerator<->cpu locality=" << std::string(topology_fabric::to_string(loc))
            << " path_class=" << std::string(topology_fabric::to_string(cls)) << "\n";
  return 0;
}