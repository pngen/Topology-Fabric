#include "ex_common.hpp"
int main() {
  topology_fabric::TopologyRuntime rt;
  rt.register_builtin_providers();
  auto snap = rt.discover();
  std::cout << "Topology Fabric snapshot: nodes=" << snap->node_count()
            << " edges=" << snap->edge_count()
            << " generation=" << snap->metadata().generation << "\n";
  std::cout << "valid=" << (snap->validation().ok ? "yes" : "no") << "\n";
  std::cout << "providers=" << snap->metadata().provider_versions << "\n";
  return 0;
}