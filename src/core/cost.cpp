
#include "topology_fabric/query.hpp"
#include "internal.hpp"

namespace topology_fabric {
namespace {

void accumulate(const TopologyNode& u, const TopologyNode& v, const TopologyEdge& e,
                const CostWeights& w, CostBreakdown& cb) {
  cb.hop_penalty += w.hop_penalty;

  double lat = e.latency_ns.value_or(detail::default_latency_ns(u, v));
  cb.latency_term += w.latency_ns_weight * lat;

  double bw = e.bandwidth_bytes_per_sec.value_or(detail::default_bandwidth_bps(u, v));
  if (bw <= 0.0) bw = w.bandwidth_ref_bps;
  cb.inverse_bandwidth_term += w.bandwidth_scale * (w.bandwidth_ref_bps / bw);

  if (v.type == NodeType::PCI_BRIDGE) cb.pci_bridge_penalty += w.pci_bridge_penalty;
  if (v.type == NodeType::PCI_ROOT) cb.root_crossing_penalty += w.root_crossing_penalty;

  if (detail::hardware_class(u.type) != detail::hardware_class(v.type))
    cb.node_transition_penalty += w.node_transition_penalty;

  if (e.confidence == Confidence::LOW || e.confidence == Confidence::UNKNOWN)
    cb.uncertainty_penalty += w.uncertainty_penalty;

  cb.policy_penalty += w.policy_penalty;
}

}  // namespace

CostBreakdown cost_breakdown(const TopologySnapshot& snap, const TopologyPath& path,
                             const CostWeights& weights) {
  CostBreakdown cb;
  cb.policy_version = weights.version;
  for (const auto& seg : path.segments) {
    auto& u = snap.node(seg.from);
    auto& v = snap.node(seg.to);
    auto& e = snap.edge(seg.edge_index);
    accumulate(u, v, e, weights, cb);
  }
  // Path-level NUMA penalty: crossing between different NUMA nodes is a property of the
  // endpoints, not of any single (possibly NUMA-less) intermediate node.
  const TopologyNode& src = snap.node(path.source);
  const TopologyNode& dst = snap.node(path.destination);
  if (src.native.numa_node && dst.native.numa_node && *src.native.numa_node != *dst.native.numa_node)
    cb.numa_penalty += weights.numa_penalty;

  cb.total = cb.hop_penalty + cb.numa_penalty + cb.pci_bridge_penalty +
             cb.root_crossing_penalty + cb.latency_term + cb.inverse_bandwidth_term +
             cb.uncertainty_penalty + cb.policy_penalty + cb.node_transition_penalty;
  return cb;
}

CostBreakdown path_cost(const TopologySnapshot& snap, const TopologyPath& path,
                        const CostWeights& weights) {
  return cost_breakdown(snap, path, weights);
}

}  // namespace topology_fabric