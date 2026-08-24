
// TopologyFabric/query.hpp - path, distance, cost, locality, ranking, and explain APIs.
#pragma once
#include <cstdint>
#include <optional>
#include <vector>
#include <string>
#include <memory>
#include "topology_fabric/snapshot.hpp"
#include "topology_fabric/types.hpp"

namespace topology_fabric {

// ---- Path -------------------------------------------------------------
struct PathSegment {
  TopologyNodeId from;
  TopologyNodeId to;
  size_t edge_index = 0;
  EdgeType edge_type = EdgeType::CONNECTED_TO;
  double edge_cost = 0.0;
};

struct TopologyPath {
  bool found = false;
  TopologyNodeId source;
  TopologyNodeId destination;
  std::vector<PathSegment> segments;
  int hop_count = 0;
  PathClass path_class = PathClass::UNKNOWN;
  LocalityClass locality = LocalityClass::UNKNOWN;
  double total_cost = 0.0;
  double estimated_bandwidth_bps = 0.0;   // bottleneck (min) along path
  double estimated_latency_ns = 0.0;      // sum along path
  Confidence confidence = Confidence::UNKNOWN;
  ProvenanceKind provenance = ProvenanceKind::UNKNOWN;
  uint32_t cost_policy_version = 0;
  std::vector<std::string> reasons;
};

// ---- Distance ---------------------------------------------------------
struct DistanceBreakdown {
  int graph_hops = 0;
  int pci_depth = 0;
  int root_crossings = 0;
  int device_class_transitions = 0;
  std::optional<double> numa_distance;
  std::optional<double> measured_latency_ns;
  std::optional<double> measured_bandwidth_bps;
  double configured_penalty = 0.0;
  double uncertainty = 0.0;
  double normalized_score = 0.0;   // policy-weighted, [0,1], lower==closer
};

// ---- Cost ---------------------------------------------------------
struct CostWeights {
  double hop_penalty = 1.0;
  double numa_penalty = 20.0;
  double pci_bridge_penalty = 5.0;
  double root_crossing_penalty = 40.0;
  double latency_ns_weight = 0.0005;
  double bandwidth_scale = 0.25;         // weight of inverse-bandwidth term
  double bandwidth_ref_bps = 64e9;       // reference bandwidth (bytes/s)
  double uncertainty_penalty = 4.0;
  double policy_penalty = 0.0;
  double node_transition_penalty = 8.0;
  uint32_t version = 1;

  bool operator==(const CostWeights&) const noexcept = default;
};

struct CostBreakdown {
  double hop_penalty = 0.0;
  double numa_penalty = 0.0;
  double pci_bridge_penalty = 0.0;
  double root_crossing_penalty = 0.0;
  double latency_term = 0.0;
  double inverse_bandwidth_term = 0.0;
  double uncertainty_penalty = 0.0;
  double policy_penalty = 0.0;
  double node_transition_penalty = 0.0;
  double total = 0.0;
  uint32_t policy_version = 0;
};

// ---- Ranking ---------------------------------------------------------
struct RankEntry {
  TopologyNodeId id;
  double cost = 0.0;
  CostBreakdown breakdown;
  DistanceBreakdown distance;
  PathClass path_class = PathClass::UNKNOWN;
  LocalityClass locality = LocalityClass::UNKNOWN;
  std::vector<std::string> reasons;
};

struct RankResult {
  bool found = false;
  TopologyNodeId source;
  std::vector<RankEntry> ranked;          // ascending cost
  uint32_t cost_policy_version = 0;
};

// ---- Explain ---------------------------------------------------------
struct ExplanationFactor {
  std::string name;
  std::string value;
  std::string note;
  double weight = 0.0;
};
struct Explanation {
  TopologyNodeId source;
  TopologyNodeId destination;
  PathClass path_class = PathClass::UNKNOWN;
  LocalityClass locality = LocalityClass::UNKNOWN;
  CostBreakdown cost;
  std::vector<ExplanationFactor> factors;
  std::string summary;
};

// ---- Query functions (operate on an immutable snapshot) --------------
// Path search: hops = fewest edges; cost = Dijkstra on a CostWeights policy.
TopologyPath shortest_path(const TopologySnapshot& snap, TopologyNodeId from,
                           TopologyNodeId to, bool in_collocation = false);
TopologyPath lowest_cost_path(const TopologySnapshot& snap, TopologyNodeId from,
                              TopologyNodeId to, const CostWeights& weights);

// Locality / classification.
LocalityClass locality_between(const TopologySnapshot& snap, TopologyNodeId a, TopologyNodeId b);
PathClass classify_path(const TopologySnapshot& snap, TopologyNodeId from, TopologyNodeId to);

// Distance + cost breakdowns.
DistanceBreakdown distance_between(const TopologySnapshot& snap, TopologyNodeId from,
                                   TopologyNodeId to, const CostWeights& weights);
CostBreakdown cost_breakdown(const TopologySnapshot& snap, const TopologyPath& path,
                             const CostWeights& weights);
CostBreakdown path_cost(const TopologySnapshot& snap, const TopologyPath& path,
                        const CostWeights& weights);

// Ranking candidate nodes of a type by topology cost from a source.
RankResult rank_candidates(const TopologySnapshot& snap, TopologyNodeId source,
                           NodeType type, size_t max_candidates,
                           const CostWeights& weights);
RankResult rank_candidates(const TopologySnapshot& snap, TopologyNodeId source,
                           const std::vector<TopologyNodeId>& candidates,
                           const CostWeights& weights);

// Explain a ranking/path decision.
Explanation explain(const TopologySnapshot& snap, TopologyNodeId source,
                    TopologyNodeId destination, const CostWeights& weights);

// Low-level node traversal helpers (parent/children/etc.).
std::vector<TopologyNodeId> parents(const TopologySnapshot& snap, TopologyNodeId id, EdgeType type);
std::vector<TopologyNodeId> children(const TopologySnapshot& snap, TopologyNodeId id, EdgeType type);
std::vector<TopologyNodeId> ancestors(const TopologySnapshot& snap, TopologyNodeId id, EdgeType type);
std::vector<TopologyNodeId> descendants(const TopologySnapshot& snap, TopologyNodeId id, EdgeType type);
std::vector<TopologyNodeId> siblings(const TopologySnapshot& snap, TopologyNodeId id, EdgeType type);
std::vector<TopologyNodeId> neighbors(const TopologySnapshot& snap, TopologyNodeId id);
TopologyNodeId root_ancestor(const TopologySnapshot& snap, TopologyNodeId id, NodeType root_type);

}  // namespace topology_fabric
