# Design

This document records the design principles and trade-offs in Topology Fabric 1.0.0, as actually
implemented. It is written for engineers who want to understand why the runtime is shaped the
way it is before they change it.

## Guiding principles

1. **Facts, not decisions.** Topology Fabric computes facts about the fabric and exposes them. It
   never places work, copies bytes, or arbitrates bandwidth. Ownership stays with the caller.
2. **Honesty over completeness.** Every fact carries a Provenance (how it was established) and a
   Confidence. When the platform does not report something, the runtime says so instead of
   inventing a value. Partial discovery is flagged and reported, not silently assumed complete.
3. **Deterministic and reproducible.** Identical inputs produce identical outputs. Node ids are
   derived deterministically from canonical native identity, never from enumeration order.
4. **Bounded everywhere.** Every input surface is capped; malformed or oversized data is rejected
   rather than handled by growing without limit.
5. **Immutable publish, lock-free read.** Queries run on an immutable snapshot. There is no
   mutable graph state for readers to race on.
6. **Never silently upgrade authority.** High-confidence facts do not overwrite low-confidence
   ones; the merge records a conflict instead of picking a winner silently.

## The topology graph

The model is a typed property graph:

- A **TopologyNode** is a resource. It has a 128-bit TopologyNodeId, a NodeType, a
  category/name/display_name, NativeIdentity (the platform-reported id), a Capability bitmask, a
  PropertyMap, Provenance, Confidence, and a synthetic flag.
- A **TopologyEdge** is a relationship. It has source/target ids, an EdgeType, an EdgeDirection,
  plus optional link facts (width, PCIe generation, bandwidth, latency), provenance, confidence,
  and properties.

See GRAPH_MODEL.md for the full vocabulary.

## Identity and the merge problem

The central design problem is that multiple providers (Windows CPU/NUMA, Configuration Manager,
CUDA, volume, network) observe overlapping resources and must be combined into one graph without
duplicating a physical device. The solution is a two-stage identity model:

- **NativeIdentity** captures what a platform reports (PCI BDF, vendor/device id, NUMA node,
  CUDA ordinal/UUID, storage id, MAC/interface name, machine name).
- **NativeIdentity::canonical_key()** reduces that to a single deterministic string, in
  priority order: PCI BDF (pci:<bdf>), then storage id/path, then network hardware id, then
  network interface name, then CUDA UUID, then CUDA ordinal, then machine name.
- **TopologyNodeId** is derive_node_id("touchstone","node", canonical_key).

The merge (merge_contributions) groups contributed nodes by that canonical key. Within a group
it pre-sorts members best-first by confidence (ties broken by contribution index, then node
index), picks the best type by a fixed type rank, ORs the capabilities, takes the maximum
confidence, and keeps the first value for each property key (recording a conflict when a later
member disagrees). This is why a PCI BDF reported by both the PCI provider and the CUDA provider
collapses into a single node instead of two.

## Confidence and provenance

Every nontrivial fact has both. Provenance records the source (provider, api, provider_version)
and the how (ProvenanceKind: discovered, inferred, measured, user-supplied, unknown). Confidence
records the certainty (authoritative, high, medium, low, unknown). The two are deliberate
different axes: a fact can be discovered (observed via an API) and still only medium confidence
(e.g. a PCI bridge name parsed from a device-instance string), or inferred yet authoritative in
practice. See PROVENANCE.md and CONFIDENCE.md.

The never-upgrade rule is enforced at merge time: a node's confidence is set to
ConfidenceRanking::maximum over its members (never lowered), and the selected Provenance is the
best-confidence member's provenance. Property conflicts are recorded, not silently resolved in
favor of the higher-confidence source at value level.

## Costing and path selection

Path queries come in two flavors, both in query.hpp:

- shortest_path(...) is breadth-first search, fewest edges. Used for structural connectivity.
- lowest_cost_path(snap, from, to, weights) is Dijkstra over a policy CostWeights.

The per-edge cost is a single function, detail::edge_traversal_cost(u, v, e, w), which sums:
hop penalty, a latency term (latency_ns_weight x latency), an inverse-bandwidth term
(bandwidth_scale x bandwidth_ref_bps / bandwidth), a PCI-bridge penalty (when the target is a
PCI_BRIDGE), a root-crossing penalty (when the target is a PCI_ROOT), a NUMA penalty (when the
endpoints differ in NUMA), a device-class transition penalty (when the hardware classes differ),
an uncertainty penalty (when the edge confidence is low/unknown), and a policy penalty.

A key design choice is that defaults are conservative and clearly separated from measured facts.
internal.hpp provides class-based default latency/bandwidth that are used only when the edge has
no measured value. The path carries both a cost and an estimated bottleneck-bandwidth /
summed-latency, plus a PathClass and a LocalityClass. See PATHS.md and COST_MODEL.md.

## Immutability and generations

A TopologySnapshot is immutable: after SnapshotBuilder::take() the builder is spent and the
returned shared_ptr is const. TopologyRuntime holds the current snapshot in a
std::atomic shared_ptr and swaps it on refresh. Readers copy the shared_ptr and read without locks.

Generation advances only on a material change (see SNAPSHOTS.md). The runtime builds a provisional
snapshot of the new discovery, diffs it against the current one with compare_snapshots, and only
increments the generation when is_material_change is true. This means a discovery run that
observes no structural change keeps the same generation, which lets downstream systems cheaply
detect nothing changed.

## Why a custom bounded JSON parser

Rather than depend on a third-party JSON library, json.hpp implements a small parser and
serializer with explicit resource bounds (max_depth 128, total-value cap, per-string cap) and a
strict reject-on-malformed stance. This is deliberate: the runtime consumes possibly untrusted
serialized snapshots (deserialize, import), so allocation and nesting must be bounded and
malformed input must fail loudly with TopologyError (MALFORMED_DATA / OVERSIZED). See SECURITY.md.

## Partial discovery is first-class

Providers report a partial flag and a list of warnings. The runtime aggregates these into
SnapshotMetadata.partial_discovery and SnapshotMetadata.warnings. A provider may succeed (it found
what it could) while still being partial (e.g. pci cannot always read the BDF on Windows, storage
does not associate disk with PCI, network does not resolve NUMA). The goal is that a consumer can
always tell how much of the fabric the snapshot truly reflects.

## Non-goals, explicitly

- No transfer, placement, or arbitration logic.
- No allocation/lifetime management of buffers.
- No runtime measurement of live link contention or dynamic bandwidth; only a local host-memcpy
  baseline is available (see LIMITATIONS.md).
- Only Windows providers are implemented. There is no ROCm/HIP/Level Zero/Metal/NVML/Vulkan/CXL/
  RDMA/InfiniBand provider yet.
