# Discovery

Discovery is the process of turning live platform facts into a topology snapshot. In Topology
Fabric 1.0.0 this is driven entirely by TopologyRuntime::discover() (see runtime.hpp and
runtime.cpp), which orchestrates the registered providers and merges their contributions.

## Entry point

TopologyRuntime::discover() is the only discovery entry point (also aliased as refresh()). It:

1. Locks discover_mu_ (discovery is serialized on one lock; readers are unaffected).
2. Increments the discovery_runs telemetry counter and records the start wall-clock time.
3. Builds a DiscoveryContext with the runtime Bounds, the start timestamp, and
   allow_measurement = false.
4. Runs every provider registered in the ProviderRegistry, in deterministic registration order.
   Each provider is called as provider->discover(ctx) and returns a Contribution. Exceptions
   thrown by a provider are caught, marked success=false / partial=true, and recorded as a
   warning. Per-provider wall-clock time and success/failure are recorded in telemetry.
5. Calls merge_contributions(contributions, bounds), which groups contributed nodes by native
   canonical key and resolves contributed edges by ref. See PROVIDERS.md.
6. Attaches any rootless resource node into the containment hierarchy. If a machine node exists,
   any ACCELERATOR, STORAGE_DEVICE, NETWORK_INTERFACE, PCI_ROOT, or PCI_DEVICE with no
   CONTAINS/ATTACHED_TO parent is given an inferred CONTAINS edge from the machine, at
   Confidence::MEDIUM. This keeps the graph connected so path and locality reasoning works.
7. Computes the machine identity and the provider=<version>;... string for metadata.
8. Builds a provisional snapshot to decide the generation: if there is a previous snapshot it
   diffs the provisional against it and increments the generation only when the diff is a
   material_change. Otherwise the generation starts at 1.
9. Builds the final snapshot, records telemetry (node/edge counts, counts by NodeType,
   validation failures, snapshot created, discovery time), and atomically publishes it via
   current_.store(...).

## Providers and what they discover

The built-in providers are registered by register_builtin_providers():

| Provider | Platform API | Primary facts |
|----------|--------------|---------------|
| host | GetComputerNameExA, RtlGetVersion, GetActiveProcessorGroupCount, GetActiveProcessorCount, GlobalMemoryStatusEx | machine node, OS version, logical/processor-group counts, memory totals |
| cpu_numa | GetLogicalProcessorInformationEx(RelationAll) | NUMA nodes, host memory domains, CPU packages, cores, threads; CONTAINS/LOCAL_TO edges |
| pci | CM_Get_Device_ID_List, CM_Get_Parent, CM_Locate_DevNode, CM_Get_DevNode_PropertyW | PCI root/bridge/endpoint hierarchy, vendor/device/subsystem ids, optional BDF |
| cuda | CUDA driver API via dynamic nvcuda.dll (cuDeviceGetCount, cuDeviceGetAttribute, ...) | GPU nodes (ordinal, UUID, PCI, compute capability, capabilities), VRAM domains, peer edges |
| storage | GetLogicalDrives, GetDiskFreeSpaceExW, GetVolumeInformationW | logical storage volumes with capacity/filesystem |
| network | GetAdaptersAddresses | network interfaces with addresses, MAC, MTU, oper-status |

Only Windows backends are implemented. On a non-Windows build each provider returns a graceful
partial=true, success=false contribution with a warning (see LIMITATIONS.md).

The cuda provider is compiled in only when TOPOLOGY_FABRIC_HAS_CUDA is defined (the
TOPOLOGY_FABRIC_ENABLE_CUDA CMake option, on by default). It dynamically loads nvcuda.dll so no
build-time CUDA toolchain is required.

## Contribution model

Providers do not mutate shared state. Each returns a Contribution:

| Field | Meaning |
|-------|---------|
| provider | provider name |
| version | provider version |
| nodes | vector of ContributedNode |
| edges | vector of ContributedEdge |
| warnings | strings captured during the run |
| partial | true when discovery was incomplete |
| success | true when the provider ran without a hard failure |

A ContributedNode carries a provider-local ref (ideally a canonical key), type, category, name,
native, capabilities, properties, provenance, and synthetic. A ContributedEdge is addressed by
from_ref / to_ref and carries type, direction, provenance, confidence, and optional link facts
(width, PCIe generation, bandwidth, latency, hop_count, peer_capability).

## Bounds during discovery

The DiscoveryContext carries the runtime Bounds. The merge enforces max_nodes / max_edges while
ingesting contributions; when a cap is hit it records a warning and stops ingesting that class.
SnapshotBuilder also enforces the same caps when nodes/edges are added and records a warning for
anything dropped.

## Partial discovery

SnapshotMetadata.partial_discovery is true if any provider reported partial. The aggregated
warnings are stored in SnapshotMetadata.warnings. This is how a consumer knows how much of the
fabric a snapshot genuinely reflects — e.g. the PCI provider may not be able to read a BDF on a
given host, or the CUDA provider may not run because the driver is absent.

## Determinism

Discovery is deterministic for identical platform state and identical provider registration:
providers run in registration order, the merge is deterministic, and node ids are derived from
canonical native identity. Enumeration order is never used as identity.

## Timing

On the validated Windows 11 AMD Ryzen host, a full local discovery completes in roughly 15-35 ms.
See BENCHMARKS.md.
