#include "test_harness.hpp"
#include "tf_test_util.hpp"
#include "topology_fabric/serialization.hpp"
#include "topology_fabric/result.hpp"

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

TF_TEST(ser_roundtrip_is_exact) {
  // What the writer emits reads back as the same values: fractional and very
  // small doubles, whole-number doubles (still doubles), unsigned values above
  // 2^63, and text outside ASCII.
  TopologyNode a; a.id = derive_node_id("t", "n", "a"); a.type = NodeType::MACHINE;
  a.name = "h\xC3\xB4te \xE2\x84\xA2";
  a.properties.emplace("ratio", PropertyValue(1.0 / 3.0));
  a.properties.emplace("tiny", PropertyValue(1.25e-9));
  a.properties.emplace("whole", PropertyValue(64.0));
  a.properties.emplace("big", PropertyValue(static_cast<uint64_t>(0xFFFFFFFFFFFFFFF0ull)));
  TopologyNode g; g.id = derive_node_id("t", "n", "g"); g.type = NodeType::ACCELERATOR; g.name = "g";
  TopologyEdge e; e.source = a.id; e.target = g.id; e.type = EdgeType::CONNECTED_TO;
  e.bandwidth_bytes_per_sec = 12345678901.25;
  e.latency_ns = 123.4567891234;
  e.locality_score = 1.0 / 3.0;
  e.peer_capability = 0x8000000000000001ull;
  SnapshotBuilder b;
  SnapshotMetadata meta; meta.generation = 1; meta.machine_identity = "m"; meta.synthetic = true;
  b.set_metadata(meta);
  b.add_node(a); b.add_node(g); b.add_edge(e);
  auto snap = b.take();
  auto back = deserialize_snapshot_json(serialize_snapshot_json(*snap));
  auto* na = back->find_node(a.id);
  ASSERT(na != nullptr);
  if (na) {
    ASSERT(na->name == a.name);
    ASSERT(na->properties == a.properties);
  }
  ASSERT(back->edge_count() == 1);
  if (back->edge_count() == 1) {
    const auto& be = back->edge(0);
    ASSERT(be.bandwidth_bytes_per_sec == e.bandwidth_bytes_per_sec);
    ASSERT(be.latency_ns == e.latency_ns);
    ASSERT(be.locality_score == e.locality_score);
    ASSERT(be.peer_capability == e.peer_capability);
  }
  // A snapshot and its round trip are the same topology.
  auto d = compare_snapshots(*snap, *back);
  ASSERT(!d.material_change);
  ASSERT(d.events.empty());
}

TF_TEST(ser_size_bounds_are_enforced_first) {
  // An input over the byte bound is refused for its size, before it is parsed.
  Bounds small; small.max_snapshot_bytes = 16;
  bool oversized = false;
  try { deserialize_snapshot_json(std::string(64, '['), small); }
  catch (const TopologyError& err) { oversized = err.code() == ErrorCode::OVERSIZED; }
  ASSERT(oversized);
  // The snapshot's per-string bound holds for imported documents.
  auto snap = tf_test_util::build_snapshot({ {NodeType::MACHINE, std::string(256, 'x'), "m"} }, {});
  auto json = serialize_snapshot_json(*snap);
  Bounds strings; strings.max_string_bytes = 64;
  bool refused = false;
  try { deserialize_snapshot_json(json, strings); }
  catch (const TopologyError& err) { refused = err.code() == ErrorCode::OVERSIZED; }
  ASSERT(refused);
  ASSERT(deserialize_snapshot_json(json) != nullptr);
}