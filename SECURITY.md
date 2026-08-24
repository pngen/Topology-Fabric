# Security

Topology Fabric processes possibly untrusted serialized snapshots (deserialize, import_synthetic)
and must not be exploited by malformed or oversized input. This document describes the input
bounds and adversarial-safety posture as implemented.

## Resource bounds

### JSON parser bounds (json.hpp)

ParseOptions enforces three caps during parsing:

- max_depth = 128 — the parser rejects nesting deeper than 128 with ParseError::DepthExceeded,
  preventing stack overflows on deeply nested documents.
- max_nodes = 4 << 20 (4,194,304) — the parser counts every value it creates and rejects beyond
  the cap with ParseError::SizeExceeded, bounding allocation.
- max_string_bytes = 1 << 24 (16 MiB) — a single string is bounded; exceeding it fails.
- allow_nan_inf = false by default, so NaN/Inf literals are rejected rather than accepted.

The parser always returns a ParseResult; every failure path yields a non-ok result with a message.
There is no path that silently accepts truncated or trailing-garbage input (TrailGarbage is
rejected).

### Snapshot bounds (Bounds, snapshot.hpp)

Bounds caps the graph itself:

- max_nodes = 1,000,000
- max_edges = 4,000,000
- max_string_bytes = 4 MiB (1 << 22)
- max_snapshot_bytes = 512 MiB (512 << 20)
- max_path_length = 16,384

### PropertyValue bounds (value.hpp)

- kMaxStringBytes = 1 MiB per string
- kMaxVectorItems = 65,536 items per vector

The merge and SnapshotBuilder both enforce max_nodes / max_edges; exceeding them records a warning
and drops the excess rather than unboundedly growing. add_node rejects a node when its id is already
present (duplicate identity) and bounds the total node count to max_nodes.

## Deserialization and import rejection

deserialize_snapshot_json and import_synthetic_json reject input with TopologyError:

- MALFORMED_DATA when the JSON parse fails, when the root is not an object, when format is not
  exactly "topology_fabric_snapshot", when nodes/edges is not an array, when a node entry is not
  an object, when id is missing/invalid hex, when an edge endpoint is missing/invalid hex, etc.
- OVERSIZED when jsonText.size() > max_snapshot_bytes, or when node count > max_nodes or edge
  count > max_edges.

TopologyError carries an ErrorCode so callers can distinguish MALFORMED_DATA from OVERSIZED.

## No unbounded allocation

- All parser growth is capped by the value-count and per-string limits.
- The snapshot graph is capped by Bounds at merge time and at build time.
- The adjacency index is built once from the bounded edge set.
- PropertyMap is ordered (std::map) and every value is a bounded variant.

## Handle validation and stale/forged ids

- A snapshot is an immutable shared_ptr; callers can only obtain one through a builder, from the
  runtime's published current snapshot, or from deserialize/import. TopologySnapshot is not
  default-constructible to a forged graph.
- node(id) throws TopologyError(NOT_FOUND) for an unknown id; find_node(id) returns nullptr. A
  forged TopologyNodeId (e.g. one never derived from a real identity) is simply not present.
- shortest_path / lowest_cost_path / rank_candidates check that both endpoints exist and return
  found=false with a reason rather than crashing on a stale/forged id.
- SnapshotBuilder rejects duplicate node ids (keeps the first) and records a warning, so a crafty
  duplicate cannot create two nodes with the same key.
- Validation rejects duplicate native identity (canonical_key) and dangling edges, so a crafted
  graph that references non-existent nodes is flagged.

## Adversarial tests

The test suite exercises these paths (see TESTING.md and test_adversarial.cpp):

- duplicate node ids (second rejected)
- oversized counts (max_nodes honored)
- malformed JSON (wrong format, non-array nodes, empty node object) rejected
- bogus BDF/NUMA values must not crash queries
- negative and NaN cost weights must not crash or infinite-loop path queries
- forged and stale ids return not-found, no crash
- deeply nested JSON rejected without stack overflow

## Limits of the posture

The runtime is not a general-purpose sandbox and does not attempt to validate semantic
plausibility of every property (e.g. a bogus but syntactically valid BDF is representable). It
binds allocation, rejects malformed input, validates graph invariants, and never crashes on a
missing/forged id. This is an honest, bounded-input defense, not a full trust boundary.