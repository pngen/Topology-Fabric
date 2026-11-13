#include "test_harness.hpp"
#include "tf_test_util.hpp"

using namespace topology_fabric;

TF_TEST(path_inaccessible_edge_is_not_a_route) {
  // a-b and a-d are recorded as not accessible. The only usable route from a
  // to b is a-c-b, and d cannot be reached at all.
  auto make = [](const char* name, NodeType type) {
    TopologyNode n; n.id = derive_node_id("t", "n", name); n.type = type; n.name = name; return n;
  };
  auto link = [](const TopologyNode& s, const TopologyNode& t, bool accessible) {
    TopologyEdge e; e.source = s.id; e.target = t.id; e.type = EdgeType::CONNECTED_TO;
    e.direction = EdgeDirection::UNDIRECTED; e.confidence = Confidence::HIGH; e.accessible = accessible;
    return e;
  };
  const TopologyNode a = make("a", NodeType::ACCELERATOR), b = make("b", NodeType::ACCELERATOR);
  const TopologyNode c = make("c", NodeType::PCI_BRIDGE), d = make("d", NodeType::ACCELERATOR);
  SnapshotBuilder builder;
  builder.add_node(a); builder.add_node(b); builder.add_node(c); builder.add_node(d);
  builder.add_edge(link(a, b, false));
  builder.add_edge(link(a, c, true));
  builder.add_edge(link(c, b, true));
  builder.add_edge(link(a, d, false));
  auto snap = builder.take();
  auto hops = shortest_path(*snap, a.id, b.id);
  ASSERT(hops.found);
  ASSERT(hops.hop_count == 2);
  auto cheapest = lowest_cost_path(*snap, a.id, b.id, CostWeights{});
  ASSERT(cheapest.found);
  ASSERT(cheapest.hop_count == 2);
  ASSERT(!shortest_path(*snap, a.id, d.id).found);
  ASSERT(!lowest_cost_path(*snap, a.id, d.id, CostWeights{}).found);
}

TF_TEST(path_same_object) {
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"}, {NodeType::ACCELERATOR, "g", "g"} },
      { tf_test_util::edge("m", "g", EdgeType::CONNECTED_TO) });
  TopologyNodeId g;
  for (auto& [id, n] : snap->nodes()) if (n.name=="g") g=id;
  auto p = shortest_path(*snap, g, g);
  ASSERT(p.found);
  ASSERT(p.hop_count == 0);
  ASSERT(p.path_class == PathClass::SAME_OBJECT);
}

TF_TEST(path_direct) {
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"}, {NodeType::ACCELERATOR, "g", "g"} },
      { tf_test_util::edge("m", "g", EdgeType::CONNECTED_TO, EdgeDirection::UNDIRECTED) });
  TopologyNodeId m, g;
  for (auto& [id, n] : snap->nodes()) { if (n.name=="m") m=id; else if (n.name=="g") g=id; }
  auto p = shortest_path(*snap, m, g);
  ASSERT(p.found);
  ASSERT(p.hop_count == 1);
}

TF_TEST(path_no_route) {
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"},
        {NodeType::ACCELERATOR, "g", "g"},
        {NodeType::STORAGE_DEVICE, "s", "s"} },
      { tf_test_util::edge("m", "g", EdgeType::CONNECTED_TO) });
  TopologyNodeId g, s;
  for (auto& [id, n] : snap->nodes()) { if (n.name=="g") g=id; else if (n.name=="s") s=id; }
  auto p = shortest_path(*snap, g, s);
  ASSERT(!p.found);
}

TF_TEST(path_lowest_cost_prefers_cheap) {
  // Direct m->g is cheap/fast; the detour m->h->g is slower. Lowest-cost path picks direct.
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"},
        {NodeType::PCI_BRIDGE, "h", "h"},
        {NodeType::ACCELERATOR, "g", "g"} },
      { tf_test_util::edge("m", "g", EdgeType::CONNECTED_TO, EdgeDirection::UNDIRECTED, 64e9, 10.0),
        tf_test_util::edge("m", "h", EdgeType::CONNECTED_TO, EdgeDirection::UNDIRECTED, 8e9, 100.0),
        tf_test_util::edge("h", "g", EdgeType::CONNECTED_TO, EdgeDirection::UNDIRECTED, 8e9, 100.0) });
  TopologyNodeId m, g;
  for (auto& [id, n] : snap->nodes()) { if (n.name=="m") m=id; else if (n.name=="g") g=id; }
  auto p = lowest_cost_path(*snap, m, g, CostWeights{});
  ASSERT(p.found);
  ASSERT(p.hop_count == 1);
}

TF_TEST(path_invalid_endpoint) {
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"}, {NodeType::ACCELERATOR, "g", "g"} }, {});
  TopologyNodeId g;
  for (auto& [id, n] : snap->nodes()) if (n.name=="g") g=id;
  TopologyNodeId bogus(99, 99);
  auto p = shortest_path(*snap, bogus, g);
  ASSERT(!p.found);
}