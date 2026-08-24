# Benchmarks

Topology Fabric ships two benchmark executables built from the benchmarks/ directory:
tf_bench_graph and tf_bench_discovery. They measure structure and timing, not absolute
throughput of any hardware link. This document describes what each measures. We do not publish
every micro-architectural figure here; run the benchmarks on your target host for authoritative
numbers.

## tf_bench_graph

bench_graph.cpp builds a synthetic topology (a machine node plus N resource nodes, each connected
to the machine by a CONTAINS edge) at three sizes: 100, 1000, and 10000 nodes. For each size it
measures:

- node_lookup — 10,000 unordered-map lookups into nodes().
- path_query — 2,000 lowest_cost_path calls from the machine to a rotating set of nodes.
- rank_20 — rank_candidates(..., NodeType::ACCELERATOR, 20, CostWeights{}).
- serialize / deserialize — serialize_snapshot_json to a string, then
  deserialize_snapshot_json back (reports the serialized byte size).
- diff — compare_snapshots between two equivalent-built graphs.
- validate — b->validation().ok on the freshly built graph.

Practical guidance: the graph benchmark reports per-operation wall-clock ms for each of these at
each node count. Expect path_query and rank to grow with graph size, while node_lookup stays
roughly constant (it is a hash lookup). Serialize/deserialize grows roughly linearly with the
number of nodes and edges.

## tf_bench_discovery

bench_discovery.cpp runs real local discovery:

- First rt.discover() — prints node count, edge count, and wall-clock ms.
- Second rt.discover() — measures re-discovery timing and prints the resulting generation.
- Prints a telemetry snapshot: discovery_runs, provider_success, provider_fail.

On the validated Windows 11 AMD Ryzen host, a full local discovery completes in roughly
**15-35 ms**. Re-discovery is typically similar. This is the number the README and release notes
reference; it depends on the host and driver state, so treat it as a representative range, not a
fixed constant.

## What these benchmarks do NOT measure

- No live link bandwidth, latency, or contention (there is no such measurement yet; see
  LIMITATIONS.md). The only measurement primitive is the CLI "measure" command, a local
  host-memcpy baseline (32 MiB, 40 iterations, reports GiB/s). It is observational and does not
  touch the topology graph.
- No multi-GPU vendor measurement.
- No cross-host / fabric measurement.

## CLI in-process benchmark

The CLI's "benchmark" command runs 2,000 lowest_cost_path queries over the freshly discovered
snapshot and prints total ms and average us/query. This is a quick throughput signal for the query
engine on the real graph, distinct from the synthetic tf_bench_graph.

## Interpreting results

Run on Release builds (the CMake default). Times are wall-clock and include allocation and graph
construction for the synthetic benchmarks, so the numbers are end-to-end for the operation being
measured rather than a micro-benchmark of a single loop. For stable comparisons, run each binary
several times and take the median.
