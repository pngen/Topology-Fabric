#include "ex_common.hpp"
int main() {
  // Build an invalid graph (dangling edge) -> validation reports error.
  topology_fabric::SnapshotBuilder b;
  topology_fabric::TopologyNode m;
  m.id = topology_fabric::derive_node_id("ex","n","m"); m.type = topology_fabric::NodeType::MACHINE; m.name="machine";
  b.add_node(m);
  topology_fabric::TopologyEdge e;
  e.source = m.id; e.target = topology_fabric::derive_node_id("ex","n","ghost"); e.type = topology_fabric::EdgeType::CONNECTED_TO;
  b.add_edge(e);
  auto snap = b.take();
  std::cout << "validation ok=" << (snap->validation().ok ? "yes" : "no") << "\n";
  for (auto& err : snap->validation().errors) std::cout << "  error: " << err << "\n";
  for (auto& w : snap->validation().warnings) std::cout << "  warning: " << w << "\n";
  return 0;
}