# Testing

Topology Fabric uses a small dependency-free test harness (tests/test_harness.hpp) and a single
`tf_tests` executable. As of 1.0.0 there are **67 passing test cases**. This document describes
the suites, the methodology, and the guarantees the tests provide.

## Running the tests

Configure and build as in CONTRIBUTING.md, then:

    cmake -S . -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release
    ctest --test-dir build -C Release
    # or directly
    build/tests/Release/tf_tests.exe

The harness prints each test as it runs, reports failures to stderr, and returns 0 only when all
tests pass. There are **no test timeouts** configured; every test completes deterministically.

## The harness

test_harness.hpp defines the TF_TEST(name) registration macro, the ASSERT / ASSERT_EQ /
ASSERT_NEAR / ASSERT_THROWS macros, and run_all(). The main() in test_main.cpp simply calls
tf_test::run_all(). A thrown exception in a test is caught and counted as a failure, never as a
crash that would terminate the runner.

## Test suites

| Suite (file) | What it covers |
|--------------|----------------|
| test_smoke.cpp | version constants, snapshot build, local discovery, isolated discovery |
| test_graph.cpp | parents/children, ancestors/descendants, siblings, root_ancestor, neighbors |
| test_identity.cpp | hex round-trip, rejection of bad hex, deterministic derivation, ordering/hash, native canonical key |
| test_merge.cpp | same-PCI unification, high-confidence-wins, edge resolution, conflict reporting |
| test_path.cpp | same-object, direct path, no-route, lowest-cost preference, invalid endpoint |
| test_ranking.cpp | two-GPUs same root, cross-root preference, accelerator-to-accelerator peer, determinism |
| test_cost.cpp | cost composition, total matches sum, configurable weights, determinism |
| test_serialization.cpp | snapshot round-trip, compact/pretty, rejects malformed, rejects oversized, property round-trip |
| test_snapshot.cpp | build+validate, immutable publication, lookup-missing throws, edge out-of-range throws, bounds reject |
| test_diff.cpp | node/edge added-removed, property change not material, edge property material |
| test_concurrency.cpp | parallel queries, refresh-while-querying |
| test_property.cpp | deterministic PRNG property tests |
| test_adversarial.cpp | duplicate ids, oversized counts, malformed JSON, bogus BDF/NUMA, negative/NaN cost, forged/stale ids, deep nesting |
| test_import.cpp | marks synthetic, rejects malformed/wrong-format/empty, two-GPU topology import |
| test_validation.cpp | dangling edge, containment cycle, self-containment, duplicate native identity, unknown-type warns |

## Property tests with deterministic PRNG

tf_test_util.hpp provides a splitmix64 PRNG class so property tests are reproducible. The
suites:

- property_random_valid_graphs — 400 random connected graphs; asserts validation ok, node/edge
  count consistency, deterministic serialization round-trip, and correct path endpoints.
- property_random_invalid_flagged — 200 random graphs with mostly-tangled CONTAINS edges; asserts
  take() never crashes and validation can be inspected.
- property_large_bounded_graph — a bounded 2000-node / 3000-edge graph; asserts no crash, correct
  node count, and a successful serialize/deserialize round trip.

## Adversarial tests

test_adversarial.cpp targets the security posture (see SECURITY.md):

- duplicate node ids are rejected (second add_node returns false).
- oversized counts hit max_nodes.
- malformed JSON (wrong format, non-array nodes, empty node object) throws TopologyError.
- a node with a bogus NUMA index and BDF is representable and does not crash queries.
- negative and NaN cost weights do not crash or infinite-loop lowest_cost_path / path_cost /
  explain.
- forged and stale ids return not-found for find_node / node, and shortest_path returns
  found=false.
- deeply nested JSON is rejected without stack overflow.

## Concurrency tests

test_concurrency.cpp runs parallel queries over a snapshot and also refreshes the runtime while
other threads query it. This validates the lock-free read path: readers observe either the old or
the new immutable snapshot, never a torn one.

## No test timeouts

The harness imposes no wall-clock deadline per test. Property and adversarial tests are bounded in
iteration count, so the suite finishes deterministically. The absence of a timeout is intentional:
tests are expected to complete, and any crash or hang is a real regression rather than something to
be masked by a deadline.

## Coverage focus

The suite concentrates on: identity/determinism, merge precedence and conflicts, path and cost
correctness, serialization round-trip and rejection, snapshot immutability and bounds, diffing and
material-change rules, validation invariants, concurrency safety, and adversarial robustness. Real
discovery is exercised in test_smoke (smoke_local_discovery) but is host-dependent; the bulk of
deterministic coverage uses synthetic topologies built through tf_test_util::build_snapshot.