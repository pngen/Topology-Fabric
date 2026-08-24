# Snapshots

A snapshot is the immutable, queryable unit of topology truth. This document describes
TopologySnapshot, SnapshotBuilder, Bounds, generations, and change detection as implemented in
snapshot.hpp/cpp, runtime.cpp, and diff.cpp.

## Immutability

TopologySnapshot is immutable: after SnapshotBuilder::take() the builder is spent (a second
take() throws TopologyError(INTERNAL)), and the returned
shared_ptr<const TopologySnapshot> shares no mutable state. All read methods are const and
thread-safe on a const snapshot.

## TopologySnapshot contents

- node(id) throws TopologyError(NOT_FOUND) for an unknown id; find_node(id) returns nullptr.
- edge(i) throws TopologyError(NOT_FOUND) when i is out of range; edges() returns the full vector.
- nodes() returns the unordered_map<TopologyNodeId, TopologyNode>.
- adjacency(id) returns the vector<EdgeRef> for id (empty when none).
- node_count(), edge_count().
- metadata() returns SnapshotMetadata; validation() returns ValidationResult; bounds() returns
  Bounds.

## SnapshotBuilder

The builder accumulates edges and nodes and validates invariants:

- add_node(node) returns false (and records a warning) when the builder is already taken, when
  max_nodes is exceeded, when the node property map exceeds max_edges, or when the node id is a
  duplicate (keeps the first). Otherwise it inserts and returns true.
- add_edge(edge) returns false when taken or when max_edges is exceeded; otherwise pushes and
  returns true.
- set_metadata / set_validation / set_bounds.
- take() runs validate(nodes, edges), merges any explicitly set validation results, moves data into
  the immutable snapshot, and builds the bidirectional adjacency map (both endpoints of every edge
  get an EdgeRef, so path search is undirected and locality reasoning works).

## Bounds

Bounds (snapshot.hpp) is the resource budget:

| Field | Default |
|-------|---------|
| max_nodes | 1,000,000 |
| max_edges | 4,000,000 |
| max_string_bytes | 4 MiB (1 << 22) |
| max_snapshot_bytes | 512 MiB (512 << 20) |
| max_path_length | 16,384 |

Both the merge and the builder enforce these; recursion/depth is bounded by the JSON parser (see
SECURITY.md).

## Validation

SnapshotBuilder::validate checks invariants and returns a ValidationResult (ok, errors, warnings):
dangling edges, duplicate native identity, self-containment, containment cycles (Kahn
topological sort), missing reverse PEER_TO (warning), accelerator PCI-binding mismatch (warning),
and unknown-type-without-category (warning). Validation failures are counted into telemetry.

## Generation tracking

TopologyRuntime holds the current snapshot in a std::atomic<shared_ptr<const TopologySnapshot>>.
On discover() it:

- Builds a provisional snapshot of the new discovery at the previous generation.
- Diffs the provisional snapshot against the current one with compare_snapshots.
- If is_material_change(diff), increments the generation and records a generation_change
  telemetry event; otherwise keeps the same generation.
- Builds the final snapshot at the resolved generation and atomically stores it.

generation() returns the current snapshot's generation, or 0 before the first discover().

## Change detection (diffing)

compare_snapshots(before, after) (diff.cpp) produces a TopologyDiff with
generation_before / generation_after, material_change, and a deterministic, ordered events vector
(DiffEvent). DiffEventKind values are NODE_ADDED, NODE_REMOVED, EDGE_ADDED, EDGE_REMOVED,
PROPERTY_CHANGED, CAPABILITY_CHANGED, LOCALITY_CHANGED, PROVIDER_CHANGED.

is_material_event decides which events count as a material change:

- structural events (NODE_ADDED / NODE_REMOVED / EDGE_ADDED / EDGE_REMOVED),
  CAPABILITY_CHANGED, LOCALITY_CHANGED, PROVIDER_CHANGED are material.
- PROPERTY_CHANGED is material only when on_edge is true (edge property changes reflect link
  structure facts); node property changes (telemetry) are not material.

Material-change detection is what keeps generation numbers stable across non-structural
rediscoveries.

## Peer-to-peer publication semantics

Readers grab the snapshot shared_ptr from current(), then query it lock-free. A refresh swaps in a
new snapshot; a reader holding the old shared_ptr continues to see the old immutable snapshot
without any locks or data races.

## Synthetic and imported snapshots

TopologyRuntime::import_synthetic(json) parses a snapshot document and, if its metadata.synthetic
flag is not set, rebuilds the immutable snapshot with synthetic=true. SnapshotMetadata.synthetic
and node-level synthetic flags clearly mark imported/synthetic topologies.
