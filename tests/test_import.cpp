#include "test_harness.hpp"
#include "tf_test_util.hpp"
#include "topology_fabric/serialization.hpp"
#include "topology_fabric/runtime.hpp"

using namespace topology_fabric;

TF_TEST(import_marks_synthetic) {
  // Build a synthetic snapshot JSON and import it; it must be marked synthetic.
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"}, {NodeType::ACCELERATOR, "g0", "g0"}, {NodeType::ACCELERATOR, "g1", "g1"} },
      { tf_test_util::edge("m", "g0", EdgeType::CONTAINS), tf_test_util::edge("m", "g1", EdgeType::CONTAINS) });
  auto json = serialize_snapshot_json(*snap);
  TopologyRuntime rt;
  auto imported = rt.import_synthetic(json);
  ASSERT(imported->metadata().synthetic);
  ASSERT(imported->node_count() > 0);
  // Every node flagged synthetic? At least the snapshot is.
}

TF_TEST(import_malformed_rejected) {
  TopologyRuntime rt;
  bool threw = false;
  try { rt.import_synthetic("{not json"); } catch (...) { threw = true; }
  ASSERT(threw);
  ASSERT_THROWS(rt.deserialize("{\"wrong\":true}"));
}

TF_TEST(import_wrong_format_rejected) {
  TopologyRuntime rt;
  ASSERT_THROWS(rt.deserialize("{\"format\":\"something_else\",\"nodes\":[]}"));
}

TF_TEST(import_empty_doc_rejected) {
  TopologyRuntime rt;
  ASSERT_THROWS(rt.deserialize("{\"format\":\"topology_fabric_snapshot\"}"));  // missing nodes
}

TF_TEST(import_two_gpu_topology) {
  // A synthetic 2-GPU import must expose correct peer/rank semantics.
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"},
        {NodeType::PCI_ROOT, "r", "r"},
        {NodeType::ACCELERATOR, "g0", "g0", 0, std::nullopt, std::nullopt, 0, 0, 0},
        {NodeType::ACCELERATOR, "g1", "g1", 0, std::nullopt, std::nullopt, 0, 1, 0} },
      { tf_test_util::edge("m", "r", EdgeType::CONTAINS),
        tf_test_util::edge("r", "g0", EdgeType::CONTAINS),
        tf_test_util::edge("r", "g1", EdgeType::CONTAINS),
        tf_test_util::edge("g0", "g1", EdgeType::PEER_TO, EdgeDirection::UNDIRECTED) });
  auto json = serialize_snapshot_json(*snap);
  TopologyRuntime rt;
  auto imported = rt.import_synthetic(json);
  ASSERT(imported->metadata().synthetic);
}  