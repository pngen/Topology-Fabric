#include "test_harness.hpp"
#include "tf_test_util.hpp"
#include "topology_fabric/diff.hpp"

using namespace topology_fabric;

TF_TEST(diff_node_added_removed) {
  auto a = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"}, {NodeType::ACCELERATOR, "g0", "g0"} },
      { tf_test_util::edge("m", "g0", EdgeType::CONTAINS) });
  auto b = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"},
        {NodeType::ACCELERATOR, "g0", "g0"}, {NodeType::ACCELERATOR, "g1", "g1"} },
      { tf_test_util::edge("m", "g0", EdgeType::CONTAINS), tf_test_util::edge("m", "g1", EdgeType::CONTAINS) });
  auto d = compare_snapshots(*a, *b);
  ASSERT(d.material_change);
  bool added = false;
  for (auto& e : d.events) if (e.kind == DiffEventKind::NODE_ADDED) added = true;
  ASSERT(added);
  auto d2 = compare_snapshots(*b, *a);
  bool removed = false;
  for (auto& e : d2.events) if (e.kind == DiffEventKind::NODE_REMOVED) removed = true;
  ASSERT(removed);
}

TF_TEST(diff_edge_added_removed) {
  auto a = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"}, {NodeType::ACCELERATOR, "g", "g"} }, {});
  auto b = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"}, {NodeType::ACCELERATOR, "g", "g"} },
      { tf_test_util::edge("m", "g", EdgeType::CONNECTED_TO) });
  auto d = compare_snapshots(*a, *b);
  ASSERT(d.material_change);
  bool edgeAdded = false;
  for (auto& e : d.events) if (e.kind == DiffEventKind::EDGE_ADDED) edgeAdded = true;
  ASSERT(edgeAdded);
}

TF_TEST(diff_property_change_not_material) {
  // Node property change only => not material.
  SnapshotBuilder ba; SnapshotMetadata ma; ma.generation=1; ba.set_metadata(ma);
  TopologyNode a; a.id = derive_node_id("t","n","x"); a.type = NodeType::MACHINE; a.name="x"; a.properties.emplace("v", PropertyValue(1));
  ba.add_node(a); auto snapA = ba.take();

  SnapshotBuilder bb; SnapshotMetadata mb; mb.generation=2; bb.set_metadata(mb);
  TopologyNode b; b.id = derive_node_id("t","n","x"); b.type = NodeType::MACHINE; b.name="x"; b.properties.emplace("v", PropertyValue(999));
  bb.add_node(b); auto snapB = bb.take();

  auto d = compare_snapshots(*snapA, *snapB);
  ASSERT(!d.material_change);  // node property change is telemetry
}

TF_TEST(diff_edge_property_material) {
  // Edge link-property change (bandwidth) IS material.
  auto a = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"}, {NodeType::ACCELERATOR, "g", "g"} },
      { tf_test_util::edge("m", "g", EdgeType::CONNECTED_TO, EdgeDirection::UNDIRECTED, 8e9, 100.0) });
  auto b = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"}, {NodeType::ACCELERATOR, "g", "g"} },
      { tf_test_util::edge("m", "g", EdgeType::CONNECTED_TO, EdgeDirection::UNDIRECTED, 64e9, 20.0) });
  auto d = compare_snapshots(*a, *b);
  ASSERT(d.material_change);
}