# Topology Fabric 1.0.0

Topology Fabric is a **vendor-neutral C++20 systems runtime** for discovering, modeling,
measuring, validating, scoring, and exposing the **hardware and interconnect topology**
beneath heterogeneous AI infrastructure.

**Core question:**

> What hardware topology exists, what paths connect it, how costly are those paths, and
> what locality constraints should higher-level runtimes know before they place or move anything?

Topology Fabric answers that question. It does not copy bytes, schedule jobs, allocate
buffers, or arbitrate bandwidth.

---

## What Topology Fabric owns

Topology Fabric owns **topology knowledge**:

- hardware discovery (machine, CPU/package/core/thread, NUMA, memory domains, accelerators,
  PCIe hierarchy, storage, network interfaces)
- node/edge modeling of the graph of resources and their relationships
- path construction, classification, distance, cost estimation, and scoring
- locality classification (same core/package/NUMA/root/host)
- topology snapshots, generations, change detection and diffing
- topology validation, consistency checking, and partial-discovery honesty
- topology serialization (bounded, versioned JSON)
- provider/backend abstraction and deterministic merge
- telemetry, benchmarking, and a stable query API

## What Topology Fabric does NOT own

Topology Fabric exposes facts; it does not make placement or transfer decisions:

| System             | Owns                                  | Topology Fabric role                      |
|--------------------|---------------------------------------|-------------------------------------------|
| Transfer Fabric    | copying, staging, scheduling, retry   | provides source/destination facts         |
| Compute Fabric     | placement, job scheduling             | provides locality/distance/cost for ranking |
| Unified Buffer     | allocation, ownership, lifetime       | not involved                              |
| FlashTier          | residency/promotion, tiering          | not involved                              |
| Tensor Cache / KV   | cache admission, state lifecycle      | not involved                              |

Topology Fabric **never absorbs** the decision/execution responsibilities of those systems.

---

## Current status

Topology Fabric 1.0.0 is a working runtime. On its primary validated platform (Windows 11,
AMD Ryzen host) it performs **real** topology discovery:

- **Host**: machine identity, OS version, 16 logical processors, 8 physical cores, 1 CPU package,
  1 NUMA node + host memory domain, memory totals.
- **PCIe**: hierarchy via the Windows Configuration Manager (root/bridge/endpoint, vendor/device ids).
- **CUDA**: NVIDIA GeForce RTX 5090 discovered via the **CUDA driver API** (dynamic nvcuda.dll loading):
  ordinal 0, PCI bus 0000:01:00.0, UUID, compute capability 12.0, unified addressing, managed memory,
  async engines, pageable/host-mapping capabilities.
- **Storage**: logical volumes (C:, E:, ...) with capacity/filesystem.
- **Network**: interfaces with addresses, MAC, MTU, operational state.

Local discovery completes in ~15-35 ms. See [BENCHMARKS.md](BENCHMARKS.md) and the release notes.

> **Honesty note:** This host has a single GPU. Topology Fabric correctly produces a trivial
> single-GPU peer matrix and does **not** claim multi-GPU hardware validation. Multi-GPU
> semantics are validated synthetically (see examples/ex_multigpu.cpp and the test suite).

---

## Building

### Requirements

- CMake 3.24+
- A C++20 compiler. Validated on MSVC 19.44 (Visual Studio 2022).
- No build-time CUDA toolchain required; the CUDA provider dynamically loads the driver API.
- Windows links cfgmgr32, iphlpapi, ws2_32 automatically.

```sh
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
cmake --build build --config Debug
```

Produces: topology_fabric static lib (TopologyFabric::topology_fabric), topology-fabric CLI,
tf_tests, 14 ex_* examples, 2 tf_bench_* benchmarks. Zero warnings (/W4 /WX on MSVC).

### Run the tests

```sh
ctest --test-dir build -C Release
# or
build/tests/Release/tf_tests.exe
```

---

## CLI

```
topology-fabric info|discover|nodes|edges|devices|cpus|numa|pci|accelerators|storage|network
              path|distance|rank|explain|snapshot|diff|validate|measure|stats|selftest|benchmark [--json]
```

Examples: discover, accelerators, pci, path --from <id> --to <id>, rank --source <id>
--type accelerator, snapshot --json, validate, selftest.

---

## Library usage

```cpp
#include <topology_fabric/runtime.hpp>
using namespace topology_fabric;
TopologyRuntime rt;
rt.register_builtin_providers();
auto snap = rt.discover();
auto rank = rank_candidates(*snap, cpu, NodeType::ACCELERATOR, 0, CostWeights{});
auto p = lowest_cost_path(*snap, cpu, gpu, CostWeights{});
```

Snapshots are immutable and thread-safe; queries are lock-free on snapshot data.
Refresh publishes a new snapshot atomically.

---

## Documentation

| Document | Contents |
|----------|----------|
| ARCHITECTURE.md | architectural boundary & layering |
| DESIGN.md | design principles & trade-offs |
| GRAPH_MODEL.md | node/edge/capability model |
| DISCOVERY.md | discovery process & sources |
| PROVIDERS.md | provider interface & merge precedence |
| CPU_NUMA.md | CPU/NUMA discovery & APIs |
| PCIE.md | PCIe hierarchy discovery |
| CUDA.md | CUDA/NVIDIA accelerator discovery |
| PATHS.md | path model & classification |
| COST_MODEL.md | cost model & weights |
| PROVENANCE.md | provenance of facts |
| CONFIDENCE.md | confidence model |
| SNAPSHOTS.md | snapshots & generations |
| SERIALIZATION.md | versioned JSON serialization |
| SECURITY.md | input bounds & adversarial safety |
| BENCHMARKS.md | measured performance |
| TESTING.md | test coverage & methodology |
| INTEROP.md | integration with sibling systems |
| LIMITATIONS.md | honest limitations |
| CONTRIBUTING.md | how to contribute |

---

## License

Apache License 2.0. Copyright 2026 Summon Software Labs. No telemetry transmission.
