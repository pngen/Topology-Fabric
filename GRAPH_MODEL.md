# Graph Model

Topology Fabric models the fabric as a **typed property graph**. Every discovered resource is a
node; every relationship is an edge. This document enumerates the exact vocabulary implemented in
the public headers (`types.hpp`, `node.hpp`, `edge.hpp`, `capability.hpp`).

## Nodes

A node is a `TopologyNode` (defined in `node.hpp`). Its fields are:

| Field | Type | Meaning |
|-------|------|---------|
| `id` | `TopologyNodeId` | 128-bit deterministic identity |
| `type` | `NodeType` | the resource category |
| `category` | `std::string` | extension / unknown category name |
| `name` | `std::string` | compact name |
| `display_name` | `std::string` | human-readable name (falls back to id hex) |
| `native` | `NativeIdentity` | platform-reported identity |
| `capabilities` | `Capability` | bitmask of capabilities |
| `properties` | `PropertyMap` | ordered string->value map |
| `provenance` | `Provenance` | how/where/when this fact was established |
| `confidence` | `Confidence` | certainty |
| `synthetic` | `bool` | true if imported/synthetic (clearly marked) |

### NodeType

The complete `NodeType` enum (`uint8_t`), in declaration order:

| Value | Enumerator | Meaning |
|-------|-----------|---------|
| 0 | `MACHINE` | the host / machine root |
| 1 | `CPU_PACKAGE` | a physical CPU package/socket |
| 2 | `CPU_CORE` | a physical CPU core |
| 3 | `CPU_THREAD` | a logical processor (hardware thread) |
| 4 | `NUMA_NODE` | a NUMA node |
| 5 | `HOST_MEMORY_DOMAIN` | host memory assigned to a NUMA node |
| 6 | `ACCELERATOR` | an accelerator (e.g. a CUDA GPU) |
| 7 | `ACCELERATOR_MEMORY_DOMAIN` | accelerator-local memory (e.g. VRAM) |
| 8 | `PCI_ROOT` | a root PCI entity (topmost PCI node) |
| 9 | `PCI_BRIDGE` | an intermediate PCI bridge |
| 10 | `PCI_DEVICE` | a PCI leaf/endpoint |
| 11 | `NETWORK_INTERFACE` | a network interface |
| 12 | `STORAGE_DEVICE` | a storage device/volume |
| 13 | `SHARED_MEMORY_DOMAIN` | a shared-memory domain |
| 14 | `UNKNOWN` | untyped node |
| 15 | `EXTENSION` | named extension node (category carries the name) |

`NodeType` is converted to/from strings via `to_string(NodeType)` /
`node_type_from_string(std::string_view)`. Unknown inputs map to a safe default and are never
fatal.

### NativeIdentity

`NativeIdentity` (in `identity.hpp`) holds the platform-reported identity. Fields are grouped
by domain and are `std::optional` when a value may not be reported; `std::nullopt` means
"not reported", never zero.

- **PCI**: `pci_domain`, `pci_bus`, `pci_device`, `pci_function`, `vendor_id`,
  `device_id`, `subsystem_id`, `subsystem_vendor_id` (all `uint16_t`).
- **CPU / NUMA**: `numa_node`, `cpu_package`, `core_id`, `processor_group`,
  `logical_processor_index` (all `uint32_t`).
- **Accelerator**: `cuda_ordinal` (`uint32_t`), `cuda_uuid` (`std::string`), `name`.
- **Network / storage**: `network_interface_name`, `network_hardware_id`, `storage_id`,
  `storage_device_path`.
- **Machine**: `machine_name`, `os_version`.

Helpers: `has_pci()` is true when `pci_bus`, `pci_device`, and `pci_function` are all set.
`pci_bdf_string()` only produces a BDF when all three are known and never fabricates one
(otherwise it returns the literal "unknown"). `canonical_key()` is the merge identity.

### Capability

`Capability` (in `capability.hpp`) is a `uint64_t` bitmask. Bits:

| Bit | Enumerator | Meaning |
|-----|-----------|---------|
| 0 | `CPU_ADDRESSABLE` | addressable by the CPU |
| 1 | `DEVICE_ADDRESSABLE` | addressable by the device |
| 2 | `DMA_CAPABLE` | capable of DMA |
| 3 | `PEER_ACCESS` | peer (device-to-device) access |
| 4 | `DIRECT_TRANSFER` | direct transfer |
| 5 | `STAGED_TRANSFER` | staged transfer |
| 6 | `SHARED_MEMORY` | shared memory |
| 7 | `PERSISTENT` | persistent |
| 8 | `COHERENT` | coherent access |
| 9 | `NUMA_LOCAL` | NUMA-local |
| 10 | `PCI_CONNECTED` | connected via PCI |
| 11 | `NETWORK_CONNECTED` | connected via network |
| 12 | `STORAGE_BACKED` | backed by storage |

Helpers: `operator|`, `operator&`, `operator~`, `has(set, flag)`, `add(set, flag)`,
`to_uint(Capability)`, `capability_from_uint(uint64_t)`. `TopologyNode::has_capability(c)`
tests a single bit.

### PropertyValue

`PropertyValue` (in `value.hpp`) is a bounded variant over
`nullptr_t | bool | int64_t | uint64_t | double | string | vector<string> | vector<int64_t>`.
Bounds: `kMaxStringBytes` = 1 MiB per string, `kMaxVectorItems` = 65,536 items. `PropertyMap`
is `std::map<std::string, PropertyValue>`, so iteration order is deterministic.

## Edges

An edge is a `TopologyEdge` (in `edge.hpp`). Fields: `source`, `target`, `type`,
`direction`, `provenance`, `confidence`, plus optional link facts (`width` lanes,
`pcie_generation`, `bandwidth_bytes_per_sec`, `latency_ns`), `hop_count`,
`locality_score`, `accessible`, `peer_capability`, `policy_version`, and `properties`.

### EdgeType

The complete `EdgeType` enum (`uint8_t`):

| Value | Enumerator | Meaning |
|-------|-----------|---------|
| 0 | `CONTAINS` | parent contains child (strict DAG) |
| 1 | `ATTACHED_TO` | attached to |
| 2 | `CONNECTED_TO` | connected to |
| 3 | `LOCAL_TO` | local to (e.g. thread is local to a NUMA node) |
| 4 | `PEER_TO` | peer to (e.g. two GPUs with peer access) |
| 5 | `SHARES_PARENT` | shares a parent |
| 6 | `SHARES_NUMA` | shares a NUMA node |
| 7 | `ROUTES_THROUGH` | routes through |
| 8 | `ACCESSIBLE_FROM` | accessible from |
| 9 | `AFFINE_TO` | affine to |

### EdgeDirection

`DIRECTED` (source -> target only) or `UNDIRECTED` (symmetric). For undirected edges, the
`key()` helper stores source/target in canonical order (`source <= target`) so that
deduplication and diffing are order-insensitive.

### Edge key

`TopologyEdge::Key` is `{ source, target, type }`. `TopologyEdge::key()` returns the
canonical form (source/target swapped for undirected so the key is symmetric). This key is used for
edge deduplication in the merge, for diffing, and as a hash-map key.

## Graph invariants (validation)

`SnapshotBuilder::validate(nodes, edges)` checks these invariants and reports them in a
`ValidationResult` (`ok`, `errors`, `warnings`):

1. **No dangling edges** — every edge source/target must exist (error).
2. **No duplicate native identity** — two nodes must not share a non-empty canonical key (error).
3. **No self-containment** — a `CONTAINS` edge must not have source == target (error).
4. **No containment cycles** — the `CONTAINS` subgraph must be a strict DAG; a Kahn
   topological sort detects cycles (error).
5. **Symmetric PEER_TO** — a directed `PEER_TO` edge without its reverse is a warning.
6. **Accelerator PCI binding** — an accelerator claiming a PCI identity with no matching node is
   a soft warning.
7. **Unknown untyped node** — a node with type `UNKNOWN` and no category is a warning.

## Relationship semantics used by queries

The query layer uses the edge vocabulary to build paths and classifications. The
`HardwareClass` mapping (in `src/internal.hpp`) groups node types into CPU, ACCELERATOR, PCI,
MEMORY, STORAGE, NETWORK, MACHINE, OTHER. `detail::collocation_edge` treats `SHARES_PARENT`
and `SHARES_NUMA` as collocation edges that are only traversed when an explicit collocation
request is made.

The result is a graph you can traverse with the helpers in `query.hpp`: `parents`,
`children`, `ancestors`, `descendants`, `siblings`, `neighbors`, and `root_ancestor`.
