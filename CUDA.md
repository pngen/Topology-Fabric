# CUDA / NVIDIA Accelerator Discovery

This document describes how Topology Fabric discovers NVIDIA CUDA accelerators on Windows. It is
implemented in src/providers/cuda.cpp.

## Loading the driver API

The CUDA provider does **not** link against the CUDA toolkit. Instead it dynamically loads
nvcuda.dll at discovery time and resolves only the functions it needs:

- cuInit, cuDeviceGetCount, cuDeviceGet, cuDeviceGetName
- cuDeviceGetAttribute, cuDeviceTotalMem, cuDeviceComputeCapability
- cuDeviceCanAccessPeer, cuDeviceGetUuid, cuDeviceGetPCIBusId

If nvcuda.dll is absent or cuInit(0) fails, the provider marks itself unavailable
(mark_available(false)) and returns a partial, unsuccessful contribution with the warning
"CUDA driver API (nvcuda.dll) not available; CUDA discovery skipped". No device counts or
attributes are assumed.

Device attributes are read with cuDeviceGetAttribute using **stable ABI attribute ids**
(the enum CudaAttr in the file, which are the CUDA driver-API attribute constants):

- kPciBusId = 33, kPciDeviceId = 34, kPciDomainId = 50
- kComputeCapabilityMajor = 75, kComputeCapabilityMinor = 76
- kUnifiedAddressing = 41, kManagedMemory = 83
- kAsyncEngineCount = 40, kConcurrentKernels = 31
- kCanMapHostMemory = 19, kPageableMemoryAccess = 88

## Device loop

For each CUDA ordinal (0..count-1) the provider:

1. Gets the device handle (cuDeviceGet) and its name (cuDeviceGetName).
2. Reads PCI bus/device/domain attributes and cuDeviceGetPCIBusId (a string BDF).
3. Reads the device UUID via cuDeviceGetUuid and formats it as 32 lowercase hex characters.
4. Reads compute capability major/minor, total memory, unified addressing, managed memory, async
   engine count, concurrent kernels, can-map-host-memory, and pageable-memory-access.
5. Builds the device ref: "cuda:uuid:<uuid>" when a UUID is present, else "cuda:ord:<ordinal>".

### Memory sanity gate

The driver occasionally reports an implausible total-memory value (e.g. 0xFFFFFFFF) under WDDM.
To stay honest, the provider only records cuda.total_memory when the value is plausible
(> 1 GiB and < 1 TiB and != 0xFFFFFFFF). Otherwise it records cuda.total_memory_reported plus
cuda.total_memory_unreliable = true.

## Accelerator node

Each device becomes an ACCELERATOR node with provenance "cuda" /
"cuDeviceGetCount/cuDeviceGetAttribute" at Confidence::AUTHORITATIVE. Base capabilities are
DEVICE_ADDRESSABLE | DMA_CAPABLE. Additional bits are added from observed attributes:

- unified addressing -> CPU_ADDRESSABLE
- can-map-host-memory -> STAGED_TRANSFER | DIRECT_TRANSFER
- managed memory -> SHARED_MEMORY | COHERENT
- any peer access (see below) -> PEER_ACCESS

Properties recorded: cuda.ordinal, cuda.compute_capability_major/minor, cuda.total_memory (or the
reported/unreliable pair), cuda.unified_addressing, cuda.managed_memory, cuda.async_engine_count,
cuda.concurrent_kernels, cuda.can_map_host_memory, cuda.pageable_memory_access, and cuda.uuid.

## Accelerator memory domain

For each device the provider emits an ACCELERATOR_MEMORY_DOMAIN node ("<ref>:mem", named
"<devName> VRAM") with provenance inferred ("cuda", "cuDeviceGetAttribute",
Confidence::MEDIUM, "accelerator memory domain") and capabilities DEVICE_ADDRESSABLE. A CONTAINS
edge from the accelerator to the domain is added at Confidence::AUTHORITATIVE. The domain's
cuda.total_memory property mirrors the device value.

## Peer edges

The provider builds an N-by-N peer-access matrix using cuDeviceCanAccessPeer and emits an
undirected PEER_TO edge between every pairwise-accessible pair, at Confidence::AUTHORITATIVE, with
peer_capability = 1.

## Partial / synthetic note

On the validated single-GPU host the peer matrix is trivially size 1x1 with no peer edges.
Multi-GPU semantics are validated synthetically in the test/example sets, not against real
multi-GPU hardware (see LIMITATIONS.md).

## Compilation

The provider is compiled into the library only when TOPOLOGY_FABRIC_HAS_CUDA is defined (the
TOPOLOGY_FABRIC_ENABLE_CUDA CMake option, ON by default). When disabled, register_builtin_providers
skips it entirely. On non-Windows builds the provider returns partial=true, success=false with the
warning "CUDA provider only supported on Windows".
