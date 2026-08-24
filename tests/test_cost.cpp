#include "test_harness.hpp"
#include "tf_test_util.hpp"

using namespace topology_fabric;

TF_TEST(cost_composition) {
  // A single-edge path with known bandwidth/latency should yield deterministic cost components.
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"},
        {NodeType::ACCELERATOR, "g", "g", 0, std::nullopt, std::nullopt, 1, 0, 0} },
      { tf_test_util::edge("m", "g", EdgeType::CONNECTED_TO, EdgeDirection::UNDIRECTED, 32e9, 100.0) });
  TopologyNodeId m, g;
  for (auto& [id, n] : snap->nodes()) { if (n.name=="m") m=id; else if (n.name=="g") g=id; }
  CostWeights w;
  auto p = lowest_cost_path(*snap, m, g, w);
  ASSERT(p.found);
  auto cb = cost_breakdown(*snap, p, w);
  ASSERT(cb.hop_penalty >= w.hop_penalty);
  ASSERT(cb.latency_term > 0.0);
  ASSERT(cb.inverse_bandwidth_term > 0.0);
  ASSERT(cb.total > 0.0);
}

TF_TEST(cost_total_matches_sum) {
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"},
        {NodeType::ACCELERATOR, "g", "g"},
        {NodeType::STORAGE_DEVICE, "s", "s"} },
      { tf_test_util::edge("m", "g", EdgeType::CONNECTED_TO, EdgeDirection::UNDIRECTED, 32e9, 100.0),
        tf_test_util::edge("m", "s", EdgeType::CONNECTED_TO, EdgeDirection::UNDIRECTED, 8e9, 200.0) });
  TopologyNodeId m, g;
  for (auto& [id, n] : snap->nodes()) { if (n.name=="m") m=id; else if (n.name=="g") g=id; }
  auto p = lowest_cost_path(*snap, m, g, CostWeights{});
  auto cb = path_cost(*snap, p, CostWeights{});
  double sum = cb.hop_penalty + cb.numa_penalty + cb.pci_bridge_penalty + cb.root_crossing_penalty +
               cb.latency_term + cb.inverse_bandwidth_term + cb.uncertainty_penalty +
               cb.policy_penalty + cb.node_transition_penalty;
  ASSERT_NEAR(cb.total, sum, 1e-9);
  ASSERT_NEAR(cb.total, p.total_cost, 1e-9);
}

TF_TEST(cost_weights_configurable) {
  // A single edge between nodes on different NUMA nodes: raising the NUMA penalty
  // must increase cost.
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::CPU_THREAD, "t0", "t0", 0},
        {NodeType::ACCELERATOR, "g", "g", 1} },
      { tf_test_util::edge("t0", "g", EdgeType::CONNECTED_TO, EdgeDirection::UNDIRECTED) });
  TopologyNodeId t0, g;
  for (auto& [id, n] : snap->nodes()) { if (n.name=="t0") t0=id; else if (n.name=="g") g=id; }
  CostWeights a; a.numa_penalty = 0.0;
  CostWeights b; b.numa_penalty = 500.0;
  auto pa = lowest_cost_path(*snap, t0, g, a);
  auto pb = lowest_cost_path(*snap, t0, g, b);
  ASSERT(pa.total_cost < pb.total_cost);
}

TF_TEST(cost_deterministic) {
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"}, {NodeType::ACCELERATOR, "g", "g"} },
      { tf_test_util::edge("m", "g", EdgeType::CONNECTED_TO) });
  TopologyNodeId m, g;
  for (auto& [id, n] : snap->nodes()) { if (n.name=="m") m=id; else if (n.name=="g") g=id; }
  auto p1 = lowest_cost_path(*snap, m, g, CostWeights{});
  auto p2 = lowest_cost_path(*snap, m, g, CostWeights{});
  ASSERT_NEAR(p1.total_cost, p2.total_cost, 1e-12);
}