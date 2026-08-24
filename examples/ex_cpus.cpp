#include "ex_common.hpp"
int main() {
  topology_fabric::TopologyRuntime rt; rt.register_builtin_providers();
  auto snap = rt.discover();
  for (const auto& [id, n] : snap->nodes()) {
    if (n.type == topology_fabric::NodeType::CPU_PACKAGE || n.type == topology_fabric::NodeType::CPU_CORE ||
        n.type == topology_fabric::NodeType::CPU_THREAD) {
      std::cout << std::string(topology_fabric::to_string(n.type)) << " " << n.name
                << " pkg=" << (n.native.cpu_package ? std::to_string(*n.native.cpu_package) : "?")
                << " core=" << (n.native.core_id ? std::to_string(*n.native.core_id) : "?")
                << " numa=" << (n.native.numa_node ? std::to_string(*n.native.numa_node) : "?") << "\n";
    }
  }
  return 0;
}