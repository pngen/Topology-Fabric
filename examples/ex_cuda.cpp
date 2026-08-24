#include "ex_common.hpp"
int main() {
  topology_fabric::TopologyRuntime rt; rt.register_builtin_providers();
  auto snap = rt.discover();
  for (const auto& [id, n] : snap->nodes()) {
    if (n.type == topology_fabric::NodeType::ACCELERATOR) {
      std::cout << "accelerator: " << n.name << " bdf=" << n.native.pci_bdf_string()
                << " ordinal=" << (n.native.cuda_ordinal ? std::to_string(*n.native.cuda_ordinal) : "?")
                << " uuid=" << n.native.cuda_uuid << "\n";
      auto mem = n.properties.find("cuda.total_memory");
      if (mem != n.properties.end()) std::cout << "  total_memory=" << mem->second.as_uint() << " bytes\n";
    }
  }
  return 0;
}