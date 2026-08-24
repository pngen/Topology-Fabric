
#include "topology_fabric/query.hpp"
#include "internal.hpp"
#include <algorithm>

namespace topology_fabric {
namespace {
bool is_host(NodeType t) noexcept {
  return t == NodeType::CPU_PACKAGE || t == NodeType::CPU_CORE ||
         t == NodeType::CPU_THREAD || t == NodeType::NUMA_NODE ||
         t == NodeType::HOST_MEMORY_DOMAIN || t == NodeType::MACHINE;
}
bool is_cpu(NodeType t) noexcept {
  return t == NodeType::CPU_PACKAGE || t == NodeType::CPU_CORE || t == NodeType::CPU_THREAD;
}
bool is_pci_endpoint(const TopologyNode& n) noexcept {
  return n.type == NodeType::PCI_DEVICE || n.type == NodeType::ACCELERATOR;
}
}  // namespace

LocalityClass locality_between(const TopologySnapshot& snap, TopologyNodeId a, TopologyNodeId b) {
  if (a == b) return LocalityClass::EXACT;
  const TopologyNode& na = snap.node(a);
  const TopologyNode& nb = snap.node(b);
  // same core
  if (na.type == NodeType::CPU_THREAD && nb.type == NodeType::CPU_THREAD &&
      na.native.core_id && nb.native.core_id && *na.native.core_id == *nb.native.core_id &&
      na.native.cpu_package && nb.native.cpu_package &&
      *na.native.cpu_package == *nb.native.cpu_package) {
    return LocalityClass::SAME_CORE;
  }
  // same package
  if (is_cpu(na.type) && is_cpu(nb.type) &&
      na.native.cpu_package && nb.native.cpu_package &&
      *na.native.cpu_package == *nb.native.cpu_package) {
    return LocalityClass::SAME_PACKAGE;
  }
  // same NUMA
  if (na.native.numa_node && nb.native.numa_node && *na.native.numa_node == *nb.native.numa_node)
    return LocalityClass::SAME_NUMA;
  // same root complex
  TopologyNodeId ra = root_ancestor(snap, a, NodeType::PCI_ROOT);
  TopologyNodeId rb = root_ancestor(snap, b, NodeType::PCI_ROOT);
  if (ra != kNullNodeId && rb != kNullNodeId && ra == rb) return LocalityClass::SAME_ROOT_COMPLEX;
  // same host machine
  TopologyNodeId ha = root_ancestor(snap, a, NodeType::MACHINE);
  TopologyNodeId hb = root_ancestor(snap, b, NodeType::MACHINE);
  if (ha != kNullNodeId && hb != kNullNodeId && ha == hb) return LocalityClass::SAME_HOST;
  if (ha == kNullNodeId && hb == kNullNodeId) return LocalityClass::UNKNOWN;
  return LocalityClass::REMOTE;
}

PathClass classify_path(const TopologySnapshot& snap, TopologyNodeId from, TopologyNodeId to) {
  if (from == to) return PathClass::SAME_OBJECT;
  const TopologyNode& na = snap.node(from);
  const TopologyNode& nb = snap.node(to);
  bool acc_a = na.type == NodeType::ACCELERATOR;
  bool acc_b = nb.type == NodeType::ACCELERATOR;
  bool host_a = is_host(na.type);
  bool host_b = is_host(nb.type);

  if (acc_a && acc_b) return PathClass::ACCELERATOR_TO_ACCELERATOR;
  if ((acc_a && host_b) || (host_a && acc_b)) return PathClass::HOST_TO_ACCELERATOR;
  if ((host_a && nb.type == NodeType::STORAGE_DEVICE) ||
      (host_b && na.type == NodeType::STORAGE_DEVICE)) return PathClass::HOST_TO_STORAGE;
  if ((host_a && nb.type == NodeType::NETWORK_INTERFACE) ||
      (host_b && na.type == NodeType::NETWORK_INTERFACE)) return PathClass::HOST_TO_NETWORK;
  if (is_cpu(na.type) && is_cpu(nb.type)) return PathClass::SAME_PROCESSOR;

  // Same PCI bridge: both endpoints under a common PCI bridge.
  if (is_pci_endpoint(na) && is_pci_endpoint(nb)) {
    auto pa = parents(snap, from, EdgeType::CONTAINS);
    auto pb = parents(snap, to, EdgeType::CONTAINS);
    for (auto x : pa) for (auto y : pb)
      if (x == y && snap.node(x).type == NodeType::PCI_BRIDGE) return PathClass::SAME_PCI_BRIDGE;
    TopologyNodeId ra = root_ancestor(snap, from, NodeType::PCI_ROOT);
    TopologyNodeId rb = root_ancestor(snap, to, NodeType::PCI_ROOT);
    if (ra != kNullNodeId && rb != kNullNodeId && ra == rb) return PathClass::SAME_ROOT_COMPLEX;
  }

  // NUMA relationship.
  if (na.native.numa_node && nb.native.numa_node) {
    if (*na.native.numa_node == *nb.native.numa_node) return PathClass::SAME_NUMA;
    return PathClass::CROSS_NUMA;
  }
  TopologyNodeId ha = root_ancestor(snap, from, NodeType::MACHINE);
  TopologyNodeId hb = root_ancestor(snap, to, NodeType::MACHINE);
  if (ha != kNullNodeId && hb != kNullNodeId && ha == hb) return PathClass::REMOTE;
  return PathClass::UNKNOWN;
}

DistanceBreakdown distance_between(const TopologySnapshot& snap, TopologyNodeId from,
                                   TopologyNodeId to, const CostWeights& weights) {
  DistanceBreakdown d;
  if (from == to) { d.graph_hops = 0; d.normalized_score = 0.0; return d; }
  auto sp = shortest_path(snap, from, to, false);
  if (!sp.found) {
    d.graph_hops = -1;
    d.normalized_score = 1.0;
    d.uncertainty = 1.0;
    return d;
  }
  d.graph_hops = sp.hop_count;
  // PCI depth: number of PCI nodes along the path.
  for (auto& seg : sp.segments) {
    auto& n = snap.node(seg.to);
    if (n.type == NodeType::PCI_BRIDGE || n.type == NodeType::PCI_ROOT) ++d.pci_depth;
  }
  TopologyNodeId ra = root_ancestor(snap, from, NodeType::PCI_ROOT);
  TopologyNodeId rb = root_ancestor(snap, to, NodeType::PCI_ROOT);
  if (ra != kNullNodeId && rb != kNullNodeId && ra != rb) ++d.root_crossings;
  // Device class transitions.
  for (auto& seg : sp.segments) {
    auto& u = snap.node(seg.from);
    auto& v = snap.node(seg.to);
    if (detail::hardware_class(u.type) != detail::hardware_class(v.type)) ++d.device_class_transitions;
  }
  const TopologyNode& na = snap.node(from);
  const TopologyNode& nb = snap.node(to);
  if (na.native.numa_node && nb.native.numa_node) {
    d.numa_distance = *na.native.numa_node == *nb.native.numa_node ? 0.0 : 1.0;
  }
  d.measured_latency_ns = sp.estimated_latency_ns;
  d.measured_bandwidth_bps = sp.estimated_bandwidth_bps;
  auto cb = path_cost(snap, sp, weights);
  d.configured_penalty = cb.policy_penalty;
  double conf_penalty = 0.0;
  for (auto& seg : sp.segments) {
    auto& e = snap.edge(seg.edge_index);
    if (e.confidence == Confidence::LOW || e.confidence == Confidence::UNKNOWN) conf_penalty += 1.0;
  }
  d.uncertainty = conf_penalty;
  double total = cb.total;
  d.normalized_score = total / (total + 1.0);  // monotonic into [0,1); lower==closer
  return d;
}

}  // namespace topology_fabric