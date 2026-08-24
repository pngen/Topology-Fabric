#include "test_harness.hpp"
#include "tf_test_util.hpp"
#include "topology_fabric/serialization.hpp"

using namespace topology_fabric;

TF_TEST(ser_roundtrip_snapshot) {
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"},
        {NodeType::ACCELERATOR, "g", "g", 0, std::nullopt, std::nullopt, 1, 0, 0, Capability::DMA_CAPABLE | Capability::DEVICE_ADDRESSABLE},
        {NodeType::CPU_THREAD, "t0", "t0", 0} },
      { tf_test_util::edge("m", "g", EdgeType::CONTAINS),
        tf_test_util::edge("m", "t0", EdgeType::CONTAINS) });
  std::string json = serialize_snapshot_json(*snap);
  ASSERT(!json.empty());
  auto back = deserialize_snapshot_json(json);
  ASSERT(back->node_count() == snap->node_count());
  ASSERT(back->edge_count() == snap->edge_count());
  // Node ids and names preserved.
  for (const auto& [id, n] : snap->nodes()) {
    auto* b = back->find_node(id);
    ASSERT(b != nullptr);
    ASSERT(b->name == n.name);
    ASSERT(b->type == n.type);
  }
}

TF_TEST(ser_compact_and_pretty) {
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"}, {NodeType::ACCELERATOR, "g", "g"} },
      { tf_test_util::edge("m", "g", EdgeType::CONNECTED_TO) });
  auto compact = serialize_snapshot_json(*snap, false);
  auto pretty = serialize_snapshot_json(*snap, true);
  ASSERT(compact.size() <= pretty.size());  // pretty has extra whitespace
  auto b1 = deserialize_snapshot_json(compact);
  auto b2 = deserialize_snapshot_json(pretty);
  ASSERT(b1->node_count() == b2->node_count());
}

TF_TEST(ser_rejects_malformed) {
  ASSERT_THROWS(deserialize_snapshot_json("{\"bogus\": true}"));
  ASSERT_THROWS(deserialize_snapshot_json("not json at all"));
  ASSERT_THROWS(deserialize_snapshot_json("[]"));
  ASSERT_THROWS(deserialize_snapshot_json(""));
}

TF_TEST(ser_rejects_oversized) {
  // Build an over-large synthetic JSON with too many nodes vs a tiny bound.
  // Use a very small max_nodes bound so the imported doc is rejected as oversized.
  Bounds b; b.max_nodes = 2;
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"},
        {NodeType::ACCELERATOR, "g0", "g0"},
        {NodeType::ACCELERATOR, "g1", "g1"},
        {NodeType::ACCELERATOR, "g2", "g2"} },
      { tf_test_util::edge("m", "g0", EdgeType::CONTAINS),
        tf_test_util::edge("m", "g1", EdgeType::CONTAINS),
        tf_test_util::edge("m", "g2", EdgeType::CONTAINS) });
  auto json = serialize_snapshot_json(*snap);
  ASSERT_THROWS(deserialize_snapshot_json(json, b));
}

TF_TEST(ser_property_roundtrip) {
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"}, {NodeType::ACCELERATOR, "g", "g"} },
      { tf_test_util::edge("m", "g", EdgeType::CONNECTED_TO) });
  // mutate a property and re-serialize
  SnapshotBuilder b;
  SnapshotMetadata meta = snap->metadata(); meta.synthetic = true;
  b.set_metadata(meta);
  for (auto& [id, n] : snap->nodes()) { TopologyNode nn = n; nn.properties.emplace("k", PropertyValue("v")); b.add_node(std::move(nn)); }
  for (auto& e : snap->edges()) b.add_edge(e);
  auto snap2 = b.take();
  auto json = serialize_snapshot_json(*snap2);
  auto back = deserialize_snapshot_json(json);
  bool found = false;
  for (auto& [id, n] : back->nodes()) { auto it = n.properties.find("k"); if (it != n.properties.end() && it->second.as_string()=="v") found = true; }
  ASSERT(found);
}