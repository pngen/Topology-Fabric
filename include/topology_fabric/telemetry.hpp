
// TopologyFabric/telemetry.hpp - runtime counters and statistics.
#pragma once
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include "topology_fabric/types.hpp"

namespace topology_fabric {

// Runtime telemetry: counters for discovery, merge, validation, snapshots,
// queries, and measurements. Thread-safe; JSON-serializable.
class Telemetry {
 public:
  struct Snapshot {
    uint64_t discovery_runs = 0;
    uint64_t nodes_total = 0;
    uint64_t edges_total = 0;
    uint64_t provider_successes = 0;
    uint64_t provider_failures = 0;
    uint64_t partial_discoveries = 0;
    uint64_t merge_conflicts = 0;
    uint64_t validation_failures = 0;
    uint64_t snapshots_created = 0;
    uint64_t generation_changes = 0;
    uint64_t path_queries = 0;
    uint64_t ranking_queries = 0;
    uint64_t measurement_runs = 0;
    uint64_t imported_snapshots = 0;
    uint64_t malformed_imports_rejected = 0;
    double discovery_total_ms = 0.0;
    double query_total_ms = 0.0;
    double provider_total_ms = 0.0;
    std::unordered_map<NodeType, uint64_t> nodes_by_type;
    std::unordered_map<std::string, uint64_t> provider_runs;
  };

  void record_discovery_run() noexcept;
  void record_nodes(uint64_t count) noexcept;
  void record_edges(uint64_t count) noexcept;
  void record_node_type(NodeType t, uint64_t count) noexcept;
  void record_provider_run(const std::string& provider, bool success, double ms) noexcept;
  void record_partial_discovery() noexcept;
  void record_merge_conflicts(uint64_t n) noexcept;
  void record_validation_failures(uint64_t n) noexcept;
  void record_snapshot_created() noexcept;
  void record_generation_change() noexcept;
  void record_path_query() noexcept;
  void record_ranking_query() noexcept;
  void record_measurement_run() noexcept;
  void record_imported_snapshot(bool ok) noexcept;
  void add_discovery_time(double ms) noexcept;
  void add_query_time(double ms) noexcept;
  void add_provider_time(double ms) noexcept;

  Snapshot snapshot() const;
  void reset() noexcept;

 private:
  mutable std::mutex mu_;
  Snapshot s_;
};

}  // namespace topology_fabric
