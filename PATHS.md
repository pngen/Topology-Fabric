# Paths

Path queries answer "how do I get from A to B in the topology graph, and how costly is it?" This
document describes the path model, the two path queries, and the classification/locality scheme.
All functions are in query.hpp and implemented in path.cpp / locality.cpp / ranking.cpp.

## Path data types

- PathSegment: { from, to, edge_index, edge_type, edge_cost }.
- TopologyPath: { found, source, destination, segments, hop_count, path_class, locality,
  total_cost, estimated_bandwidth_bps, estimated_latency_ns, confidence, provenance,
  cost_policy_version, reasons }.

A path is reported even when not found (found=false) with a reasons list explaining why.

## Two path queries

### shortest_path(snap, from, to, in_collocation=false)

Breadth-first search over the adjacency list, minimizing hop count. It treats collocation edges
(SHARES_PARENT, SHARES_NUMA) as traversable only when in_collocation is true. When from==to it
returns found=true with 0 hops, LocalityClass::EXACT, PathClass::SAME_OBJECT. When an endpoint is
missing it returns found=false; when no route exists it returns found=false with a reason. It then
calls fill_path to compute the estimated bottleneck bandwidth, summed latency, hop count, locality,
path class (using the default CostWeights for consistency), and defaults confidence to MEDIUM.

### lowest_cost_path(snap, from, to, weights)

Dijkstra over the adjacency list using detail::edge_traversal_cost as the edge weight (see
COST_MODEL.md). Edges are traversed for reachability (collocation edges skipped unless the caller
requests collocation). It reconstructs the segments, computes the same fill_path fields, and sets
total_cost to the summed edge cost.

## fill_path

For each segment, fill_path reads the edge's optional bandwidth/latency, substituting a
device-class default when absent, takes the minimum bandwidth along the path (bottleneck) and the
sum of latencies, then sets hop_count, locality = locality_between(...), path_class =
classify_path(...), cost_policy_version = weights.version, and a default confidence of MEDIUM
(refined by cost_breakdown).

## Locality classification

locality_between(snap, a, b) returns a LocalityClass:

- EXACT when a == b.
- SAME_CORE when both are CPU_THREAD with equal core_id and cpu_package.
- SAME_PACKAGE when both are CPU nodes with equal cpu_package.
- SAME_NUMA when both have the same numa_node.
- SAME_ROOT_COMPLEX when both resolve to the same PCI_ROOT ancestor.
- SAME_HOST when both resolve to the same MACHINE ancestor.
- REMOTE when they have different machine ancestors (or one is remote).
- UNKNOWN when neither has a machine ancestor.

## Path classification

classify_path(snap, from, to) returns a PathClass:

- SAME_OBJECT when from == to.
- ACCELERATOR_TO_ACCELERATOR (both are accelerators).
- HOST_TO_ACCELERATOR (one is accelerator, the other is a host node).
- HOST_TO_STORAGE (host node <-> STORAGE_DEVICE).
- HOST_TO_NETWORK (host node <-> NETWORK_INTERFACE).
- SAME_PROCESSOR (both are CPU nodes).
- SAME_PCI_BRIDGE (both endpoints are PCI endpoints sharing a common PCI_BRIDGE parent).
- SAME_ROOT_COMPLEX (both endpoints share the same PCI_ROOT ancestor).
- SAME_NUMA (both have the same numa_node).
- CROSS_NUMA (both have a numa_node but different values).
- REMOTE (same machine ancestor but neither CPU/accelerator/storage/network specialization
  matched and no NUMA both-set case).
- UNKNOWN otherwise.

"host" nodes are CPU_PACKAGE, CPU_CORE, CPU_THREAD, NUMA_NODE, HOST_MEMORY_DOMAIN, MACHINE.

## Distance breakdown

distance_between(snap, from, to, weights) returns a DistanceBreakdown with graph_hops, pci_depth,
root_crossings, device_class_transitions, numa_distance (0/1 when both have a NUMA node),
measured_latency_ns (the estimated path latency), measured_bandwidth_bps (the estimated bottleneck),
configured_penalty (policy penalty), uncertainty (count of low/unknown-confidence edges on the
path), and normalized_score. When from==to it returns 0 hops and score 0.0. When no path exists it
sets graph_hops = -1, normalized_score = 1.0, uncertainty = 1.0. Otherwise normalized_score =
total / (total + 1.0), which is monotone into [0,1) with lower meaning closer.

## Traversal helpers

For structural queries the graph API provides parents, children, ancestors, descendants, siblings,
neighbors, and root_ancestor (in graph.cpp). root_ancestor walks CONTAINS parents until it reaches
a node of the requested root_type (e.g. PCI_ROOT or MACHINE); it guards against cycles.

## Ranking and explanation

rank_candidates(snap, source, type | candidate list, max_candidates, weights) computes a
lowest-cost path from the source to every candidate of the requested type (or in the explicit
candidate list), and sorts the results ascending by cost, capped by max_candidates. Each RankEntry
carries the cost, the CostBreakdown and DistanceBreakdown, the path class and locality, and a
sorted reasons vector built from the locality/classification/distance facts (e.g. "same_numa",
"same_package", "host_to_accelerator", "crosses_pci_root", "uncertain_path").

explain(snap, source, destination, weights) builds a human-readable Explanation: path class,
locality, cost breakdown, and an ordered list of ExplanationFactor entries (locality, path_class,
hop_count, numa_penalty, pci_bridge_penalty, root_crossing_penalty, latency_term,
inverse_bandwidth_term, uncertainty_penalty, policy_penalty, total_cost) plus a summary string.
