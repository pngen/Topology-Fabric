# Contributing

Thank you for contributing to Topology Fabric. This guide covers building, testing, code style, and
the contribution process for this repository.

## License

All contributions are licensed under the Apache License 2.0. By contributing, you agree that your
work is offered under that license. See LICENSE for the full text (Copyright 2026 Summon Software
Labs). Do not submit code that you are not licensed to contribute.

## Requirements

- CMake 3.24 or newer.
- A C++20 compiler. The project is validated on MSVC 19.44 (Visual Studio 2022).
- No build-time CUDA toolchain is required; the CUDA provider dynamically loads nvcuda.dll.
- Windows only for the built-in providers; the build still succeeds on other platforms with
  providers reporting "not implemented".

## Building

From the repository root:

    cmake -S . -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release
    cmake --build build --config Debug

This produces:

- topology_fabric static library (TopologyFabric::topology_fabric).
- topology-fabric CLI.
- tf_tests test executable.
- 14 ex_* examples.
- 2 tf_bench_* benchmarks.

## CMake options

| Option | Default | Description |
|--------|---------|-------------|
| TOPOLOGY_FABRIC_BUILD_TESTS | ON | build the test suite |
| TOPOLOGY_FABRIC_BUILD_EXAMPLES | ON | build the examples |
| TOPOLOGY_FABRIC_BUILD_BENCHMARKS | ON | build the benchmarks |
| TOPOLOGY_FABRIC_ENABLE_CUDA | ON | compile in the CUDA provider (TOPOLOGY_FABRIC_HAS_CUDA) |
| TOPOLOGY_FABRIC_WARNINGS_AS_ERRORS | ON | treat warnings as errors (/WX or -Werror) |

## Running the tests

    ctest --test-dir build -C Release
    # or directly
    build/tests/Release/tf_tests.exe

There are 67 test cases. A clean run must print "0 failure(s)" and exit 0. The suite includes
property tests (deterministic PRNG), adversarial tests, and concurrency tests; there are no test
timeouts.

## Coding style

- C++20. No exceptions for control flow in performance paths beyond the documented Result<T> and
  TopologyError usage.
- Namespace topology_fabric. Public headers declare only the intended ABI; implementation details
  live under src/ (and in src/internal.hpp for shared helpers).
- Keep the public headers self-contained (each includes what it needs).
- Prefer value semantics and immutability for snapshot data; build immutable snapshots with
  SnapshotBuilder and publish via atomic shared_ptr.
- Bounds are mandatory on any input surface (nodes, edges, strings, JSON). If you add a parser or a
  new unbounded container, bound it.
- Add a Provenance and a Confidence to every nontrivial fact. Never silently upgrade authority;
  record a conflict instead.
- Deterministic iteration: use std::map for property maps, and do not rely on unordered-memory
  iteration order for serialization or diffing.
- Keep enums in types.hpp; add any new enum string conversions there (to_string / *_from_string).

## Warning policy

The build compiles with /W4 /WX on MSVC and -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
on GCC/Clang, with a small MSVC suppression list for known-benign system/CUDA-header codes. A pull
request must build with **zero warnings**. If you need to suppress a specific warning, justify it in
the CMake list rather than disabling the policy globally.

## Adding a provider

1. Add src/providers/<name>.cpp with a TopologyProvider subclass: name(), version(), discover().
2. Add a factory to src/providers/provider_factories.hpp and the .cpp.
3. Register it in TopologyRuntime::register_builtin_providers() (runtime.cpp).
4. If it is platform-specific, return partial=true / success=false with a clear warning on the
   unsupported platform, never fabricate data.
5. Read every value through a real API; add provenance and confidence. Mark partial where the
   platform may not report a value.
6. Add tests: at minimum a merge/identity test if it overlaps other providers, and a serialization
   round-trip check.

## Adding a query or cost term

1. Add the function/struct to query.hpp and implement in the matching src/core/*.cpp.
2. If you add a CostWeights field, update the defaults, edge_traversal_cost, and cost_breakdown, and
   add a serialization/persistence path if appropriate.
3. Add tests that assert determinism and that a monotone policy stays behave.

## Documentation

Add or update the matching doc in the repository root (ARCHITECTURE, DESIGN, GRAPH_MODEL,
DISCOVERY, PROVIDERS, CPU_NUMA, PCIE, CUDA, PATHS, COST_MODEL, PROVENANCE, CONFIDENCE, SNAPSHOTS,
SERIALIZATION, SECURITY, BENCHMARKS, TESTING, INTEROP, LIMITATIONS). A doc must match the actual
implementation; read the header and the .cpp before writing.

## Pull request checklist

- Builds clean in Debug and Release with zero warnings.
- All 67 existing tests pass; new tests cover your change.
- Public headers are self-contained; no new unbounded inputs.
- Determinism preserved (identical inputs yield identical outputs).
- Provenance / confidence added for any new fact.
- README and relevant docs updated.
- No breaking ABI change to the public query surface without a version bump.