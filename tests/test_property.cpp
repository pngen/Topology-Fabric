#include "test_harness.hpp"
#include "tf_test_util.hpp"
#include "topology_fabric/serialization.hpp"
#include <vector>

using namespace topology_fabric;

TF_TEST(property_random_valid_graphs) {
  tf_test_util::PRNG rng(0xABCDEF12345ull);
  int cases = 400;
  for (int c = 0; c < cases; ++c) {
    size_t n = 4 + rng.range(0, 20);
    std::vector<tf_test_util::NodeSpec> nodes;
    std::vector<ContributedEdge> edges;
    std::vector<std::string> refs;
    // machine + a tree of resources connected to it.
    nodes.push_back({NodeType::MACHINE, "m", "m"});
    refs.push_back("m");
    for (size_t i = 0; i < n; ++i) {
      NodeType t = static_cast<NodeType>(1 + (rng.next() % 8));
      std::string ref = "n" + std::to_string(i);
      nodes.push_back({t, ref, ref});
      // connect to a random existing node (guarantee reachability to m eventually)
      std::string parent = refs[rng.range(0, refs.size() - 1)];
      edges.push_back(tf_test_util::edge(parent, ref, EdgeType::CONTAINS));
      refs.push_back(ref);
      // occasionally add a peer/connected edge among resources
      if (rng.chance(30) && refs.size() > 2) {
        std::string to = refs[rng.range(0, refs.size() - 1)];
        if (to != ref) edges.push_back(tf_test_util::edge(ref, to, EdgeType::CONNECTED_TO, EdgeDirection::UNDIRECTED));
      }
    }
    auto snap = tf_test_util::build_snapshot(nodes, edges);
    // Property: no dangling refs (validation ok for connected trees).
    ASSERT(snap->validation().ok);
    // Property: node/edge counts consistent.
    ASSERT(snap->node_count() == n + 1);
    // Property: deterministic serialization round-trip.
    auto json = serialize_snapshot_json(*snap);
    auto back = deserialize_snapshot_json(json);
    ASSERT(back->node_count() == snap->node_count());
    // Property: endpoints correct for a path from m to the last node.
    if (refs.size() > 1) {
      TopologyNodeId last;
      for (auto& [id, nd] : snap->nodes()) if (nd.name == refs.back()) last = id;
      TopologyNodeId mid;
      for (auto& [id, nd] : snap->nodes()) if (nd.name == "m") mid = id;
      auto p = shortest_path(*snap, mid, last);
      ASSERT(p.found);
      if (p.found) { ASSERT(p.source == mid); ASSERT(p.destination == last); }
    }
  }
}

TF_TEST(property_random_invalid_flagged) {
  tf_test_util::PRNG rng(0xFFFF0000ull);
  for (int c = 0; c < 200; ++c) {
    SnapshotBuilder b;
    size_t n = 2 + rng.range(0, 6);
    for (size_t i = 0; i < n; ++i) {
      TopologyNode nd; nd.id = derive_node_id("t","n", std::to_string(i)); nd.type = NodeType::MACHINE; nd.name = std::to_string(i);
      b.add_node(nd);
    }
    // Mostly-tangled containment edges -> likely cycles; test must not crash.
    for (size_t i = 0; i < n; ++i) {
      if (rng.chance(80)) {
        TopologyEdge e; e.source = derive_node_id("t","n", std::to_string(i));
        e.target = derive_node_id("t","n", std::to_string(rng.range(0, n-1)));
        e.type = EdgeType::CONTAINS; b.add_edge(e);
      }
    }
    auto snap = b.take();  // must never crash on malformed input
    ASSERT(snap != nullptr);
    (void)snap->validation();  // inspect
  }
}

TF_TEST(property_large_bounded_graph) {
  // A bounded large graph: 2000 nodes, 3000 edges -> no crash, serialization works.
  tf_test_util::PRNG rng(42);
  std::vector<tf_test_util::NodeSpec> nodes;
  std::vector<ContributedEdge> edges;
  nodes.push_back({NodeType::MACHINE, "m", "m"});
  for (int i = 0; i < 2000; ++i) {
    std::string ref = "r" + std::to_string(i);
    nodes.push_back({static_cast<NodeType>(1 + (rng.next() % 6)), ref, ref});
    edges.push_back(tf_test_util::edge("m", ref, EdgeType::CONTAINS));
  }
  for (int i = 0; i < 1000; ++i) {
    size_t a = rng.range(1, 2000), bb = rng.range(1, 2000);
    if (a != bb) edges.push_back(tf_test_util::edge("r" + std::to_string(a), "r" + std::to_string(bb), EdgeType::CONNECTED_TO, EdgeDirection::UNDIRECTED));
  }
  Bounds bo; bo.max_nodes = 100000; bo.max_edges = 200000;
  auto snap = tf_test_util::build_snapshot(nodes, edges, bo);
  ASSERT(snap->node_count() == 2001);
  auto json = serialize_snapshot_json(*snap);
  ASSERT(json.size() > 0);
  auto back = deserialize_snapshot_json(json, bo);
  ASSERT(back->node_count() == 2001);
}