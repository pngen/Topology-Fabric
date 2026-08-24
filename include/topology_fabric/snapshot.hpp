
// TopologyFabric/snapshot.hpp - immutable, queryable topology snapshot.
#pragma once
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>
#include "topology_fabric/types.hpp"
#include "topology_fabric/node.hpp"
#include "topology_fabric/edge.hpp"
#include "topology_fabric/node_id.hpp"

namespace topology_fabric {

// Result of validating a snapshot against graph invariants.
struct ValidationResult {
  bool ok = true;
  std::vector<std::string> errors;
  std::vector<std::string> warnings;
  void add_error(std::string e) { ok = false; errors.push_back(std::move(e)); }
  void add_warning(std::string w) { warnings.push_back(std::move(w)); }
};

// Snapshot-level metadata.
struct SnapshotMetadata {
  std::string snapshot_id;
  uint64_t generation = 0;
  int64_t created_ms = 0;
  std::string machine_identity;
  std::string schema_version = "1.0";
  std::string provider_versions;
  bool partial_discovery = false;
  bool synthetic = false;
  std::vector<std::string> warnings;
};

// A directed adjacency record.
struct EdgeRef {
  TopologyNodeId neighbor;
  size_t edge_index;
};

// Resource bounds applied at construction/serialization.
struct Bounds {
  size_t max_nodes = 1'000'000;
  size_t max_edges = 4'000'000;
  size_t max_string_bytes = 1u << 22;   // 4 MiB per string
  size_t max_snapshot_bytes = 512u << 20;  // 512 MiB serialized
  size_t max_path_length = 16'384;
};

// Immutable topology snapshot. All reads are thread-safe on a const snapshot.
class TopologySnapshot {
 public:
  TopologySnapshot() = default;

  // Lookup / iteration.
  const TopologyNode& node(TopologyNodeId id) const;
  const TopologyNode* find_node(TopologyNodeId id) const noexcept;
  const TopologyEdge& edge(size_t i) const;
  const std::vector<TopologyEdge>& edges() const noexcept { return edges_; }
  const std::unordered_map<TopologyNodeId, TopologyNode>& nodes() const noexcept { return nodes_; }
  const std::vector<EdgeRef>& adjacency(TopologyNodeId id) const noexcept;

  size_t node_count() const noexcept { return nodes_.size(); }
  size_t edge_count() const noexcept { return edges_.size(); }

  const SnapshotMetadata& metadata() const noexcept { return metadata_; }
  const ValidationResult& validation() const noexcept { return validation_; }
  const Bounds& bounds() const noexcept { return bounds_; }

 private:
  friend class SnapshotBuilder;
  friend class Runtime;

  std::unordered_map<TopologyNodeId, TopologyNode> nodes_;
  std::vector<TopologyEdge> edges_;
  std::unordered_map<TopologyNodeId, std::vector<EdgeRef>> adjacency_;
  SnapshotMetadata metadata_;
  ValidationResult validation_;
  Bounds bounds_;
  bool adjacency_built_ = false;
};

// Builds and validates an immutable snapshot. After take(), the builder is spent.
class SnapshotBuilder {
 public:
  explicit SnapshotBuilder(Bounds bounds = {}) : bounds_(std::move(bounds)) {}
  ~SnapshotBuilder() = default;

  // Add a node/edge. Returns false and records a warning on bound violation.
  bool add_node(TopologyNode node);
  bool add_edge(TopologyEdge edge);

  void set_metadata(SnapshotMetadata meta);
  void set_validation(ValidationResult val);
  void set_bounds(Bounds b) { bounds_ = std::move(b); }

  // Build (validates invariants, builds adjacency) and return the immutable snapshot.
  std::shared_ptr<const TopologySnapshot> take();

  size_t node_count() const noexcept { return nodes_.size(); }
  size_t edge_count() const noexcept { return edges_.size(); }

  // Validation-only routine used by snapshot validation and by tests.
  static ValidationResult validate(const std::unordered_map<TopologyNodeId, TopologyNode>& nodes,
                                   const std::vector<TopologyEdge>& edges);

 private:
  Bounds bounds_;
  std::unordered_map<TopologyNodeId, TopologyNode> nodes_;
  std::vector<TopologyEdge> edges_;
  SnapshotMetadata metadata_;
  ValidationResult validation_;
  bool taken_ = false;
};

}  // namespace topology_fabric
