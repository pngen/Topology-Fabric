
// TopologyFabric/types.hpp - core enumerated types and categorical vocabulary.
#pragma once
#include <string>
#include <string_view>
#include <optional>
#include <cstdint>

namespace topology_fabric {

// Node category of a discovered hardware/abstract resource.
enum class NodeType : uint8_t {
  MACHINE = 0,
  CPU_PACKAGE,
  CPU_CORE,
  CPU_THREAD,
  NUMA_NODE,
  HOST_MEMORY_DOMAIN,
  ACCELERATOR,
  ACCELERATOR_MEMORY_DOMAIN,
  PCI_ROOT,
  PCI_BRIDGE,
  PCI_DEVICE,
  NETWORK_INTERFACE,
  STORAGE_DEVICE,
  SHARED_MEMORY_DOMAIN,
  UNKNOWN,
  EXTENSION
};

enum class EdgeType : uint8_t {
  CONTAINS = 0,
  ATTACHED_TO,
  CONNECTED_TO,
  LOCAL_TO,
  PEER_TO,
  SHARES_PARENT,
  SHARES_NUMA,
  ROUTES_THROUGH,
  ACCESSIBLE_FROM,
  AFFINE_TO
};

enum class EdgeDirection : uint8_t {
  DIRECTED = 0,    // source -> target only
  UNDIRECTED       // symmetric
};

// How a fact was established.
enum class ProvenanceKind : uint8_t {
  DISCOVERED = 0,   // directly observed via an API/platform call
  INFERRED,         // derived from other facts / reasoning
  MEASURED,         // produced by a measurement
  USER_SUPPLIED,    // supplied by a caller/config
  UNKNOWN
};

// Explicit confidence, never silently upgraded.
enum class Confidence : uint8_t {
  AUTHORITATIVE = 0,
  HIGH,
  MEDIUM,
  LOW,
  UNKNOWN
};

enum class LocalityClass : uint8_t {
  EXACT = 0,
  SAME_CORE,
  SAME_PACKAGE,
  SAME_NUMA,
  SAME_ROOT_COMPLEX,
  SAME_HOST,
  REMOTE,
  UNKNOWN
};

enum class PathClass : uint8_t {
  SAME_OBJECT = 0,
  SAME_PROCESSOR,
  SAME_NUMA,
  SAME_PCI_DEVICE,
  SAME_PCI_BRIDGE,
  SAME_ROOT_COMPLEX,
  HOST_TO_ACCELERATOR,
  ACCELERATOR_TO_ACCELERATOR,
  HOST_TO_STORAGE,
  HOST_TO_NETWORK,
  CROSS_NUMA,
  REMOTE,
  EXTERNAL,
  UNKNOWN
};

// Classification of a topology fact with respect to how volatile it is.
enum class FactClass : uint8_t {
  STATIC = 0,
  SEMI_STATIC,
  DYNAMIC
};

enum class DiffEventKind : uint8_t {
  NODE_ADDED = 0,
  NODE_REMOVED,
  EDGE_ADDED,
  EDGE_REMOVED,
  PROPERTY_CHANGED,
  CAPABILITY_CHANGED,
  LOCALITY_CHANGED,
  PROVIDER_CHANGED
};

// Enum <-> string conversions. Unknown inputs map to a default and are never fatal.
std::string_view to_string(NodeType t) noexcept;
NodeType node_type_from_string(std::string_view s) noexcept;
std::string_view to_string(EdgeType t) noexcept;
EdgeType edge_type_from_string(std::string_view s) noexcept;
std::string_view to_string(EdgeDirection t) noexcept;
EdgeDirection edge_direction_from_string(std::string_view s) noexcept;
std::string_view to_string(ProvenanceKind k) noexcept;
ProvenanceKind provenance_kind_from_string(std::string_view s) noexcept;
std::string_view to_string(Confidence c) noexcept;
Confidence confidence_from_string(std::string_view s) noexcept;
std::string_view to_string(LocalityClass c) noexcept;
LocalityClass locality_from_string(std::string_view s) noexcept;
std::string_view to_string(PathClass c) noexcept;
PathClass path_class_from_string(std::string_view s) noexcept;
std::string_view to_string(FactClass c) noexcept;
FactClass fact_class_from_string(std::string_view s) noexcept;
std::string_view to_string(DiffEventKind k) noexcept;
DiffEventKind diff_event_kind_from_string(std::string_view s) noexcept;

}  // namespace topology_fabric