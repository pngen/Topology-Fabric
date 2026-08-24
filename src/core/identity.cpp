
#include "topology_fabric/identity.hpp"
#include <sstream>
#include <iomanip>

namespace topology_fabric {

std::string NativeIdentity::pci_bdf_string() const {
  // Only report a BDF when all components are actually known. Never fabricate one.
  if (!pci_bus || !pci_device || !pci_function) return std::string("unknown");
  uint16_t domain = pci_domain.value_or(0);
  uint16_t bus = *pci_bus;
  uint16_t dev = *pci_device;
  uint16_t fn = *pci_function;
  std::ostringstream os;
  os << std::hex << std::setfill('0')
     << std::setw(4) << domain << ':'
     << std::setw(2) << bus << ':'
     << std::setw(2) << dev << '.'
     << std::setw(1) << fn;
  return os.str();
}

std::string NativeIdentity::canonical_key() const {
  // Prefer the most stable, cross-provider native identity.
  if (has_pci()) {
    return std::string("pci:") + pci_bdf_string();
  }
  if (!storage_id.empty()) return std::string("storage:") + storage_id;
  if (!storage_device_path.empty()) return std::string("storage:") + storage_device_path;
  if (!network_hardware_id.empty()) return std::string("net:") + network_hardware_id;
  if (!network_interface_name.empty()) return std::string("net:") + network_interface_name;
  if (!cuda_uuid.empty()) return std::string("cuda:") + cuda_uuid;
  if (cuda_ordinal) return std::string("cuda-ord:") + std::to_string(*cuda_ordinal);
  if (!machine_name.empty()) return std::string("machine:") + machine_name;
  return std::string{};
}

}  // namespace topology_fabric