# Limitations

This document is an honest account of what Topology Fabric 1.0.0 does and does not do, so that a
consumer can judge how much to trust a snapshot. Each item below is a real, observed constraint.

## 1. PCI device-tree BDF is not always exposed on Windows

The PCI provider reads a device BDF via CM_Get_DevNode_PropertyW for DEVPKEY_Device_LocationInfo
and the LocationPaths property. On the validated host both properties returned CR_NOT_FOUND, so the
BDF for the NVIDIA GPU was not surfaced. When the BDF is unavailable, NativeIdentity::pci_bdf_string()
returns "unknown" instead of fabricating one, and canonical_key() falls through to the next-best
identity. The practical result is that the CUDA GPU node and its OS PCI node may remain two separate
nodes rather than being unified by BDF.

This is a **partial-discovery outcome, not a bug**: the runtime honestly reports that the BDF is
unknown and does not invent one. On a host where Configuration Manager exposes LocationInfo, the two
nodes would collapse under the shared PCI BDF canonical key.

## 2. Single-GPU host — no multi-GPU hardware validation

The validated hardware has exactly one NVIDIA GPU. Consequently the peer-access matrix is trivially
1x1 with no P2P edges, and multi-GPU behavior (peer edges, cross-root ranking, multi-GPU locality)
is only validated **synthetically** via test_import (import_two_gpu_topology), test_ranking
(ranking_two_gpus_same_root, ranking_cross_root_prefers_same_root, ranking_acl_to_acl_peer), and the
example ex_multigpu.cpp. There is no real multi-GPU hardware validation behind these results.

## 3. Topology measurements are limited to a host-memcpy baseline

There is no live interconnect measurement in 1.0.0. The only measurement primitive is the CLI
"measure" command, which runs a local host-memcpy benchmark (32 MiB buffer, 40 iterations,
reporting GiB/s). It is observational and does not touch or mutate the topology graph. Any
bandwidth/latency reported by path queries is either a provider-supplied value or a conservative
class default (see COST_MODEL.md), not a runtime measurement of a real link.

## 4. PCI link width/speed are default estimates, not measured

The PCI provider does not read link width or link generation from Configuration Manager (or any
other source). A TopologyEdge worth of width / pcie_generation is left unset, and the cost model
falls back to conservative default bandwidth/latency by device class (internal.hpp). These are
nominal estimates, not measured link speeds, so path cost should be treated as a relative ranking
signal rather than an absolute bandwidth prediction.

## 5. Only Windows providers are implemented

The host, cpu_numa, pci, cuda, storage, and network providers are Windows-only. On a non-Windows
build, each returns a partial, unsuccessful contribution with a clear "only implemented on Windows"
warning. There is no ROCm/HIP provider, no Level Zero provider, no Metal provider, no NVML provider,
no Vulkan provider, no CXL provider, no RDMA/InfiniBand provider, and no Linux sysfs-based provider
yet. Non-Windows topologies cannot currently be discovered by the built-in providers.

## 6. Dynamic link / contention facts are out of scope

Topology Fabric models static and semi-static structure. It does not measure or model live link
contention, dynamic bandwidth, fluctuating NUMA distance, or runtime link state. Edges are labeled
accessible / not accessible and carry nominal bandwidth/latency at best. The FactClass enum
(STATIC, SEMI_STATIC, DYNAMIC) is defined, but the built-in providers only produce static facts.

## Additional notes

- The CUDA provider reads device memory via the driver API but gates implausible values (for
  example 0xFFFFFFFF under WDDM) behind cuda.total_memory_reported /
  cuda.total_memory_unreliable rather than claiming a bad number.
- The storage provider enumerates logical volumes and does not associate a volume with its physical
  disk or PCI controller (it sets partial=true).
- The network provider enumerates adapters and does not resolve PCI/NUMA association (it sets
  partial=true).
- Snapshots can be partial; check SnapshotMetadata.partial_discovery and metadata.warnings to see
  what a discovery run could and could not observe.
- Imported/synthetic snapshots are marked synthetic=true and are not presented as discovered.