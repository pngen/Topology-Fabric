#include "ex_common.hpp"
int main() {
  // Synthetic multi-GPU topology (clearly labeled synthetic -- NOT real hardware).
  topology_fabric::Bounds b;
  topology_fabric::SnapshotBuilder sb(b);
  topology_fabric::SnapshotMetadata meta; meta.synthetic = true; meta.machine_identity = "synthetic-4gpu";
  sb.set_metadata(meta);
  auto mk = [](const std::string& ref, topology_fabric::NodeType t, const std::string& name, std::optional<uint32_t> numa) {
    topology_fabric::TopologyNode n;
    n.id = topology_fabric::derive_node_id("ex", "node", ref);
    n.type = t; n.name = name; n.native.numa_node = numa;
    n.provenance = topology_fabric::Provenance::user_supplied(topology_fabric::Confidence::HIGH, "synthetic");
    return n;
  };
  auto m = mk("m", topology_fabric::NodeType::MACHINE, "machine", std::nullopt);
  auto r0 = mk("r0", topology_fabric::NodeType::PCI_ROOT, "root0", std::nullopt);
  auto g0 = mk("g0", topology_fabric::NodeType::ACCELERATOR, "gpu0", 0);
  auto g1 = mk("g1", topology_fabric::NodeType::ACCELERATOR, "gpu1", 0);
  auto g2 = mk("g2", topology_fabric::NodeType::ACCELERATOR, "gpu2", 1);
  auto g3 = mk("g3", topology_fabric::NodeType::ACCELERATOR, "gpu3", 1);
  sb.add_node(m); sb.add_node(r0); sb.add_node(g0); sb.add_node(g1); sb.add_node(g2); sb.add_node(g3);
  auto add = [&](topology_fabric::TopologyNodeId a, topology_fabric::TopologyNodeId c, topology_fabric::EdgeType t) {
    topology_fabric::TopologyEdge e; e.source=a; e.target=c; e.type=t; e.direction=topology_fabric::EdgeDirection::DIRECTED;
    e.provenance = topology_fabric::Provenance::user_supplied(topology_fabric::Confidence::HIGH, "synthetic"); sb.add_edge(e);
  };
  add(m.id, r0.id, topology_fabric::EdgeType::CONTAINS);
  add(r0.id, g0.id, topology_fabric::EdgeType::CONTAINS);
  add(r0.id, g1.id, topology_fabric::EdgeType::CONTAINS);
  add(m.id, g2.id, topology_fabric::EdgeType::CONTAINS);
  add(m.id, g3.id, topology_fabric::EdgeType::CONTAINS);
  add(g0.id, g1.id, topology_fabric::EdgeType::PEER_TO);
  auto snap = sb.take();
  std::cout << "synthetic multi-GPU snapshot: nodes=" << snap->node_count() << "\n";
  auto rr = topology_fabric::rank_candidates(*snap, g0.id, topology_fabric::NodeType::ACCELERATOR, 0, topology_fabric::CostWeights{});
  for (auto& e : rr.ranked) { const auto* n = snap->find_node(e.id); std::cout << "  " << (n?n->name:"?") << " cost=" << e.cost << "\n"; }
  (void)b;
  return 0;
}