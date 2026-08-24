
#include "topology_fabric/query.hpp"
#include "internal.hpp"
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <limits>
#include <cmath>

namespace topology_fabric {
namespace {

bool traverse_edge(const TopologySnapshot& snap, const EdgeRef& ref, bool in_collocation) {
  const auto& e = snap.edge(ref.edge_index);
  if (detail::collocation_edge(e.type)) return in_collocation;
  return true;
}

void fill_path(const TopologySnapshot& snap, TopologyPath& p, const CostWeights& w) {
  // Bottleneck bandwidth / summed latency.
  double bw = std::numeric_limits<double>::max();
  double lat = 0.0;
  bool any_bw = false;
  for (const auto& seg : p.segments) {
    const TopologyNode& un = snap.node(seg.from);
    const TopologyNode& vn = snap.node(seg.to);
    const TopologyEdge& e = snap.edge(seg.edge_index);
    double eb = e.bandwidth_bytes_per_sec.value_or(detail::default_bandwidth_bps(un, vn));
    double el = e.latency_ns.value_or(detail::default_latency_ns(un, vn));
    if (eb <= 0.0) eb = w.bandwidth_ref_bps;
    bw = std::min(bw, eb);
    lat += el;
    any_bw = true;
  }
  if (!any_bw) bw = w.bandwidth_ref_bps;
  p.estimated_bandwidth_bps = bw;
  p.estimated_latency_ns = lat;
  p.hop_count = static_cast<int>(p.segments.size());
  p.locality = locality_between(snap, p.source, p.destination);
  p.path_class = classify_path(snap, p.source, p.destination);
  p.cost_policy_version = w.version;
  p.confidence = Confidence::MEDIUM;  // default; refined in cost_breakdown
}

}  // namespace

TopologyPath shortest_path(const TopologySnapshot& snap, TopologyNodeId from,
                           TopologyNodeId to, bool in_collocation) {
  TopologyPath p;
  p.source = from;
  p.destination = to;
  if (!snap.find_node(from) || !snap.find_node(to)) {
    p.reasons.push_back("endpoint node not present in snapshot");
    return p;
  }
  if (from == to) {
    p.found = true;
    p.hop_count = 0;
    p.locality = LocalityClass::EXACT;
    p.path_class = PathClass::SAME_OBJECT;
    return p;
  }
  std::unordered_map<TopologyNodeId, TopologyNodeId> parent;
  std::unordered_map<TopologyNodeId, size_t> came_edge;
  std::queue<TopologyNodeId> q;
  std::unordered_set<TopologyNodeId> visited;
  q.push(from);
  visited.insert(from);
  bool found = false;
  while (!q.empty()) {
    auto cur = q.front(); q.pop();
    if (cur == to) { found = true; break; }
    for (const auto& ref : snap.adjacency(cur)) {
      if (!traverse_edge(snap, ref, in_collocation)) continue;
      auto nxt = ref.neighbor;
      if (visited.insert(nxt).second) {
        visited.insert(nxt);
        parent[nxt] = cur;
        came_edge[nxt] = ref.edge_index;
        q.push(nxt);
      }
    }
  }
  if (!found) {
    p.reasons.push_back("no connected topology path between endpoints");
    return p;
  }
  // Reconstruct.
  std::vector<PathSegment> segs;
  TopologyNodeId cur = to;
  while (cur != from) {
    auto prev = parent[cur];
    auto ei = came_edge[cur];
    segs.push_back(PathSegment{prev, cur, ei, snap.edge(ei).type, 0.0});
    cur = prev;
  }
  std::reverse(segs.begin(), segs.end());
  p.segments = std::move(segs);
  p.found = true;
  fill_path(snap, p, CostWeights{});
  return p;
}

TopologyPath lowest_cost_path(const TopologySnapshot& snap, TopologyNodeId from,
                              TopologyNodeId to, const CostWeights& weights) {
  TopologyPath p;
  p.source = from;
  p.destination = to;
  if (!snap.find_node(from) || !snap.find_node(to)) {
    p.reasons.push_back("endpoint node not present in snapshot");
    return p;
  }
  if (from == to) {
    p.found = true;
    p.hop_count = 0;
    p.locality = LocalityClass::EXACT;
    p.path_class = PathClass::SAME_OBJECT;
    p.cost_policy_version = weights.version;
    return p;
  }
  using QE = std::pair<double, TopologyNodeId>;
  std::priority_queue<QE, std::vector<QE>, std::greater<>> pq;
  std::unordered_map<TopologyNodeId, double> dist;
  std::unordered_map<TopologyNodeId, TopologyNodeId> parent;
  std::unordered_map<TopologyNodeId, size_t> came_edge;
  pq.emplace(0.0, from);
  dist[from] = 0.0;
  bool found = false;
  while (!pq.empty()) {
    auto [d, u] = pq.top(); pq.pop();
    if (u == to) { found = true; break; }
    if (d > dist[u]) continue;
    for (const auto& ref : snap.adjacency(u)) {
      if (!traverse_edge(snap, ref, false)) continue;
      auto v = ref.neighbor;
      const auto& e = snap.edge(ref.edge_index);
      const TopologyNode& un = snap.node(u);
      const TopologyNode& vn = snap.node(v);
      double nd = d + detail::edge_traversal_cost(un, vn, e, weights);
      auto it = dist.find(v);
      if (it == dist.end() || nd < it->second) {
        dist[v] = nd;
        parent[v] = u;
        came_edge[v] = ref.edge_index;
        pq.emplace(nd, v);
      }
    }
  }
  if (!found) {
    p.reasons.push_back("no connected topology path between endpoints");
    return p;
  }
  std::vector<PathSegment> segs;
  TopologyNodeId cur = to;
  double total = 0.0;
  while (cur != from) {
    auto prev = parent[cur];
    auto ei = came_edge[cur];
    const auto& e = snap.edge(ei);
    double ec = detail::edge_traversal_cost(snap.node(prev), snap.node(cur), e, weights);
    total += ec;
    segs.push_back(PathSegment{prev, cur, ei, e.type, ec});
    cur = prev;
  }
  std::reverse(segs.begin(), segs.end());
  p.segments = std::move(segs);
  p.found = true;
  p.total_cost = total;
  // Path-level NUMA penalty: crossing between different NUMA nodes is an endpoint
  // property, not an artifact of any NUMA-less intermediate node.
  {
    const TopologyNode& src = snap.node(from);
    const TopologyNode& dst = snap.node(to);
    if (src.native.numa_node && dst.native.numa_node && *src.native.numa_node != *dst.native.numa_node)
      p.total_cost += weights.numa_penalty;
  }
  fill_path(snap, p, weights);
  return p;
}

}  // namespace topology_fabric