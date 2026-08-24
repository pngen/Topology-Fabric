# Interoperability

Topology Fabric exposes topology facts through a clean, stable query surface so that sibling
systems (Transfer, Compute, Bandwidth Governor, NUMA, PCIe, Collective, and others) can consume
what they need without taking on topology-modeling responsibility. This document describes that
surface and the ownership boundary.

## Ownership boundary

Topology Fabric owns topology knowledge. It does NOT copy bytes, schedule jobs, allocate buffers,
arbitrate bandwidth, or manage residency. Those decisions stay with the caller.

| System | Owns | Topology Fabric role |
|--------|------|----------------------|
| Transfer Fabric | copying, staging, scheduling, retry | provides source/destination facts |
| Compute Fabric | placement, job scheduling | provides locality/distance/cost for ranking |
| Unified Buffer | allocation, ownership, lifetime | not involved |
| FlashTier | residency/promotion, tiering | not involved |
| Tensor Cache / KV | cache admission, state lifecycle | not involved |
| Bandwidth Governor | (future) contention arbitration | provides link identities and nominal bandwidth |
| NUMA Fabric | (future) dynamic NUMA placement | provides NUMA structure |
| PCIe Fabric | (future) PCIe monitoring/control | provides PCIe hierarchy |
| Collective Fabric | (future) collective scheduling | provides topology relationships |

Topology Fabric never absorbs the decision/execution responsibilities of those systems.

## The query surface

Everything below operates on a const TopologySnapshot (immutable, thread-safe). All functions are
declared in query.hpp and take the snapshot first.

### For Transfer systems (source/destination facts)

Use the node and edge accessors plus capability checks to decide whether a transfer is supported:

- snap.find_node(id) / snap.node(id) — resolve a node.
- node.has_capability(Capability) — CPU_ADDRESSABLE, DEVICE_ADDRESSABLE, DIRECT_TRANSFER,
  STAGED_TRANSFER, PEER_ACCESS, SHARED_MEMORY, DMA_CAPABLE.
- snap.adjacency(id) — immediate neighbors.

### For Compute systems (placement / ranking)

Use the ranking and cost surface to order candidate placements:

- rank_candidates(snap, source, NodeType, max_candidates, weights) — rank accelerators (or another
  type) by topology cost from a source.
- rank_candidates(snap, source, {ids...}, weights) — rank an explicit candidate list.
- RankEntry.cost, RankEntry.breakdown (CostBreakdown), RankEntry.distance (DistanceBreakdown),
  RankEntry.path_class, RankEntry.locality, RankEntry.reasons.

### For locality-aware systems (NUMA / placement hints)

- locality_between(snap, a, b) — LocalityClass: EXACT, SAME_CORE, SAME_PACKAGE, SAME_NUMA,
  SAME_ROOT_COMPLEX, SAME_HOST, REMOTE, UNKNOWN.
- classify_path(snap, from, to) — PathClass: SAME_OBJECT, SAME_PROCESSOR, SAME_NUMA,
  SAME_PCI_DEVICE, SAME_PCI_BRIDGE, SAME_ROOT_COMPLEX, HOST_TO_ACCELERATOR,
  ACCELERATOR_TO_ACCELERATOR, HOST_TO_STORAGE, HOST_TO_NETWORK, CROSS_NUMA, REMOTE, EXTERNAL,
  UNKNOWN.
- distance_between(snap, from, to, weights) — graph_hops, pci_depth, root_crossings,
  device_class_transitions, numa_distance, measured_latency_ns / measured_bandwidth_bps,
  normalized_score.

### For path / routing systems

- shortest_path(snap, from, to, in_collocation) — fewest-edge path (BFS).
- lowest_cost_path(snap, from, to, weights) — min-cost path (Dijkstra).
- TopologyPath.segments (each with edge_type and edge_cost), .hop_count, .path_class, .locality,
  .total_cost, .estimated_bandwidth_bps, .estimated_latency_ns, .confidence, .reasons.

### For explanation / debugging

- explain(snap, source, destination, weights) — Explanation with path_class, locality, cost
  breakdown, and an ordered list of ExplanationFactor entries plus a summary.

## What facts each provider contributes (for consumers that filter by source)

| Provider | Node types contributed | Notable properties |
|----------|------------------------|--------------------|
| host | MACHINE | os.version, machine.name, processor.logical_count/group_count, memory.* |
| cpu_numa | NUMA_NODE, HOST_MEMORY_DOMAIN, CPU_PACKAGE, CPU_CORE, CPU_THREAD | native numa/package/core/group; LOCAL_TO, CONTAINS edges |
| pci | PCI_ROOT, PCI_BRIDGE, PCI_DEVICE | vendor/device/subsystem id, optional BDF; CONTAINS edges |
| cuda | ACCELERATOR, ACCELERATOR_MEMORY_DOMAIN | cuda.ordinal, compute_capability_*, cuda.total_memory, cuda.uuid; PEER_TO edges |
| storage | STORAGE_DEVICE | storage.letter, total/free bytes, filesystem, volume_label |
| network | NETWORK_INTERFACE | network.ifindex, mtu, oper_status, type, addresses, description |

Each node/edge also carries Provenance (provider, api, kind, confidence) so a consumer can filter
by how a fact was established and how certain it is.

## Snapshot lifecycle for consumers

- rt.discover() returns a shared_ptr<const TopologySnapshot>; rt.current() returns the published
  one without running discovery.
- A snapshot is immutable; hold the shared_ptr as long as you need it. A refresh publishes a new
  snapshot atomically and never mutates an old one.
- Use TopologyRuntime.diff(before, after) or compare_snapshots to detect changes;
  is_material_change(diff) tells you whether the generation advanced.
- Use rt.serialize(snap, pretty) to persist and rt.deserialize(json) to load a snapshot.

## Versioning and cost policy

CostWeights is the policy object. CostBreakdown.policy_version and TopologyPath.cost_policy_version
carry the policy version so a consumer can tell which model produced a number.
TopologyRuntime::default_cost_weights() returns the default policy. If a consumer changes the
policy, it must pass its own CostWeights to the query functions (rank_candidates,
lowest_cost_path, cost_breakdown, distance_between) rather than relying on the default path.

## Threading contract

Queries are lock-free on an immutable snapshot. Discovery serializes on one lock. Consumers that
call rt.discover() from multiple threads are safe: the runtime serializes discovery internally,
and reader threads are never blocked. Consumers that construct and query their own snapshots from
a builder do so on their own thread; SnapshotBuilder is not thread-safe for concurrent mutation.