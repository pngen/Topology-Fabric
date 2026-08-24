#include "test_harness.hpp"
#include "tf_test_util.hpp"

using namespace topology_fabric;

TF_TEST(snapshot_build_and_validate_ok) {
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"}, {NodeType::ACCELERATOR, "g", "g"} },
      { tf_test_util::edge("m", "g", EdgeType::CONTAINS) });
  ASSERT(snap->validation().ok);
  ASSERT(snap->node_count() == 2);
  ASSERT(snap->metadata().generation == 1);
}

TF_TEST(snapshot_immutable_publication) {
  // A snapshot is immutable; repeated reads give stable content.
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"}, {NodeType::ACCELERATOR, "g", "g"} }, {});
  size_t c1 = snap->node_count(), c2 = snap->node_count();
  ASSERT(c1 == c2 && c1 == 2);
}

TF_TEST(snapshot_lookup_missing_throws) {
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"} }, {});
  TopologyNodeId bogus(123, 456);
  ASSERT_THROWS(snap->node(bogus));
  ASSERT(snap->find_node(bogus) == nullptr);
}

TF_TEST(snapshot_edge_out_of_range_throws) {
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"} }, {});
  ASSERT_THROWS(snap->edge(999));
}

TF_TEST(snapshot_bounds_reject) {
  Bounds b; b.max_nodes = 1;
  SnapshotBuilder sb(b);
  TopologyNode a; a.id = derive_node_id("t","n","a"); a.type = NodeType::MACHINE; a.name="a";
  TopologyNode c; c.id = derive_node_id("t","n","c"); c.type = NodeType::CPU_THREAD; c.name="c";
  sb.add_node(a);
  ASSERT(!sb.add_node(c));  // exceeds max_nodes -> rejected with warning
  ASSERT(sb.node_count() == 1);
}