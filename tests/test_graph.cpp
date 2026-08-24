#include "test_harness.hpp"
#include "tf_test_util.hpp"

using namespace topology_fabric;

// graph.cpp traversal helpers.

TF_TEST(graph_parents_children) {
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"},
        {NodeType::CPU_PACKAGE, "pkg", "pkg"},
        {NodeType::CPU_CORE, "core", "core"},
        {NodeType::CPU_THREAD, "t0", "t0"} },
      { tf_test_util::edge("m", "pkg", EdgeType::CONTAINS),
        tf_test_util::edge("pkg", "core", EdgeType::CONTAINS),
        tf_test_util::edge("core", "t0", EdgeType::CONTAINS) });
  TopologyNodeId m, pkg, core, t0;
  for (auto& [id, n] : snap->nodes()) {
    if (n.name == "m") m = id; else if (n.name == "pkg") pkg = id;
    else if (n.name == "core") core = id; else if (n.name == "t0") t0 = id;
  }
  ASSERT(!m.is_null());
  auto kids = children(*snap, m, EdgeType::CONTAINS);
  ASSERT(kids.size() == 1 && kids[0] == pkg);
  auto ps = parents(*snap, t0, EdgeType::CONTAINS);
  ASSERT(ps.size() == 1 && ps[0] == core);
}

TF_TEST(graph_ancestors_descendants) {
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"},
        {NodeType::CPU_PACKAGE, "pkg", "pkg"},
        {NodeType::CPU_CORE, "core", "core"},
        {NodeType::CPU_THREAD, "t0", "t0"} },
      { tf_test_util::edge("m", "pkg", EdgeType::CONTAINS),
        tf_test_util::edge("pkg", "core", EdgeType::CONTAINS),
        tf_test_util::edge("core", "t0", EdgeType::CONTAINS) });
  TopologyNodeId m, t0;
  for (auto& [id, n] : snap->nodes()) { if (n.name=="m") m=id; else if (n.name=="t0") t0=id; }
  auto anc = ancestors(*snap, t0, EdgeType::CONTAINS);
  ASSERT(anc.size() == 3);  // core, pkg, m
  auto desc = descendants(*snap, m, EdgeType::CONTAINS);
  ASSERT(desc.size() == 3);
}

TF_TEST(graph_siblings) {
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"},
        {NodeType::PCI_ROOT, "r", "r"},
        {NodeType::PCI_DEVICE, "a", "a"},
        {NodeType::PCI_DEVICE, "b", "b"} },
      { tf_test_util::edge("m", "r", EdgeType::CONTAINS),
        tf_test_util::edge("r", "a", EdgeType::CONTAINS),
        tf_test_util::edge("r", "b", EdgeType::CONTAINS) });
  TopologyNodeId a, b;
  for (auto& [id, n] : snap->nodes()) { if (n.name=="a") a=id; else if (n.name=="b") b=id; }
  auto sib = siblings(*snap, a, EdgeType::CONTAINS);
  ASSERT(sib.size() == 1 && sib[0] == b);
}

TF_TEST(graph_root_ancestor) {
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"},
        {NodeType::PCI_ROOT, "r", "r"},
        {NodeType::PCI_BRIDGE, "b", "b"},
        {NodeType::ACCELERATOR, "g", "g"} },
      { tf_test_util::edge("m", "r", EdgeType::CONTAINS),
        tf_test_util::edge("r", "b", EdgeType::CONTAINS),
        tf_test_util::edge("b", "g", EdgeType::CONTAINS) });
  TopologyNodeId g;
  for (auto& [id, n] : snap->nodes()) if (n.name=="g") g=id;
  auto root = root_ancestor(*snap, g, NodeType::PCI_ROOT);
  ASSERT(!root.is_null());
  auto mach = root_ancestor(*snap, g, NodeType::MACHINE);
  ASSERT(!mach.is_null());
}

TF_TEST(graph_neighbors) {
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"},
        {NodeType::ACCELERATOR, "g", "g"} },
      { tf_test_util::edge("m", "g", EdgeType::CONNECTED_TO, EdgeDirection::UNDIRECTED) });
  TopologyNodeId m;
  for (auto& [id, n] : snap->nodes()) if (n.name=="m") m=id;
  auto nb = neighbors(*snap, m);
  ASSERT(nb.size() == 1);
}