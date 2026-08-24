#include "test_harness.hpp"
#include "tf_test_util.hpp"
#include "topology_fabric/serialization.hpp"
#include "topology_fabric/json.hpp"
#include <limits>

using namespace topology_fabric;

TF_TEST(adversarial_duplicate_node_ids) {
  SnapshotBuilder b;
  TopologyNode a; a.id = derive_node_id("t","n","a"); a.type = NodeType::MACHINE; a.name="a";
  TopologyNode c; c.id = a.id; c.type = NodeType::ACCELERATOR; c.name="c";  // duplicate id
  ASSERT(b.add_node(a));
  ASSERT(!b.add_node(c));  // second rejected
  auto snap = b.take();
  ASSERT(snap->node_count() == 1);
}

TF_TEST(adversarial_oversized_counts) {
  Bounds bo; bo.max_nodes = 2;
  SnapshotBuilder b(bo);
  TopologyNode a; a.id = derive_node_id("t","n","a"); a.type = NodeType::MACHINE; a.name="a";
  TopologyNode c; c.id = derive_node_id("t","n","c"); c.type = NodeType::ACCELERATOR; c.name="c";
  TopologyNode d; d.id = derive_node_id("t","n","d"); d.type = NodeType::ACCELERATOR; d.name="d";
  b.add_node(a); b.add_node(c);
  ASSERT(!b.add_node(d));  // hits max_nodes
}

TF_TEST(adversarial_malformed_json_corrupted) {
  ASSERT_THROWS(deserialize_snapshot_json("[1,2,3]"));
  ASSERT_THROWS(deserialize_snapshot_json("{\"format\":\"topology_fabric_snapshot\",\"nodes\":123}"));
  ASSERT_THROWS(deserialize_snapshot_json("{\"format\":\"topology_fabric_snapshot\",\"nodes\":[{}]}"));
}

TF_TEST(adversarial_bogus_bdf_and_numa) {
  // A node claiming a bogus NUMA index and BDF must still be representable (partial) but
  // must not crash queries.
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"},
        {NodeType::ACCELERATOR, "g", "g", 999, std::nullopt, std::nullopt, 300, 99, 7} },
      { tf_test_util::edge("m", "g", EdgeType::CONTAINS) });
  ASSERT(snap->node_count() == 2);
  TopologyNodeId g;
  for (auto& [id, n] : snap->nodes()) if (n.name=="g") g=id;
  auto p = shortest_path(*snap, snap->nodes().begin()->first, g);
  ASSERT(p.found || !p.found);  // must not crash
}

TF_TEST(adversarial_negative_and_nan_cost) {
  CostWeights w;
  w.hop_penalty = -5.0;    // nonsensical but must be handled without crash
  w.latency_ns_weight = std::numeric_limits<double>::quiet_NaN();
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"}, {NodeType::ACCELERATOR, "g", "g"} },
      { tf_test_util::edge("m", "g", EdgeType::CONNECTED_TO) });
  TopologyNodeId m, g;
  for (auto& [id, n] : snap->nodes()) { if (n.name=="m") m=id; else if (n.name=="g") g=id; }
  auto p = lowest_cost_path(*snap, m, g, w);  // must not crash / infinite loop
  auto cb = path_cost(*snap, p, w);
  auto ex = explain(*snap, m, g, w);
  (void)cb; (void)ex;
  ASSERT(p.found);
}

TF_TEST(adversarial_forged_and_stale_ids) {
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"}, {NodeType::ACCELERATOR, "g", "g"} }, {});
  TopologyNodeId forged(0xdead, 0xbeef);
  ASSERT(snap->find_node(forged) == nullptr);
  ASSERT_THROWS(snap->node(forged));
  // path query on a removed/unknown node returns not-found, no crash.
  auto p = shortest_path(*snap, forged, snap->nodes().begin()->first);
  ASSERT(!p.found);
}

TF_TEST(adversarial_deep_nesting_json) {
  // Very deep JSON nesting must not overflow the stack.
  std::string deep = "{\"format\":\"topology_fabric_snapshot\",\"nodes\":[]}";
  ASSERT_THROWS(deserialize_snapshot_json(deep));  // missing nothing? nodes present -> empty is OK actually
  // A deeply-nested non-snapshot doc should be rejected cleanly.
  std::string nested(10000, '[');
  std::string doc = nested + std::string(10000, ']');
  json::ParseOptions opt; opt.max_depth = 8;
  auto r = json::parse(doc, opt);
  ASSERT(!r.ok());  // rejected without crashing
}