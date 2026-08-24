
// TopologyFabric/identity.hpp - preserved native hardware identity.
#pragma once
#include <cstdint>
#include <string>
#include <optional>
#include <iomanip>
#include <sstream>

namespace topology_fabric {

// Native hardware identity as reported by platform/vendor APIs. A value that is
// std::nullopt means "not reported". Transient enumeration order is never used
// as the sole identity.
struct NativeIdentity {
  // PCI identity
  std::optional<uint16_t> pci_domain;
  std::optional<uint16_t> pci_bus;
  std::optional<uint16_t> pci_device;
  std::optional<uint16_t> pci_function;
  std::optional<uint16_t> vendor_id;
  std::optional<uint16_t> device_id;
  std::optional<uint16_t> subsystem_id;
  std::optional<uint16_t> subsystem_vendor_id;

  // CPU / NUMA
  std::optional<uint32_t> numa_node;
  std::optional<uint32_t> cpu_package;
  std::optional<uint32_t> core_id;
  std::optional<uint32_t> processor_group;
  std::optional<uint32_t> logical_processor_index;

  // Accelerator
  std::optional<uint32_t> cuda_ordinal;
  std::string              cuda_uuid;
  std::string              name;

  // Network / storage
  std::string network_interface_name;  // OS-friendly name, e.g. "Ethernet"
  std::string network_hardware_id;     // e.g. a hardware link address
  std::string storage_id;              // e.g. disk model/serial
  std::string storage_device_path;     // e.g. "\\?\PhysicalDrive0"

  // Machine identity
  std::string machine_name;
  std::string os_version;

  // Generic identity key used for deterministic merge matching.
  std::string canonical_key() const;
  bool has_pci() const noexcept { return pci_bus && pci_device && pci_function; }
  std::string pci_bdf_string() const;
  bool operator==(const NativeIdentity&) const noexcept = default;
};

}  // namespace topology_fabric
