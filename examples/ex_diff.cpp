#include "ex_common.hpp"
int main() {
  topology_fabric::TopologyRuntime rt; rt.register_builtin_providers();
  auto s1 = rt.discover();
  auto s2 = rt.discover();  // second run (may or may not be a material change)
  auto d = topology_fabric::compare_snapshots(*s1, *s2);
  std::cout << "generation " << d.generation_before << " -> " << d.generation_after << "\n";
  for (auto& e : d.events) std::cout << "  " << std::string(to_string(e.kind)) << " " << e.node.to_hex() << "\n";
  return 0;
}