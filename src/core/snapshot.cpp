
#include "topology_fabric/snapshot.hpp"
#include "topology_fabric/result.hpp"
#include <stdexcept>
#include <unordered_map>
#include <algorithm>

namespace topology_fabric {

const TopologyNode& TopologySnapshot::node(TopologyNodeId id) const {
  auto it = nodes_.find(id);
  if (it == nodes_.end())
    throw TopologyError(ErrorCode::NOT_FOUND, std::string("unknown node id ") + id.to_hex());
  return it->second;
}

const TopologyNode* TopologySnapshot::find_node(TopologyNodeId id) const noexcept {
  auto it = nodes_.find(id);
  return it == nodes_.end() ? nullptr : &it->second;
}

const TopologyEdge& TopologySnapshot::edge(size_t i) const {
  if (i >= edges_.size())
    throw TopologyError(ErrorCode::NOT_FOUND, "edge index out of range");
  return edges_[i];
}

const std::vector<EdgeRef>& TopologySnapshot::adjacency(TopologyNodeId id) const noexcept {
  static const std::vector<EdgeRef> kEmpty;
  auto it = adjacency_.find(id);
  return it == adjacency_.end() ? kEmpty : it->second;
}


ValidationResult SnapshotBuilder::validate(const std::unordered_map<TopologyNodeId, TopologyNode>& nodes,
                                           const std::vector<TopologyEdge>& edges) {
  ValidationResult v;
  // 1. Dangling edges.
  for (const auto& e : edges) {
    if (nodes.find(e.source) == nodes.end())
      v.add_error("edge references unknown source node " + e.source.to_hex());
    if (nodes.find(e.target) == nodes.end())
      v.add_error("edge references unknown target node " + e.target.to_hex());
  }
  // 2. Self-containment and duplicate native identity.
  std::unordered_map<std::string, TopologyNodeId> seen_key;
  for (const auto& [id, n] : nodes) {
    if (n.type == NodeType::UNKNOWN && n.category.empty())
      v.add_warning("node " + id.to_hex() + " has unknown type and no extension category");
    std::string key = n.native.canonical_key();
    if (!key.empty()) {
      auto it = seen_key.find(key);
      if (it != seen_key.end() && it->second != id) {
        v.add_error("duplicate native identity '" + key + "' for nodes " +
                    it->second.to_hex() + " and " + id.to_hex());
      } else if (it == seen_key.end()) {
        seen_key.emplace(key, id);
      }
    }
  }
  for (const auto& e : edges) {
    if (e.type == EdgeType::CONTAINS && e.source == e.target)
      v.add_error("self-containment edge on node " + e.source.to_hex());
  }
  // 3. Containment cycles (CONTAINS is a strict parent->child DAG).
  std::unordered_map<TopologyNodeId, std::vector<TopologyNodeId>> child_of;
  for (const auto& e : edges) {
    if (e.type == EdgeType::CONTAINS)
      child_of[e.source].push_back(e.target);
  }
  auto has_cycle = [&]() -> bool {
    // Kahn's topological sort over CONTAINS edges.
    std::unordered_map<TopologyNodeId, int> indeg;
    for (auto& [k, v] : child_of) indeg[k] += 0;
    for (auto& [k, kids] : child_of)
      for (auto& kid : kids) indeg[kid] += 1;
    std::vector<TopologyNodeId> stack;
    for (auto& [k, d] : indeg) if (d == 0) stack.push_back(k);
    size_t visited = 0;
    while (!stack.empty()) {
      auto cur = stack.back(); stack.pop_back();
      visited++;
      for (auto& kid : child_of[cur]) {
        if (--indeg[kid] == 0) stack.push_back(kid);
      }
    }
    return visited != indeg.size();
  };
  if (has_cycle())
    v.add_error("containment cycle detected among CONTAINS edges");

  // 4. Symmetric required edges (PEER_TO symmetric where directed+required).
  std::unordered_map<TopologyEdge::Key, bool> peer_present;
  for (const auto& e : edges) {
    if (e.type == EdgeType::PEER_TO) peer_present[e.key()] = true;
  }
  for (const auto& e : edges) {
    if (e.type == EdgeType::PEER_TO && e.direction == EdgeDirection::DIRECTED) {
      TopologyEdge::Key rev{e.target, e.source, EdgeType::PEER_TO};
      if (!peer_present.count(rev))
        v.add_warning("PEER_TO edge " + e.source.to_hex() + "->" + e.target.to_hex() +
                      " is directed without a symmetric counterpart");
    }
  }

  // 5. Accelerator PCI binding consistency (soft, warning).
  for (const auto& [id, n] : nodes) {
    if (n.type == NodeType::ACCELERATOR && n.native.has_pci()) {
      auto it = seen_key.find(n.native.canonical_key());
      if (it == seen_key.end())
        v.add_warning("accelerator " + id.to_hex() + " claims PCI identity but no matching node exists");
    }
  }
  return v;
}

bool SnapshotBuilder::add_node(TopologyNode node) {
  if (taken_) return false;
  if (nodes_.size() >= bounds_.max_nodes) { validation_.add_warning("max_nodes exceeded; node dropped"); return false; }
  auto [it, inserted] = nodes_.emplace(node.id, std::move(node));
  if (!inserted) {
    validation_.add_warning("duplicate node id " + it->first.to_hex() + "; keeping first");
    return false;
  }
  return true;
}

bool SnapshotBuilder::add_edge(TopologyEdge edge) {
  if (taken_) return false;
  if (edges_.size() >= bounds_.max_edges) { validation_.add_warning("max_edges exceeded; edge dropped"); return false; }
  edges_.push_back(std::move(edge));
  return true;
}

void SnapshotBuilder::set_metadata(SnapshotMetadata meta) { metadata_ = std::move(meta); }
void SnapshotBuilder::set_validation(ValidationResult val) { validation_ = std::move(val); }

std::shared_ptr<const TopologySnapshot> SnapshotBuilder::take() {
  if (taken_) throw TopologyError(ErrorCode::INTERNAL, "SnapshotBuilder::take() called twice");
  taken_ = true;
  // Validate (starts from independently-forced warnings).
  auto v = validate(nodes_, edges_);
  // Merge with explicitly set validation results (warnings/errors).
  for (auto& e : validation_.errors) v.add_error(std::move(e));
  for (auto& w : validation_.warnings) v.add_warning(std::move(w));

  auto snap = std::make_shared<TopologySnapshot>();
  snap->nodes_ = std::move(nodes_);
  snap->edges_ = std::move(edges_);
  snap->metadata_ = std::move(metadata_);
  snap->validation_ = std::move(v);
  snap->bounds_ = bounds_;
  // Build adjacency (undirected traversal over non-collocation edges and
  // bidirectional over all others; collocation edges included for locality).
  for (size_t i = 0; i < snap->edges_.size(); ++i) {
    const auto& e = snap->edges_[i];
    snap->adjacency_[e.source].push_back(EdgeRef{e.target, i});
    snap->adjacency_[e.target].push_back(EdgeRef{e.source, i});
  }
  snap->adjacency_built_ = true;
  return snap;
}

}  // namespace topology_fabric