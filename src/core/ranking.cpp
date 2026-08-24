
#include "topology_fabric/query.hpp"
#include "internal.hpp"
#include <algorithm>

namespace topology_fabric {
namespace {

std::vector<std::string> build_reasons(const TopologySnapshot& snap, TopologyNodeId src,
                                       TopologyNodeId dst, const CostWeights& w) {
  std::vector<std::string> reasons;
  LocalityClass loc = locality_between(snap, src, dst);
  auto classification = classify_path(snap, src, dst);
  auto d = distance_between(snap, src, dst, w);
  if (loc == LocalityClass::SAME_NUMA) reasons.push_back("same_numa");
  if (loc == LocalityClass::SAME_PACKAGE) reasons.push_back("same_package");
  if (loc == LocalityClass::SAME_CORE) reasons.push_back("same_core");
  if (loc == LocalityClass::SAME_ROOT_COMPLEX) reasons.push_back("same_root_complex");
  if (loc == LocalityClass::REMOTE) reasons.push_back("remote");
  if (loc == LocalityClass::UNKNOWN) reasons.push_back("locality_unknown");
  if (classification == PathClass::HOST_TO_ACCELERATOR) reasons.push_back("host_to_accelerator");
  if (classification == PathClass::ACCELERATOR_TO_ACCELERATOR) reasons.push_back("accelerator_to_accelerator");
  if (classification == PathClass::CROSS_NUMA) reasons.push_back("cross_numa");
  if (d.root_crossings > 0) reasons.push_back("crosses_pci_root");
  if (d.pci_depth == 0 && loc != LocalityClass::UNKNOWN) reasons.push_back("direct_connectivity");
  if (d.uncertainty > 0) reasons.push_back("uncertain_path");
  if (classification == PathClass::SAME_PCI_BRIDGE) reasons.push_back("same_pci_bridge");
  if (classification == PathClass::SAME_PCI_DEVICE) reasons.push_back("same_pci_device");
  std::sort(reasons.begin(), reasons.end());
  return reasons;
}

}  // namespace

RankResult rank_candidates(const TopologySnapshot& snap, TopologyNodeId source,
                           NodeType type, size_t max_candidates, const CostWeights& weights) {
  RankResult rr;
  rr.source = source;
  rr.cost_policy_version = weights.version;
  if (!snap.find_node(source)) return rr;
  for (const auto& [id, n] : snap.nodes()) {
    if (id == source || n.type != type) continue;
    auto p = lowest_cost_path(snap, source, id, weights);
    if (!p.found) continue;
    RankEntry e;
    e.id = id;
    e.cost = p.total_cost;
    e.breakdown = cost_breakdown(snap, p, weights);
    e.distance = distance_between(snap, source, id, weights);
    e.path_class = p.path_class;
    e.locality = p.locality;
    e.reasons = build_reasons(snap, source, id, weights);
    rr.ranked.push_back(std::move(e));
  }
  std::sort(rr.ranked.begin(), rr.ranked.end(),
            [](const RankEntry& a, const RankEntry& b) {
              if (a.cost != b.cost) return a.cost < b.cost;
              return a.id < b.id;   // deterministic tie-break
            });
  rr.found = !rr.ranked.empty();
  if (max_candidates > 0 && rr.ranked.size() > max_candidates)
    rr.ranked.resize(max_candidates);
  return rr;
}

RankResult rank_candidates(const TopologySnapshot& snap, TopologyNodeId source,
                           const std::vector<TopologyNodeId>& candidates,
                           const CostWeights& weights) {
  RankResult rr;
  rr.source = source;
  rr.cost_policy_version = weights.version;
  if (!snap.find_node(source)) return rr;
  for (auto id : candidates) {
    if (id == source || !snap.find_node(id)) continue;
    auto p = lowest_cost_path(snap, source, id, weights);
    if (!p.found) continue;
    RankEntry e;
    e.id = id;
    e.cost = p.total_cost;
    e.breakdown = cost_breakdown(snap, p, weights);
    e.distance = distance_between(snap, source, id, weights);
    e.path_class = p.path_class;
    e.locality = p.locality;
    e.reasons = build_reasons(snap, source, id, weights);
    rr.ranked.push_back(std::move(e));
  }
  std::sort(rr.ranked.begin(), rr.ranked.end(),
            [](const RankEntry& a, const RankEntry& b) {
              if (a.cost != b.cost) return a.cost < b.cost;
              return a.id < b.id;   // deterministic tie-break
            });
  rr.found = !rr.ranked.empty();
  return rr;
}

Explanation explain(const TopologySnapshot& snap, TopologyNodeId source,
                    TopologyNodeId destination, const CostWeights& weights) {
  Explanation ex;
  ex.source = source;
  ex.destination = destination;
  if (!snap.find_node(source) || !snap.find_node(destination)) {
    ex.summary = "endpoint node not present in snapshot";
    return ex;
  }
  auto p = lowest_cost_path(snap, source, destination, weights);
  ex.path_class = p.path_class;
  ex.locality = p.locality;
  ex.cost = cost_breakdown(snap, p, weights);
  auto reasons = build_reasons(snap, source, destination, weights);
  // Factors in deterministic order.
  auto add = [&](std::string name, std::string value, std::string note, double weight) {
    ex.factors.push_back(ExplanationFactor{std::move(name), std::move(value), std::move(note), weight});
  };
  add("locality", std::string(to_string(p.locality)), "policy locality classification", 0);
  add("path_class", std::string(to_string(p.path_class)), "path categorization", 0);
  add("hop_count", std::to_string(p.hop_count), "edge count along path", p.hop_count);
  add("numa_penalty", std::to_string(ex.cost.numa_penalty), "same/different NUMA cost", ex.cost.numa_penalty);
  add("pci_bridge_penalty", std::to_string(ex.cost.pci_bridge_penalty), "bridges crossed", ex.cost.pci_bridge_penalty);
  add("root_crossing_penalty", std::to_string(ex.cost.root_crossing_penalty), "PCI root complexes crossed", ex.cost.root_crossing_penalty);
  add("latency_term", std::to_string(ex.cost.latency_term), "latency weight", ex.cost.latency_term);
  add("inverse_bandwidth_term", std::to_string(ex.cost.inverse_bandwidth_term), "inverse bandwidth weight", ex.cost.inverse_bandwidth_term);
  add("uncertainty_penalty", std::to_string(ex.cost.uncertainty_penalty), "low-confidence edges", ex.cost.uncertainty_penalty);
  add("policy_penalty", std::to_string(ex.cost.policy_penalty), "configured policy penalty", ex.cost.policy_penalty);
  add("total_cost", std::to_string(p.total_cost), "sum of all penalties/weights", p.total_cost);
  std::string sum = "path class " + std::string(to_string(p.path_class)) +
                    ", locality " + std::string(to_string(p.locality)) +
                    ", cost " + std::to_string(p.total_cost);
  ex.summary = sum;
  return ex;
}

}  // namespace topology_fabric