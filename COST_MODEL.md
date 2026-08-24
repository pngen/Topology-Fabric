# Cost Model

The cost model turns a topology path into a single scalar so the runtime can rank and compare
candidate placements. This document describes CostWeights, the per-edge cost function, and the
resulting CostBreakdown, as implemented in query.hpp, internal.hpp, and cost.cpp.

## CostWeights

CostWeights (query.hpp) is the policy object that controls path cost. Default values:

| Field | Default | Meaning |
|-------|---------|---------|
| hop_penalty | 1.0 | base cost per edge traversed |
| numa_penalty | 20.0 | penalty for crossing between different NUMA nodes |
| pci_bridge_penalty | 5.0 | penalty for entering a PCI_BRIDGE |
| root_crossing_penalty | 40.0 | penalty for entering a PCI_ROOT |
| latency_ns_weight | 0.0005 | weight applied to edge latency in ns |
| bandwidth_scale | 0.25 | weight of the inverse-bandwidth term |
| bandwidth_ref_bps | 64e9 | reference bandwidth (bytes/sec) |
| uncertainty_penalty | 4.0 | penalty per low/unknown-confidence edge |
| policy_penalty | 0.0 | blanket penalty configured by a caller |
| node_transition_penalty | 8.0 | penalty for a device-class transition |
| version | 1 | policy version, carried through results |

TopologyRuntime::default_cost_weights() returns CostWeights{}.

## Per-edge cost

The core function is detail::edge_traversal_cost(u, v, e, w), in internal.hpp. It accumulates:

  cost = hop_penalty
       + latency_ns_weight * latency
       + bandwidth_scale * (bandwidth_ref_bps / bandwidth)
       + (v is PCI_BRIDGE ? pci_bridge_penalty : 0)
       + (v is PCI_ROOT   ? root_crossing_penalty : 0)
       + (u.numa != v.numa ? numa_penalty : 0)
       + (hardware_class(u) != hardware_class(v) ? node_transition_penalty : 0)
       + (e.confidence is LOW or UNKNOWN ? uncertainty_penalty : 0)
       + policy_penalty

- latency uses e.latency_ns if set, else detail::default_latency_ns(u, v).
- bandwidth uses e.bandwidth_bytes_per_sec if set, else detail::default_bandwidth_bps(u, v); if
  that is <= 0 it falls back to bandwidth_ref_bps. The inverse-bandwidth term grows as available
  bandwidth shrinks, so slower links are more expensive.
- hardware_class is the NodeType -> HardwareClass mapping (CPU, ACCELERATOR, PCI, MEMORY,
  STORAGE, NETWORK, MACHINE, OTHER) in internal.hpp.

## Conservative defaults

internal.hpp provides class-based defaults that are used only when an edge has no measured value.
They are never presented as measured facts.

default_latency_ns (by target node type):

| NodeType | ns |
|----------|-----|
| ACCELERATOR | 300 |
| CPU_THREAD | 60 |
| PCI_BRIDGE | 120 |
| PCI_ROOT | 250 |
| STORAGE_DEVICE | 80 |
| NETWORK_INTERFACE | 500 |
| default | 100 |

default_bandwidth_bps (by target node type):

| NodeType | bps |
|----------|-----|
| ACCELERATOR | 32e9 |
| CPU_THREAD | 32e9 |
| PCI_BRIDGE | 16e9 |
| PCI_ROOT | 16e9 |
| STORAGE_DEVICE | 3e9 |
| NETWORK_INTERFACE | 10e9 |
| default | 8e9 |

## CostBreakdown

cost_breakdown(snap, path, weights) and its alias path_cost(...) accumulate the per-segment terms
into a CostBreakdown: hop_penalty, numa_penalty, pci_bridge_penalty, root_crossing_penalty,
latency_term, inverse_bandwidth_term, uncertainty_penalty, policy_penalty,
node_transition_penalty, total, and policy_version. total is the sum of all terms.

## Distance and normalized score

distance_between(snap, from, to, weights) reports the cost components plus graph_hops, pci_depth,
root_crossings, device_class_transitions, numa_distance, measured_latency_ns / measured_bandwidth_bps
(the path's estimated bottleneck), configured_penalty, uncertainty, and a normalized_score =
total / (total + 1.0). This maps the raw cost monotonically into [0,1) with lower = closer. When
no path exists it returns graph_hops = -1 and normalized_score = 1.0.

## How cost drives decisions

- lowest_cost_path uses the same per-edge cost in a Dijkstra search, so the returned path
  minimizes total cost under the given policy.
- rank_candidates computes a lowest-cost path from a source to every candidate and sorts ascending
  by cost, so the cheapest placements come first.
- explain decomposes a cost into named factors (hop_count, numa_penalty, pci_bridge_penalty,
  root_crossing_penalty, latency_term, inverse_bandwidth_term, uncertainty_penalty,
  policy_penalty, total_cost) with a human-readable summary.

## Honesty about "measured" terms

The cost model is a policy-weighted estimate. Latency and bandwidth are only as good as the facts
behind them: an edge that carries a measured bandwidth/latency uses it; an edge without uses a
conservative class default. The uncertainty_penalty penalizes edges whose confidence is LOW or
UNKNOWN. Nothing here is presented as a live measurement of link contention (see LIMITATIONS.md).
