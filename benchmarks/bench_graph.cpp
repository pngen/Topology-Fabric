// benchmarks/bench_graph.cpp - synthetic graph scaling benchmarks.
#include "topology_fabric/runtime.hpp"
#include "topology_fabric/snapshot.hpp"
#include "topology_fabric/serialization.hpp"
#include "topology_fabric/query.hpp"
#include "topology_fabric/diff.hpp"
#include <chrono>
#include <cstdio>
#include <vector>
#include <string>

using namespace topology_fabric;
namespace {
double ms(std::chrono::steady_clock::time_point a, std::chrono::steady_clock::time_point b) {
  return std::chrono::duration<double, std::milli>(b - a).count();
}
std::shared_ptr<const TopologySnapshot> make_graph(size_t n) {
  Bounds b; b.max_nodes = 100000; b.max_edges = 500000;
  SnapshotBuilder sb(b);
  SnapshotMetadata meta; meta.synthetic = true; meta.generation = 1; sb.set_metadata(meta);
  std::vector<TopologyNodeId> ids;
  auto mk = [&](const std::string& ref, NodeType t, const std::string& name) {
    TopologyNode nd; nd.id = derive_node_id("bench","n", ref); nd.type = t; nd.name = name;
    nd.provenance = Provenance::user_supplied(Confidence::HIGH, "bench"); nd.synthetic = true; return nd;
  };
  auto machine = mk("m", NodeType::MACHINE, "machine"); sb.add_node(machine); ids.push_back(machine.id);
  for (size_t i = 0; i < n; ++i) {
    auto r = mk("r" + std::to_string(i), static_cast<NodeType>(1 + (i % 6)), "r" + std::to_string(i));
    sb.add_node(r); ids.push_back(r.id);
    TopologyEdge e; e.source = machine.id; e.target = r.id; e.type = EdgeType::CONTAINS;
    e.provenance = Provenance::user_supplied(Confidence::HIGH, "bench"); sb.add_edge(e);
  }
  return sb.take();
}
}  // namespace

int main() {
  for (size_t n : { (size_t)100, (size_t)1000, (size_t)10000 }) {
    std::printf("=== graph size %zu nodes ===\n", n);
    auto snap = make_graph(n);
    std::vector<TopologyNodeId> ids;
    for (auto& [id, x] : snap->nodes()) ids.push_back(id);
    // node lookup
    {
      auto t0 = std::chrono::steady_clock::now();
      size_t sum = 0;
      for (int k = 0; k < 10000; ++k) { auto it = snap->nodes().find(ids[k % ids.size()]); if (it != snap->nodes().end()) ++sum; }
      auto t1 = std::chrono::steady_clock::now();
      std::printf("  node_lookup  %.3f ms  (%zu lookups)\n", ms(t0,t1), (size_t)10000); (void)sum;
    }
    // path query
    {
      auto t0 = std::chrono::steady_clock::now();
      size_t cnt = 0;
      for (int k = 0; k < 2000; ++k) { auto p = lowest_cost_path(*snap, ids[0], ids[k % ids.size()], CostWeights{}); if (p.found) ++cnt; }
      auto t1 = std::chrono::steady_clock::now();
      std::printf("  path_query   %.3f ms  (%zu paths)\n", ms(t0,t1), cnt);
    }
    // ranking
    {
      auto t0 = std::chrono::steady_clock::now();
      auto rr = rank_candidates(*snap, ids[0], NodeType::ACCELERATOR, 20, CostWeights{});
      auto t1 = std::chrono::steady_clock::now();
      std::printf("  rank_20      %.3f ms  (%zu candidates)\n", ms(t0,t1), rr.ranked.size());
    }
    // serialization
    {
      auto t0 = std::chrono::steady_clock::now();
      std::string js = serialize_snapshot_json(*snap);
      auto t1 = std::chrono::steady_clock::now();
      auto back = deserialize_snapshot_json(js);
      auto t2 = std::chrono::steady_clock::now();
      std::printf("  serialize    %.3f ms   deserialize %.3f ms  (%zu bytes)\n", ms(t0,t1), ms(t1,t2), js.size());
      (void)back;
    }
    // validation + diff
    {
      auto b = make_graph(n);
      auto t0 = std::chrono::steady_clock::now();
      auto d = compare_snapshots(*snap, *b);
      auto t1 = std::chrono::steady_clock::now();
      std::printf("  diff         %.3f ms  (%zu events)\n", ms(t0,t1), d.events.size());
      auto t2 = std::chrono::steady_clock::now();
      volatile bool ok = b->validation().ok;
      auto t3 = std::chrono::steady_clock::now();
      std::printf("  validate     %.3f ms  (%s)\n", ms(t2,t3), ok?"ok":"fail");
    }
  }
  return 0;
}