#include "test_harness.hpp"
#include "tf_test_util.hpp"

using namespace topology_fabric;

// Multi-GPU synthetic topology: machine -> root0 -> {g0, g1}, machine -> root1 -> g2, g3 peer g0.

TF_TEST(ranking_two_gpus_same_root) {
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"},
        {NodeType::PCI_ROOT, "r", "r"},
        {NodeType::ACCELERATOR, "g0", "g0", 0, std::nullopt, std::nullopt, 0, 0, 0},
        {NodeType::ACCELERATOR, "g1", "g1", 0, std::nullopt, std::nullopt, 0, 1, 0},
        {NodeType::CPU_THREAD, "t0", "t0", 0} },
      { tf_test_util::edge("m", "r", EdgeType::CONTAINS),
        tf_test_util::edge("r", "g0", EdgeType::CONTAINS),
        tf_test_util::edge("r", "g1", EdgeType::CONTAINS),
        tf_test_util::edge("m", "t0", EdgeType::CONTAINS) });
  TopologyNodeId t0;
  for (auto& [id, n] : snap->nodes()) if (n.name=="t0") t0=id;
  auto rr = rank_candidates(*snap, t0, NodeType::ACCELERATOR, 0, CostWeights{});
  ASSERT(rr.found);
  ASSERT(rr.ranked.size() == 2);
  for (auto& e : rr.ranked) ASSERT(e.locality == LocalityClass::SAME_NUMA);
}

TF_TEST(ranking_cross_root_prefers_same_root) {
  // g0 under root0 (closer), g2 under root1 (farther). Node with same root ranks first.
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"},
        {NodeType::PCI_ROOT, "r0", "r0"},
        {NodeType::PCI_ROOT, "r1", "r1"},
        {NodeType::ACCELERATOR, "g0", "g0", 0, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt},
        {NodeType::ACCELERATOR, "g2", "g2", 1, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt},
        {NodeType::CPU_THREAD, "t0", "t0", 0} },
      { tf_test_util::edge("m", "r0", EdgeType::CONTAINS),
        tf_test_util::edge("m", "r1", EdgeType::CONTAINS),
        tf_test_util::edge("r0", "g0", EdgeType::CONTAINS),
        tf_test_util::edge("r1", "g2", EdgeType::CONTAINS),
        tf_test_util::edge("m", "t0", EdgeType::CONTAINS) });
  TopologyNodeId t0;
  for (auto& [id, n] : snap->nodes()) if (n.name=="t0") t0=id;
  auto rr = rank_candidates(*snap, t0, NodeType::ACCELERATOR, 0, CostWeights{});
  ASSERT(rr.found && rr.ranked.size() == 2);
  std::string firstName;
  for (auto& [id, n] : snap->nodes()) if (id == rr.ranked[0].id) firstName = n.name;
  ASSERT(firstName == "g0");  // same root, same numa -> cheaper
}

TF_TEST(ranking_acl_to_acl_peer) {
  // g0 and g1 are peers (direct P2P); g0 and g2 are not (differ root).
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"},
        {NodeType::PCI_ROOT, "r0", "r0"}, {NodeType::PCI_ROOT, "r1", "r1"},
        {NodeType::ACCELERATOR, "g0", "g0", 0, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt},
        {NodeType::ACCELERATOR, "g1", "g1", 0, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt},
        {NodeType::ACCELERATOR, "g2", "g2", 1, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt} },
      { tf_test_util::edge("m", "r0", EdgeType::CONTAINS),
        tf_test_util::edge("m", "r1", EdgeType::CONTAINS),
        tf_test_util::edge("r0", "g0", EdgeType::CONTAINS),
        tf_test_util::edge("r0", "g1", EdgeType::CONTAINS),
        tf_test_util::edge("r1", "g2", EdgeType::CONTAINS),
        tf_test_util::edge("g0", "g1", EdgeType::PEER_TO, EdgeDirection::UNDIRECTED, 100e9, 50.0) });
  TopologyNodeId g0;
  for (auto& [id, n] : snap->nodes()) if (n.name=="g0") g0=id;
  auto rr = rank_candidates(*snap, g0, NodeType::ACCELERATOR, 0, CostWeights{});
  ASSERT(rr.found && rr.ranked.size() == 2);
  std::string first;
  for (auto& [id, n] : snap->nodes()) if (id == rr.ranked[0].id) first = n.name;
  ASSERT(first == "g1");  // direct peer cheaper than cross-root
}

TF_TEST(ranking_deterministic) {
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"},
        {NodeType::ACCELERATOR, "g0", "g0"}, {NodeType::ACCELERATOR, "g1", "g1"},
        {NodeType::CPU_THREAD, "t0", "t0"} },
      { tf_test_util::edge("m", "g0", EdgeType::CONTAINS),
        tf_test_util::edge("m", "g1", EdgeType::CONTAINS),
        tf_test_util::edge("m", "t0", EdgeType::CONTAINS) });
  TopologyNodeId t0;
  for (auto& [id, n] : snap->nodes()) if (n.name=="t0") t0=id;
  auto a = rank_candidates(*snap, t0, NodeType::ACCELERATOR, 0, CostWeights{});
  auto b = rank_candidates(*snap, t0, NodeType::ACCELERATOR, 0, CostWeights{});
  ASSERT(a.ranked.size() == b.ranked.size());
  for (size_t i = 0; i < a.ranked.size(); ++i) ASSERT(a.ranked[i].id == b.ranked[i].id);
}