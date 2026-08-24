
#include "topology_fabric/telemetry.hpp"

namespace topology_fabric {

void Telemetry::record_discovery_run() noexcept { std::lock_guard<std::mutex> lk(mu_); ++s_.discovery_runs; }
void Telemetry::record_nodes(uint64_t count) noexcept { std::lock_guard<std::mutex> lk(mu_); s_.nodes_total = count; }
void Telemetry::record_edges(uint64_t count) noexcept { std::lock_guard<std::mutex> lk(mu_); s_.edges_total = count; }
void Telemetry::record_node_type(NodeType t, uint64_t count) noexcept { std::lock_guard<std::mutex> lk(mu_); s_.nodes_by_type[t] = count; }
void Telemetry::record_provider_run(const std::string& provider, bool success, double ms) noexcept {
  std::lock_guard<std::mutex> lk(mu_); ++s_.provider_runs[provider]; if (success) ++s_.provider_successes; else ++s_.provider_failures; s_.provider_total_ms += ms;
}
void Telemetry::record_partial_discovery() noexcept { std::lock_guard<std::mutex> lk(mu_); ++s_.partial_discoveries; }
void Telemetry::record_merge_conflicts(uint64_t n) noexcept { std::lock_guard<std::mutex> lk(mu_); s_.merge_conflicts += n; }
void Telemetry::record_validation_failures(uint64_t n) noexcept { std::lock_guard<std::mutex> lk(mu_); s_.validation_failures += n; }
void Telemetry::record_snapshot_created() noexcept { std::lock_guard<std::mutex> lk(mu_); ++s_.snapshots_created; }
void Telemetry::record_generation_change() noexcept { std::lock_guard<std::mutex> lk(mu_); ++s_.generation_changes; }
void Telemetry::record_path_query() noexcept { std::lock_guard<std::mutex> lk(mu_); ++s_.path_queries; }
void Telemetry::record_ranking_query() noexcept { std::lock_guard<std::mutex> lk(mu_); ++s_.ranking_queries; }
void Telemetry::record_measurement_run() noexcept { std::lock_guard<std::mutex> lk(mu_); ++s_.measurement_runs; }
void Telemetry::record_imported_snapshot(bool ok) noexcept {
  std::lock_guard<std::mutex> lk(mu_); if (ok) ++s_.imported_snapshots; else ++s_.malformed_imports_rejected;
}
void Telemetry::add_discovery_time(double ms) noexcept { std::lock_guard<std::mutex> lk(mu_); s_.discovery_total_ms += ms; }
void Telemetry::add_query_time(double ms) noexcept { std::lock_guard<std::mutex> lk(mu_); s_.query_total_ms += ms; }
void Telemetry::add_provider_time(double ms) noexcept { std::lock_guard<std::mutex> lk(mu_); s_.provider_total_ms += ms; }

Telemetry::Snapshot Telemetry::snapshot() const {
  std::lock_guard<std::mutex> lk(mu_);
  return s_;
}

void Telemetry::reset() noexcept { std::lock_guard<std::mutex> lk(mu_); s_ = Snapshot{}; }

}  // namespace topology_fabric
