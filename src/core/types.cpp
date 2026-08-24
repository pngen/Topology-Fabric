
#include "topology_fabric/types.hpp"
#include <array>

namespace topology_fabric {

std::string_view to_string(NodeType t) noexcept {
  switch (t) {
    case NodeType::MACHINE: return "machine";
    case NodeType::CPU_PACKAGE: return "cpu_package";
    case NodeType::CPU_CORE: return "cpu_core";
    case NodeType::CPU_THREAD: return "cpu_thread";
    case NodeType::NUMA_NODE: return "numa_node";
    case NodeType::HOST_MEMORY_DOMAIN: return "host_memory_domain";
    case NodeType::ACCELERATOR: return "accelerator";
    case NodeType::ACCELERATOR_MEMORY_DOMAIN: return "accelerator_memory_domain";
    case NodeType::PCI_ROOT: return "pci_root";
    case NodeType::PCI_BRIDGE: return "pci_bridge";
    case NodeType::PCI_DEVICE: return "pci_device";
    case NodeType::NETWORK_INTERFACE: return "network_interface";
    case NodeType::STORAGE_DEVICE: return "storage_device";
    case NodeType::SHARED_MEMORY_DOMAIN: return "shared_memory_domain";
    case NodeType::UNKNOWN: return "unknown";
    case NodeType::EXTENSION: return "extension";
  }
  return "unknown";
}

NodeType node_type_from_string(std::string_view s) noexcept {
  if (s == "machine") return NodeType::MACHINE;
  if (s == "cpu_package") return NodeType::CPU_PACKAGE;
  if (s == "cpu_core") return NodeType::CPU_CORE;
  if (s == "cpu_thread") return NodeType::CPU_THREAD;
  if (s == "numa_node") return NodeType::NUMA_NODE;
  if (s == "host_memory_domain") return NodeType::HOST_MEMORY_DOMAIN;
  if (s == "accelerator") return NodeType::ACCELERATOR;
  if (s == "accelerator_memory_domain") return NodeType::ACCELERATOR_MEMORY_DOMAIN;
  if (s == "pci_root") return NodeType::PCI_ROOT;
  if (s == "pci_bridge") return NodeType::PCI_BRIDGE;
  if (s == "pci_device") return NodeType::PCI_DEVICE;
  if (s == "network_interface") return NodeType::NETWORK_INTERFACE;
  if (s == "storage_device") return NodeType::STORAGE_DEVICE;
  if (s == "shared_memory_domain") return NodeType::SHARED_MEMORY_DOMAIN;
  if (s == "extension") return NodeType::EXTENSION;
  return NodeType::UNKNOWN;
}

std::string_view to_string(EdgeType t) noexcept {
  switch (t) {
    case EdgeType::CONTAINS: return "contains";
    case EdgeType::ATTACHED_TO: return "attached_to";
    case EdgeType::CONNECTED_TO: return "connected_to";
    case EdgeType::LOCAL_TO: return "local_to";
    case EdgeType::PEER_TO: return "peer_to";
    case EdgeType::SHARES_PARENT: return "shares_parent";
    case EdgeType::SHARES_NUMA: return "shares_numa";
    case EdgeType::ROUTES_THROUGH: return "routes_through";
    case EdgeType::ACCESSIBLE_FROM: return "accessible_from";
    case EdgeType::AFFINE_TO: return "affine_to";
  }
  return "connected_to";
}

EdgeType edge_type_from_string(std::string_view s) noexcept {
  if (s == "contains") return EdgeType::CONTAINS;
  if (s == "attached_to") return EdgeType::ATTACHED_TO;
  if (s == "connected_to") return EdgeType::CONNECTED_TO;
  if (s == "local_to") return EdgeType::LOCAL_TO;
  if (s == "peer_to") return EdgeType::PEER_TO;
  if (s == "shares_parent") return EdgeType::SHARES_PARENT;
  if (s == "shares_numa") return EdgeType::SHARES_NUMA;
  if (s == "routes_through") return EdgeType::ROUTES_THROUGH;
  if (s == "accessible_from") return EdgeType::ACCESSIBLE_FROM;
  if (s == "affine_to") return EdgeType::AFFINE_TO;
  return EdgeType::CONNECTED_TO;
}

std::string_view to_string(EdgeDirection d) noexcept {
  switch (d) {
    case EdgeDirection::DIRECTED: return "directed";
    case EdgeDirection::UNDIRECTED: return "undirected";
  }
  return "directed";
}

EdgeDirection edge_direction_from_string(std::string_view s) noexcept {
  if (s == "undirected") return EdgeDirection::UNDIRECTED;
  return EdgeDirection::DIRECTED;
}

std::string_view to_string(ProvenanceKind k) noexcept {
  switch (k) {
    case ProvenanceKind::DISCOVERED: return "discovered";
    case ProvenanceKind::INFERRED: return "inferred";
    case ProvenanceKind::MEASURED: return "measured";
    case ProvenanceKind::USER_SUPPLIED: return "user_supplied";
    case ProvenanceKind::UNKNOWN: return "unknown";
  }
  return "unknown";
}

ProvenanceKind provenance_kind_from_string(std::string_view s) noexcept {
  if (s == "discovered") return ProvenanceKind::DISCOVERED;
  if (s == "inferred") return ProvenanceKind::INFERRED;
  if (s == "measured") return ProvenanceKind::MEASURED;
  if (s == "user_supplied") return ProvenanceKind::USER_SUPPLIED;
  return ProvenanceKind::UNKNOWN;
}

std::string_view to_string(Confidence c) noexcept {
  switch (c) {
    case Confidence::AUTHORITATIVE: return "authoritative";
    case Confidence::HIGH: return "high";
    case Confidence::MEDIUM: return "medium";
    case Confidence::LOW: return "low";
    case Confidence::UNKNOWN: return "unknown";
  }
  return "unknown";
}

Confidence confidence_from_string(std::string_view s) noexcept {
  if (s == "authoritative") return Confidence::AUTHORITATIVE;
  if (s == "high") return Confidence::HIGH;
  if (s == "medium") return Confidence::MEDIUM;
  if (s == "low") return Confidence::LOW;
  return Confidence::UNKNOWN;
}

std::string_view to_string(LocalityClass c) noexcept {
  switch (c) {
    case LocalityClass::EXACT: return "exact";
    case LocalityClass::SAME_CORE: return "same_core";
    case LocalityClass::SAME_PACKAGE: return "same_package";
    case LocalityClass::SAME_NUMA: return "same_numa";
    case LocalityClass::SAME_ROOT_COMPLEX: return "same_root_complex";
    case LocalityClass::SAME_HOST: return "same_host";
    case LocalityClass::REMOTE: return "remote";
    case LocalityClass::UNKNOWN: return "unknown";
  }
  return "unknown";
}

LocalityClass locality_from_string(std::string_view s) noexcept {
  if (s == "exact") return LocalityClass::EXACT;
  if (s == "same_core") return LocalityClass::SAME_CORE;
  if (s == "same_package") return LocalityClass::SAME_PACKAGE;
  if (s == "same_numa") return LocalityClass::SAME_NUMA;
  if (s == "same_root_complex") return LocalityClass::SAME_ROOT_COMPLEX;
  if (s == "same_host") return LocalityClass::SAME_HOST;
  if (s == "remote") return LocalityClass::REMOTE;
  return LocalityClass::UNKNOWN;
}

std::string_view to_string(PathClass c) noexcept {
  switch (c) {
    case PathClass::SAME_OBJECT: return "same_object";
    case PathClass::SAME_PROCESSOR: return "same_processor";
    case PathClass::SAME_NUMA: return "same_numa";
    case PathClass::SAME_PCI_DEVICE: return "same_pci_device";
    case PathClass::SAME_PCI_BRIDGE: return "same_pci_bridge";
    case PathClass::SAME_ROOT_COMPLEX: return "same_root_complex";
    case PathClass::HOST_TO_ACCELERATOR: return "host_to_accelerator";
    case PathClass::ACCELERATOR_TO_ACCELERATOR: return "accelerator_to_accelerator";
    case PathClass::HOST_TO_STORAGE: return "host_to_storage";
    case PathClass::HOST_TO_NETWORK: return "host_to_network";
    case PathClass::CROSS_NUMA: return "cross_numa";
    case PathClass::REMOTE: return "remote";
    case PathClass::EXTERNAL: return "external";
    case PathClass::UNKNOWN: return "unknown";
  }
  return "unknown";
}

PathClass path_class_from_string(std::string_view s) noexcept {
  if (s == "same_object") return PathClass::SAME_OBJECT;
  if (s == "same_processor") return PathClass::SAME_PROCESSOR;
  if (s == "same_numa") return PathClass::SAME_NUMA;
  if (s == "same_pci_device") return PathClass::SAME_PCI_DEVICE;
  if (s == "same_pci_bridge") return PathClass::SAME_PCI_BRIDGE;
  if (s == "same_root_complex") return PathClass::SAME_ROOT_COMPLEX;
  if (s == "host_to_accelerator") return PathClass::HOST_TO_ACCELERATOR;
  if (s == "accelerator_to_accelerator") return PathClass::ACCELERATOR_TO_ACCELERATOR;
  if (s == "host_to_storage") return PathClass::HOST_TO_STORAGE;
  if (s == "host_to_network") return PathClass::HOST_TO_NETWORK;
  if (s == "cross_numa") return PathClass::CROSS_NUMA;
  if (s == "remote") return PathClass::REMOTE;
  if (s == "external") return PathClass::EXTERNAL;
  return PathClass::UNKNOWN;
}

std::string_view to_string(FactClass c) noexcept {
  switch (c) {
    case FactClass::STATIC: return "static";
    case FactClass::SEMI_STATIC: return "semi_static";
    case FactClass::DYNAMIC: return "dynamic";
  }
  return "static";
}

FactClass fact_class_from_string(std::string_view s) noexcept {
  if (s == "dynamic") return FactClass::DYNAMIC;
  if (s == "semi_static") return FactClass::SEMI_STATIC;
  return FactClass::STATIC;
}

std::string_view to_string(DiffEventKind k) noexcept {
  switch (k) {
    case DiffEventKind::NODE_ADDED: return "node_added";
    case DiffEventKind::NODE_REMOVED: return "node_removed";
    case DiffEventKind::EDGE_ADDED: return "edge_added";
    case DiffEventKind::EDGE_REMOVED: return "edge_removed";
    case DiffEventKind::PROPERTY_CHANGED: return "property_changed";
    case DiffEventKind::CAPABILITY_CHANGED: return "capability_changed";
    case DiffEventKind::LOCALITY_CHANGED: return "locality_changed";
    case DiffEventKind::PROVIDER_CHANGED: return "provider_changed";
  }
  return "property_changed";
}

DiffEventKind diff_event_kind_from_string(std::string_view s) noexcept {
  if (s == "node_added") return DiffEventKind::NODE_ADDED;
  if (s == "node_removed") return DiffEventKind::NODE_REMOVED;
  if (s == "edge_added") return DiffEventKind::EDGE_ADDED;
  if (s == "edge_removed") return DiffEventKind::EDGE_REMOVED;
  if (s == "property_changed") return DiffEventKind::PROPERTY_CHANGED;
  if (s == "capability_changed") return DiffEventKind::CAPABILITY_CHANGED;
  if (s == "locality_changed") return DiffEventKind::LOCALITY_CHANGED;
  if (s == "provider_changed") return DiffEventKind::PROVIDER_CHANGED;
  return DiffEventKind::PROPERTY_CHANGED;
}

}  // namespace topology_fabric
