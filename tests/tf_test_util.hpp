// tests/tf_test_util.hpp - helpers for building synthetic topologies and PRNG.
#pragma once
#include "topology_fabric/runtime.hpp"
#include "topology_fabric/snapshot.hpp"
#include "topology_fabric/merge.hpp"
#include "topology_fabric/serialization.hpp"
#include "topology_fabric/query.hpp"
#include "topology_fabric/node_id.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <random>

namespace tf_test_util {

// Deterministic PRNG (splitmix64) so property tests are reproducible.
class PRNG {
 public:
  explicit PRNG(uint64_t seed) : state_(seed) {}
  uint64_t next() {
    uint64_t z = (state_ += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
  }
  uint64_t range(uint64_t lo, uint64_t hi) { return lo + (next() % (hi - lo + 1)); }
  bool chance(uint64_t pct) { return (next() % 100) < pct; }
  const uint64_t& state() const { return state_; }
 private:
  uint64_t state_;
};

struct NodeSpec {
  topology_fabric::NodeType type;
  std::string name;
  std::string ref;
  std::optional<uint32_t> numa;
  std::optional<uint32_t> package;
  std::optional<uint32_t> core;
  std::optional<uint16_t> pci_bus;
  std::optional<uint16_t> pci_dev;
  std::optional<uint16_t> pci_func;
  topology_fabric::Capability caps = topology_fabric::Capability::NONE;
};

// Build a snapshot from node/edge specs.
inline std::shared_ptr<const topology_fabric::TopologySnapshot> build_snapshot(
    std::vector<NodeSpec> nodes,
    std::vector<topology_fabric::ContributedEdge> edges,
    const topology_fabric::Bounds& bounds = {}) {
  std::vector<topology_fabric::Contribution> contribs;
  topology_fabric::Contribution c;
  c.provider = "test"; c.version = "1.0";
  for (auto& ns : nodes) {
    topology_fabric::ContributedNode n;
    n.ref = ns.ref;
    n.type = ns.type;
    n.name = ns.name;
    n.native.numa_node = ns.numa;
    n.native.cpu_package = ns.package;
    n.native.core_id = ns.core;
    n.native.pci_bus = ns.pci_bus;
    n.native.pci_device = ns.pci_dev;
    n.native.pci_function = ns.pci_func;
    n.capabilities = ns.caps;
    n.provenance = topology_fabric::Provenance::user_supplied(topology_fabric::Confidence::HIGH, "test");
    c.nodes.push_back(std::move(n));
  }
  c.edges = std::move(edges);
  c.success = true;
  contribs.push_back(std::move(c));
  auto mg = topology_fabric::merge_contributions(contribs, bounds);
  topology_fabric::SnapshotBuilder b(bounds);
  topology_fabric::SnapshotMetadata meta;
  meta.generation = 1;
  meta.machine_identity = "test-machine";
  meta.synthetic = true;
  b.set_metadata(std::move(meta));
  for (const auto& [id, n] : mg.nodes) b.add_node(n);
  for (auto& e : mg.edges) b.add_edge(e);
  return b.take();
}

inline topology_fabric::ContributedEdge edge(const std::string& from, const std::string& to,
                                             topology_fabric::EdgeType type,
                                             topology_fabric::EdgeDirection dir = topology_fabric::EdgeDirection::DIRECTED,
                                             std::optional<double> bw = std::nullopt,
                                             std::optional<double> lat = std::nullopt) {
  topology_fabric::ContributedEdge e;
  e.from_ref = from; e.to_ref = to; e.type = type; e.direction = dir;
  e.provenance = topology_fabric::Provenance::user_supplied(topology_fabric::Confidence::HIGH, "test");
  e.confidence = topology_fabric::Confidence::HIGH;
  e.bandwidth_bytes_per_sec = bw;
  e.latency_ns = lat;
  return e;
}

inline std::shared_ptr<const topology_fabric::TopologySnapshot> import_snap(const std::string& json,
                                                                            const topology_fabric::Bounds& bounds = {}) {
  return topology_fabric::deserialize_snapshot_json(json, bounds);
}

// Helper to build a local discovery snapshot (real machine) or return nullptr on non-Windows.
std::shared_ptr<const topology_fabric::TopologySnapshot> discover_local();

inline std::shared_ptr<const topology_fabric::TopologySnapshot> discover_local() {
  topology_fabric::TopologyRuntime rt;
  rt.register_builtin_providers();
  return rt.discover();
}

}  // namespace tf_test_util