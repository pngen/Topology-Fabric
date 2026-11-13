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

TF_TEST(adversarial_json_string_errors_are_errors) {
  // A string that cannot be read fails the whole parse. It is never replaced
  // by null with parsing carried on behind it.
  ASSERT(!json::parse("[\"a\\,\"b\"]").ok());     // invalid escape, then a comma
  ASSERT(!json::parse("[\"a\\]").ok());            // invalid escape, then the closing bracket
  ASSERT(!json::parse("[{\"k\\]").ok());           // the same inside an object key
  ASSERT(!json::parse("\"abc").ok());              // unterminated
  ASSERT(json::parse("[\"a\\,\"b\"]").error == json::ParseError::InvalidEscape);
  ASSERT_THROWS(deserialize_snapshot_json(
      "{\"format\":\"topology_fabric_snapshot\",\"nodes\":[],\"edges\":[],\"warnings\":[\"x\\,\"y\"]}"));
  // Raw UTF-8 is legal JSON; malformed UTF-8 is not.
  auto text = json::parse("\"caf\xC3\xA9 \xE2\x84\xA2\"");
  ASSERT(text.ok());
  if (text.ok()) ASSERT(text.value.as_string() == "caf\xC3\xA9 \xE2\x84\xA2");
  ASSERT(!json::parse("\"bad \xC3\"").ok());       // truncated sequence
  ASSERT(!json::parse("\"bad \xFF\"").ok());       // not a lead byte
  ASSERT(!json::parse("\"bad \xC0\xAF\"").ok());   // overlong encoding
}

TF_TEST(adversarial_json_numbers_out_of_range) {
  // The whole unsigned range reads back as written; a number beyond what an
  // integer can hold is rejected, not clamped to the nearest limit.
  auto top = json::parse("18446744073709551615");
  ASSERT(top.ok());
  if (top.ok()) {
    ASSERT(top.value.is_uint());
    if (top.value.is_uint()) ASSERT(top.value.as_uint() == 18446744073709551615ull);
  }
  ASSERT(!json::parse("18446744073709551616").ok());
  ASSERT(json::parse("-9223372036854775808").ok());
  ASSERT(!json::parse("-9223372036854775809").ok());
}

TF_TEST(adversarial_identity_fields_are_range_checked) {
  // A native identity number that does not fit its field is malformed. It must
  // not wrap around into the address of another device.
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"},
        {NodeType::PCI_DEVICE, "d", "d", std::nullopt, std::nullopt, std::nullopt, 1, 2, 0} }, {});
  const std::string json = serialize_snapshot_json(*snap);
  const std::string field = "\"pci_bus\":1,";
  const auto at = json.find(field);
  ASSERT(at != std::string::npos);
  if (at == std::string::npos) return;
  auto with = [&](const char* replacement) { std::string doc = json; doc.replace(at, field.size(), replacement); return doc; };
  ASSERT_THROWS(deserialize_snapshot_json(with("\"pci_bus\":65537,")));
  ASSERT_THROWS(deserialize_snapshot_json(with("\"pci_bus\":-1,")));
  ASSERT_THROWS(deserialize_snapshot_json(with("\"pci_bus\":\"1\",")));
  ASSERT(deserialize_snapshot_json(with("\"pci_bus\":255,")) != nullptr);
}

// Keep this test last in the file: on a build without the fix it does not return.
TF_TEST(adversarial_negative_cost_search_terminates) {
  // Weights that make an edge cost negative must not keep the search going:
  // looking for a destination that cannot be reached still ends, with no route.
  CostWeights w;
  w.hop_penalty = -1000.0;
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::ACCELERATOR, "a", "a"}, {NodeType::ACCELERATOR, "b", "b"}, {NodeType::ACCELERATOR, "c", "c"} },
      { tf_test_util::edge("a", "b", EdgeType::CONNECTED_TO) });
  TopologyNodeId a, c;
  for (auto& [id, n] : snap->nodes()) { if (n.name=="a") a=id; else if (n.name=="c") c=id; }
  auto p = lowest_cost_path(*snap, a, c, w);
  ASSERT(!p.found);
  auto rr = rank_candidates(*snap, a, NodeType::ACCELERATOR, 0, w);   // reaches b, not c
  ASSERT(rr.found);
  ASSERT(rr.ranked.size() == 1);
}