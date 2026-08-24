# Architecture

Topology Fabric 1.0.0 is a **vendor-neutral C++20 systems runtime** for discovering, modeling,
measuring, validating, scoring, and exposing the hardware and interconnect topology beneath
heterogeneous AI infrastructure. This document describes the architectural boundary, the layers,
and the ownership model. It reflects the actual implementation in this repository.

## Core question

> What hardware topology exists, what paths connect it, how costly are those paths, and what
> locality constraints should higher-level runtimes know before they place or move anything?

Topology Fabric answers that question. It does **not** copy bytes, schedule jobs, allocate
buffers, or arbitrate bandwidth. Those are the responsibilities of sibling systems (see
[INTEROP.md](INTEROP.md)).

## What Topology Fabric owns

Topology Fabric owns **topology knowledge**:

- **Discovery** of machine, CPU (package/core/thread), NUMA, memory domains, accelerators,
  PCIe hierarchy, storage, and network interfaces.
- **Graph modeling** of resources as typed nodes and typed relationships as edges.
- **Path construction**, classification, distance, cost estimation, and scoring.
- **Locality classification** (exact / same core / same package / same NUMA / same root complex /
  same host / remote / unknown).
- **Snapshots** (immutable), generations, change detection, and diffing.
- **Validation** against graph invariants, plus honest partial-discovery reporting.
- **Serialization** to bounded, versioned JSON.
- A **provider/backend abstraction** and deterministic merging of provider contributions.
- **Telemetry**, benchmarking, and a **stable query API**.

## Layers

The runtime is organized into four cooperating layers. The public API is the only stable surface;
everything under `src/` is an implementation detail.

### 1. Public headers (`include/topology_fabric/`)

The installed API. Headers expose the enums, POD structs, and functions that constitute the
contract. Every public type lives in namespace `topology_fabric`.

- `types.hpp` — `NodeType`, `EdgeType`, `EdgeDirection`, `ProvenanceKind`, `Confidence`,
  `LocalityClass`, `PathClass`, `FactClass`, `DiffEventKind`, and the enum<->string
  conversion functions.
- `node_id.hpp` — `TopologyNodeId` (128-bit) and the deterministic derivation functions.
- `identity.hpp` — `NativeIdentity` and its `canonical_key()` / `pci_bdf_string()`.
- `value.hpp` — `PropertyValue` (bounded variant) and `PropertyMap`.
- `provenance.hpp` / `confidence.hpp` — provenance struct and confidence ranking helpers.
- `capability.hpp` — the `Capability` bitmask and helpers.
- `node.hpp` / `edge.hpp` — `TopologyNode` and `TopologyEdge`.
- `snapshot.hpp` — `TopologySnapshot` (immutable), `SnapshotBuilder`, `Bounds`,
  `ValidationResult`, `SnapshotMetadata`.
- `provider.hpp` / `registry.hpp` / `merge.hpp` — the provider model and merge.
- `query.hpp` — path, distance, cost, ranking, explain, and traversal functions.
- `runtime.hpp` — the `TopologyRuntime` facade.
- `diff.hpp` / `serialization.hpp` — diffing and versioned JSON.
- `telemetry.hpp` — runtime counters.
- `result.hpp` — `ErrorCode`, `Error`, `Result<T>`, `TopologyError`.
- `json.hpp` — the bounded JSON value/parser/serializer.
- `version.hpp` / `export.hpp` — version identification and symbol-export macros.

### 2. Core implementation (`src/core/`)

One translation unit per concern:

| Source | Responsibility |
|--------|----------------|
| `types.cpp` | enum<->string conversions |
| `node_id.cpp` | 128-bit id hashing and derivation |
| `identity.cpp` | `canonical_key()`, `pci_bdf_string()` |
| `node.cpp`, `edge.cpp`, `value.cpp`, `provenance.cpp` | POD bodies and `TopologyEdge::key()` |
| `snapshot.cpp` | `SnapshotBuilder`, validation, adjacency build |
| `merge.cpp` | `merge_contributions` |
| `registry.cpp` | provider registry |
| `runtime.cpp` | `TopologyRuntime::discover`, generation, serialization |
| `path.cpp` | BFS shortest path, Dijkstra lowest-cost path |
| `cost.cpp` | `CostBreakdown` accumulation |
| `locality.cpp` | `locality_between`, `classify_path`, `distance_between` |
| `distance.cpp` | (empty; `distance_between` lives in `locality.cpp`) |
| `ranking.cpp` | `rank_candidates`, `explain`, reason building |
| `graph.cpp` | traversal helpers (parents, children, ancestors, ...) |
| `diff.cpp` | `compare_snapshots`, material-change rules |
| `serialization.cpp` | JSON snapshot round-trip |
| `telemetry.cpp` | counters |

Shared helpers are declared in the non-installed header `src/internal.hpp`, which supplies the
`HardwareClass` mapping, conservative default latency/bandwidth by device class, and the
per-edge traversal cost function `detail::edge_traversal_cost`.

### 3. Providers (`src/providers/`)

Providers are `TopologyProvider` subclasses that observe platform facts and contribute them
independently. The built-in set is `host`, `cpu_numa`, `pci`, `cuda`, `storage`,
`network`. Currently only Windows backends are implemented; on non-Windows builds each provider
returns a graceful "not implemented" partial contribution. The factories are declared in
`src/providers/provider_factories.hpp`.

### 4. CLI / examples / benchmarks / tests

- `src/cli/main.cpp` — the `topology-fabric` command-line tool.
- `examples/` — 14 runnable `ex_*` programs.
- `benchmarks/` — `tf_bench_graph` and `tf_bench_discovery`.
- `tests/` — `tf_tests` with 67 test cases.

## Discovery pipeline

`TopologyRuntime::discover()` is the single entry point. It:

1. Takes the discovery mutex (discovery is serialized on one lock).
2. Runs every registered provider in deterministic registration order.
3. Gathers each provider's `Contribution` (nodes, edges, warnings, partial flag).
4. Calls `merge_contributions(contributions, bounds)` to build a `MergedGraph`.
5. Attaches any rootless resource nodes (accelerator, storage, network, PCI root/device) under
   the machine node with an inferred `CONTAINS` edge so the graph stays connected.
6. Builds a provisional snapshot, diffs it against the current one, and decides the new
   generation: increments only on a **material change**.
7. Builds and atomically publishes the final immutable snapshot via a `std::atomic<`shared_ptr`>`.

Reads are lock-free against the published snapshot because snapshots are immutable. Refresh
publishes a new snapshot atomically; readers observe either the old or the new snapshot, never a
partial one.

## Determinism

Merge order, node iteration, property maps (`std::map`), and edge deduplication are all
deterministic for the same inputs. `TopologyNodeId` is derived deterministically from
(namespace, category, native identity), never from transient enumeration order, so a given
physical device maps to a stable id across runs.

## Bounded by design

Every input surface is bounded. `Bounds` caps nodes (1,000,000), edges (4,000,000), per-string
bytes (4 MiB), serialized snapshot size (512 MiB), and path length (16,384). The JSON parser is
bounded (max depth 128, value-count cap, per-string cap) and rejects malformed input with a
`TopologyError` (see [SECURITY.md](SECURITY.md)).

## Ownership boundary

Topology Fabric exposes facts; it never absorbs the decision/execution responsibilities of other
systems. The full boundary table is in [INTEROP.md](INTEROP.md).
