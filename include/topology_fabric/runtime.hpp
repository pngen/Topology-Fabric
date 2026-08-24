
// TopologyFabric/runtime.hpp - the public Topology Fabric runtime facade.
#pragma once
#include <memory>
#include <atomic>
#include <mutex>
#include "topology_fabric/snapshot.hpp"
#include "topology_fabric/registry.hpp"
#include "topology_fabric/telemetry.hpp"
#include "topology_fabric/diff.hpp"
#include "topology_fabric/query.hpp"

namespace topology_fabric {

// Topology Fabric runtime: owns provider registration, discovery orchestration,
// merging, generation tracking, immutable-snapshot publishing, and serialization.
// Reads are lock-free (immutable snapshots); discovery serializes on a single lock.
class TopologyRuntime {
 public:
  explicit TopologyRuntime(Bounds bounds = {});
  ~TopologyRuntime() = default;

  TopologyRuntime(const TopologyRuntime&) = delete;
  TopologyRuntime& operator=(const TopologyRuntime&) = delete;

  ProviderRegistry& providers() noexcept { return registry_; }
  const ProviderRegistry& providers() const noexcept { return registry_; }
  const Bounds& bounds() const noexcept { return bounds_; }
  Telemetry& telemetry() noexcept { return telemetry_; }
  const Telemetry& telemetry() const noexcept { return telemetry_; }

  // Register the built-in discovery providers (host/cpu_numa/pci/cuda/storage/network).
  void register_builtin_providers();

  // Run discovery, merge provider contributions, and publish a new immutable snapshot.
  // Returns the newly published snapshot.
  std::shared_ptr<const TopologySnapshot> discover();
  std::shared_ptr<const TopologySnapshot> refresh() { return discover(); }

  // Currently published snapshot (nullptr until first discover).
  std::shared_ptr<const TopologySnapshot> current() const noexcept { return current_.load(); }
  uint64_t generation() const noexcept;

  TopologyDiff diff(const TopologySnapshot& before, const TopologySnapshot& after) const;

  std::string serialize(const TopologySnapshot& snap, bool pretty = false) const;
  std::shared_ptr<const TopologySnapshot> deserialize(const std::string& json) const;
  std::shared_ptr<const TopologySnapshot> import_synthetic(const std::string& json) const;

  static CostWeights default_cost_weights();

 private:
  Bounds bounds_;
  ProviderRegistry registry_;
  mutable Telemetry telemetry_;
  std::atomic<std::shared_ptr<const TopologySnapshot>> current_{};
  std::mutex discover_mu_;
  std::atomic<uint64_t> snapshot_counter_{0};
};

}  // namespace topology_fabric