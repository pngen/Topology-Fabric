
#include "topology_fabric/query.hpp"
#include <unordered_set>
#include <deque>
#include <algorithm>

namespace topology_fabric {

static bool edge_type_matches(EdgeType want, EdgeType got) {
  return want == got;
}

std::vector<TopologyNodeId> parents(const TopologySnapshot& snap, TopologyNodeId id, EdgeType type) {
  std::vector<TopologyNodeId> out;
  for (const auto& e : snap.edges())
    if (edge_type_matches(type, e.type) && e.target == id)
      out.push_back(e.source);
  return out;
}

std::vector<TopologyNodeId> children(const TopologySnapshot& snap, TopologyNodeId id, EdgeType type) {
  std::vector<TopologyNodeId> out;
  for (const auto& e : snap.edges())
    if (edge_type_matches(type, e.type) && e.source == id)
      out.push_back(e.target);
  return out;
}

std::vector<TopologyNodeId> ancestors(const TopologySnapshot& snap, TopologyNodeId id, EdgeType type) {
  std::vector<TopologyNodeId> out;
  std::unordered_set<TopologyNodeId> visited;
  std::deque<TopologyNodeId> queue{id};
  while (!queue.empty()) {
    auto cur = queue.front(); queue.pop_front();
    for (auto p : parents(snap, cur, type)) {
      if (visited.insert(p).second) { out.push_back(p); queue.push_back(p); }
    }
  }
  return out;
}

std::vector<TopologyNodeId> descendants(const TopologySnapshot& snap, TopologyNodeId id, EdgeType type) {
  std::vector<TopologyNodeId> out;
  std::unordered_set<TopologyNodeId> visited;
  std::deque<TopologyNodeId> queue{id};
  while (!queue.empty()) {
    auto cur = queue.front(); queue.pop_front();
    for (auto c : children(snap, cur, type)) {
      if (visited.insert(c).second) { out.push_back(c); queue.push_back(c); }
    }
  }
  return out;
}

std::vector<TopologyNodeId> siblings(const TopologySnapshot& snap, TopologyNodeId id, EdgeType type) {
  std::vector<TopologyNodeId> out;
  std::unordered_set<TopologyNodeId> seen;
  for (auto p : parents(snap, id, type)) {
    for (auto c : children(snap, p, type)) {
      if (c != id && seen.insert(c).second) out.push_back(c);
    }
  }
  return out;
}

std::vector<TopologyNodeId> neighbors(const TopologySnapshot& snap, TopologyNodeId id) {
  std::vector<TopologyNodeId> out;
  for (const auto& ref : snap.adjacency(id)) out.push_back(ref.neighbor);
  return out;
}

TopologyNodeId root_ancestor(const TopologySnapshot& snap, TopologyNodeId id, NodeType root_type) {
  TopologyNodeId cur = id;
  std::unordered_set<TopologyNodeId> seen;
  while (cur != kNullNodeId && snap.find_node(cur)) {
    if (snap.node(cur).type == root_type) return cur;
    if (!seen.insert(cur).second) break;  // cycle guard
    auto ps = parents(snap, cur, EdgeType::CONTAINS);
    if (ps.empty()) {
      // Fall back to any CONTAINS parent (e.g., machine holds cpu package).
      break;
    }
    cur = ps.front();
  }
  return kNullNodeId;
}

}  // namespace topology_fabric
