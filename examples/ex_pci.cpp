#include "ex_common.hpp"
int main() {
  topology_fabric::TopologyRuntime rt; rt.register_builtin_providers();
  auto snap = rt.discover();
  for (const auto& [id, n] : snap->nodes()) {
    if (n.type == topology_fabric::NodeType::PCI_ROOT || n.type == topology_fabric::NodeType::PCI_BRIDGE ||
        n.type == topology_fabric::NodeType::PCI_DEVICE) {
      std::cout << std::string(topology_fabric::to_string(n.type)) << " " << n.name
                << " bdf=" << n.native.pci_bdf_string() << "\n";
    }
  }
  return 0;
}