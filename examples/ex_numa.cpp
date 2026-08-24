#include "ex_common.hpp"
int main() {
  topology_fabric::TopologyRuntime rt; rt.register_builtin_providers();
  auto snap = rt.discover();
  for (const auto& [id, n] : snap->nodes()) {
    if (n.type == topology_fabric::NodeType::NUMA_NODE || n.type == topology_fabric::NodeType::HOST_MEMORY_DOMAIN) {
      std::cout << std::string(topology_fabric::to_string(n.type)) << " " << n.name
                << " numa=" << (n.native.numa_node ? std::to_string(*n.native.numa_node) : "?") << "\n";
    }
  }
  return 0;
}