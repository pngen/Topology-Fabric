#include "ex_common.hpp"
int main() {
  topology_fabric::TopologyRuntime rt; rt.register_builtin_providers();
  auto snap = rt.discover();
  std::string json = topology_fabric::serialize_snapshot_json(*snap, true);
  std::cout << "snapshot_id=" << snap->metadata().snapshot_id << "\n";
  std::cout << "serialized_bytes=" << json.size() << "\n";
  auto back = topology_fabric::deserialize_snapshot_json(json);
  std::cout << "roundtrip nodes=" << back->node_count() << " edges=" << back->edge_count() << "\n";
  return 0;
}