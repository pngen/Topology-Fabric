
// Internal (non-installed) shared helpers for path/cost/ranking computation.
#pragma once
#include "topology_fabric/snapshot.hpp"
#include "topology_fabric/query.hpp"
#include <cmath>

namespace topology_fabric {
namespace detail {

enum class HardwareClass { CPU, ACCELERATOR, PCI, MEMORY, STORAGE, NETWORK, MACHINE, OTHER };

inline HardwareClass hardware_class(NodeType t) noexcept {
  switch (t) {
    case NodeType::CPU_PACKAGE:
    case NodeType::CPU_CORE:
    case NodeType::CPU_THREAD:
      return HardwareClass::CPU;
    case NodeType::ACCELERATOR:
    case NodeType::ACCELERATOR_MEMORY_DOMAIN:
      return HardwareClass::ACCELERATOR;
    case NodeType::PCI_ROOT:
    case NodeType::PCI_BRIDGE:
    case NodeType::PCI_DEVICE:
      return HardwareClass::PCI;
    case NodeType::NUMA_NODE:
    case NodeType::HOST_MEMORY_DOMAIN:
    case NodeType::SHARED_MEMORY_DOMAIN:
      return HardwareClass::MEMORY;
    case NodeType::STORAGE_DEVICE:
      return HardwareClass::STORAGE;
    case NodeType::NETWORK_INTERFACE:
      return HardwareClass::NETWORK;
    case NodeType::MACHINE:
      return HardwareClass::MACHINE;
    case NodeType::UNKNOWN:
    case NodeType::EXTENSION:
      return HardwareClass::OTHER;
  }
  return HardwareClass::OTHER;
}

inline double default_latency_ns(const TopologyNode& /*u*/, const TopologyNode& v) noexcept {
  // Conservative defaults by device class; never presented as factual.
  switch (v.type) {
    case NodeType::ACCELERATOR: return 300.0;
    case NodeType::CPU_THREAD:  return 60.0;
    case NodeType::PCI_BRIDGE:  return 120.0;
    case NodeType::PCI_ROOT:    return 250.0;
    case NodeType::STORAGE_DEVICE: return 80.0;
    case NodeType::NETWORK_INTERFACE: return 500.0;
    default: return 100.0;
  }
}

inline double default_bandwidth_bps(const TopologyNode&, const TopologyNode& v) noexcept {
  switch (v.type) {
    case NodeType::ACCELERATOR: return 32e9;
    case NodeType::CPU_THREAD:  return 32e9;
    case NodeType::PCI_BRIDGE:  return 16e9;
    case NodeType::PCI_ROOT:    return 16e9;
    case NodeType::STORAGE_DEVICE: return 3e9;
    case NodeType::NETWORK_INTERFACE: return 10e9;
    default: return 8e9;
  }
}

inline double edge_traversal_cost(const TopologyNode& u, const TopologyNode& v,
                                  const TopologyEdge& e, const CostWeights& w) noexcept {
  double cost = w.hop_penalty;

  double lat = e.latency_ns.value_or(default_latency_ns(u, v));
  cost += w.latency_ns_weight * lat;

  double bw = e.bandwidth_bytes_per_sec.value_or(default_bandwidth_bps(u, v));
  if (bw <= 0.0) bw = w.bandwidth_ref_bps;
  cost += w.bandwidth_scale * (w.bandwidth_ref_bps / bw);

  if (v.type == NodeType::PCI_BRIDGE) cost += w.pci_bridge_penalty;
  if (v.type == NodeType::PCI_ROOT) cost += w.root_crossing_penalty;

  if (hardware_class(u.type) != hardware_class(v.type)) cost += w.node_transition_penalty;

  if (e.confidence == Confidence::LOW || e.confidence == Confidence::UNKNOWN)
    cost += w.uncertainty_penalty;

  cost += w.policy_penalty;
  // Guard against NaN/inf from degenerate weights/metrics so the priority queue and
  // cost comparisons remain well-defined. A huge finite penalty is deterministic.
  if (std::isnan(cost) || std::isinf(cost)) cost = 1e15;
  return cost;
}

inline bool collocation_edge(EdgeType t) noexcept {
  return t == EdgeType::SHARES_PARENT || t == EdgeType::SHARES_NUMA;
}

}  // namespace detail
}  // namespace topology_fabric