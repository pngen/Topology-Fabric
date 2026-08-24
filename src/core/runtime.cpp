#include "topology_fabric/runtime.hpp"
#include "topology_fabric/merge.hpp"
#include "topology_fabric/serialization.hpp"
#include "topology_fabric/result.hpp"
#include "providers/provider_factories.hpp"
#include <chrono>
#include <random>
#include <sstream>
#include <algorithm>
#include <unordered_set>

namespace topology_fabric {
namespace {
int64_t now_ms() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}
std::string uuid_hex() {
  std::random_device rd;
  uint64_t a = (static_cast<uint64_t>(rd()) << 32) ^ rd();
  uint64_t b = (static_cast<uint64_t>(rd()) << 32) ^ rd();
  return TopologyNodeId(a, b).to_hex();
}
}  // namespace

TopologyRuntime::TopologyRuntime(Bounds bounds) : bounds_(std::move(bounds)) {}

void TopologyRuntime::register_builtin_providers() {
  registry_.register_provider(create_host_provider());
  registry_.register_provider(create_cpu_numa_provider());
  registry_.register_provider(create_pci_provider());
#ifdef TOPOLOGY_FABRIC_HAS_CUDA
  registry_.register_provider(create_cuda_provider());
#endif
  registry_.register_provider(create_storage_provider());
  registry_.register_provider(create_network_provider());
}

std::shared_ptr<const TopologySnapshot> TopologyRuntime::discover() {
  std::lock_guard<std::mutex> lk(discover_mu_);
  const auto t_start = std::chrono::steady_clock::now();
  telemetry_.record_discovery_run();

  DiscoveryContext ctx;
  ctx.bounds = bounds_;
  ctx.started_ms = now_ms();

  std::vector<Contribution> contributions;
  std::vector<std::string> allWarnings;
  bool anyPartial = false;
  std::ostringstream provVersions;

  for (auto& provider : registry_.all()) {
    const auto t0 = std::chrono::steady_clock::now();
    Contribution cont;
        try {
      cont = provider->discover(ctx);
    } catch (const std::exception& e) {
      cont.success = false;
      cont.partial = true;
      cont.warnings.push_back(std::string("provider threw: ") + e.what());
    }
    const auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    telemetry_.record_provider_run(provider->name(), cont.success, ms);
    telemetry_.add_provider_time(ms);
    if (cont.partial) { anyPartial = true; telemetry_.record_partial_discovery(); }
    contributions.push_back(std::move(cont));
  }

  MergedGraph merged = merge_contributions(contributions, bounds_);

  // Connect rootless resource nodes into the machine containment hierarchy so the
  // graph is connected and path/locality reasoning works. Deterministic and safe:
  // a resource with no CONTAINS/ATTACHED_TO parent is placed under the machine.
  {
    TopologyNodeId machineId = kNullNodeId;
    for (const auto& [id, n] : merged.nodes) { if (n.type == NodeType::MACHINE) { machineId = id; break; } }
    if (machineId != kNullNodeId) {
      std::unordered_set<TopologyNodeId> hasParent;
      for (const auto& e : merged.edges)
        if (e.type == EdgeType::CONTAINS || e.type == EdgeType::ATTACHED_TO) hasParent.insert(e.target);
      for (const auto& [id, n] : merged.nodes) {
        if (id == machineId) continue;
        bool resource = n.type == NodeType::ACCELERATOR || n.type == NodeType::STORAGE_DEVICE ||
                        n.type == NodeType::NETWORK_INTERFACE || n.type == NodeType::PCI_ROOT ||
                        n.type == NodeType::PCI_DEVICE;
        if (resource && !hasParent.count(id)) {
          TopologyEdge e;
          e.source = machineId; e.target = id; e.type = EdgeType::CONTAINS; e.direction = EdgeDirection::DIRECTED;
          e.provenance = Provenance::inferred("merge", "machine attach", Confidence::MEDIUM, "resource placed under machine");
          e.confidence = Confidence::MEDIUM;
          merged.edges.push_back(std::move(e));
        }
      }
    }
  }



  // Machine identity.
  std::string machineIdentity;
  for (const auto& [id, n] : merged.nodes) {
    if (n.type == NodeType::MACHINE) { machineIdentity = n.name; break; }
  }
  if (machineIdentity.empty()) machineIdentity = "unknown-machine";
  std::string providerVersions;
  for (auto& c : contributions) { if (!providerVersions.empty()) providerVersions += ";"; providerVersions += c.provider + "=" + c.version; }

  telemetry_.record_merge_conflicts(merged.conflicts.size());

  // Build provisional snapshot for generation decision.
  auto build_snapshot = [&](uint64_t generation, bool synthetic) -> std::shared_ptr<const TopologySnapshot> {
    SnapshotBuilder b(bounds_);
    SnapshotMetadata meta;
    meta.snapshot_id = "snap-" + std::to_string(++snapshot_counter_) + "-" + uuid_hex();
    meta.generation = generation;
    meta.created_ms = now_ms();
    meta.machine_identity = machineIdentity;
    meta.provider_versions = providerVersions;
    meta.partial_discovery = anyPartial;
    meta.synthetic = synthetic;
    meta.warnings = allWarnings;
    for (const auto& [id, n] : merged.nodes) b.add_node(n);
    for (auto& e : merged.edges) b.add_edge(e);
    b.set_metadata(std::move(meta));
    return b.take();
  };

  auto prev = current_.load();
  uint64_t gen;
  if (!prev) gen = 1;
  else {
    auto provisional = build_snapshot(prev->metadata().generation, false);
    auto d = compare_snapshots(*prev, *provisional);
    gen = d.material_change ? prev->metadata().generation + 1 : prev->metadata().generation;
    if (d.material_change) telemetry_.record_generation_change();
  }

  auto snap = build_snapshot(gen, false);

  telemetry_.record_nodes(snap->node_count());
  telemetry_.record_edges(snap->edge_count());
  std::unordered_map<NodeType, uint64_t> byType;
  for (const auto& [id, n] : snap->nodes()) ++byType[n.type];
  for (auto& [t, c] : byType) telemetry_.record_node_type(t, c);
  telemetry_.record_snapshot_created();
  if (!snap->validation().ok) telemetry_.record_validation_failures(snap->validation().errors.size());

  const auto t_end = std::chrono::steady_clock::now();
  double disc_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();
  telemetry_.add_discovery_time(disc_ms);

  current_.store(snap);
  return snap;
}

uint64_t TopologyRuntime::generation() const noexcept {
  auto s = current_.load();
  return s ? s->metadata().generation : 0;
}

TopologyDiff TopologyRuntime::diff(const TopologySnapshot& before, const TopologySnapshot& after) const {
  return compare_snapshots(before, after);
}

std::string TopologyRuntime::serialize(const TopologySnapshot& snap, bool pretty) const {
  return serialize_snapshot_json(snap, pretty);
}

std::shared_ptr<const TopologySnapshot> TopologyRuntime::deserialize(const std::string& json) const {
  try {
    auto snap = deserialize_snapshot_json(json, bounds_);
    telemetry_.record_imported_snapshot(true);
    return snap;
  } catch (...) {
    telemetry_.record_imported_snapshot(false);
    throw;
  }
}

std::shared_ptr<const TopologySnapshot> TopologyRuntime::import_synthetic(const std::string& json) const {
  try {
    auto snap = deserialize_snapshot_json(json, bounds_);
    if (!snap->metadata().synthetic) {
      // Force the synthetic flag by rebuilding the immutable snapshot.
      SnapshotBuilder b(bounds_);
      SnapshotMetadata meta = snap->metadata();
      meta.synthetic = true;
      b.set_metadata(std::move(meta));
      for (const auto& [id, n] : snap->nodes()) b.add_node(n);
      for (auto& e : snap->edges()) b.add_edge(e);
      snap = b.take();
    }
    telemetry_.record_imported_snapshot(true);
    return snap;
  } catch (...) {
    telemetry_.record_imported_snapshot(false);
    throw;
  }
}

CostWeights TopologyRuntime::default_cost_weights() { return CostWeights{}; }

}  // namespace topology_fabric