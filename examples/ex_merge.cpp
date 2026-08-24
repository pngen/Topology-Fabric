#include "ex_common.hpp"
#include "topology_fabric/merge.hpp"
int main() {
  // Demonstrate deterministic provider merge: two contributions describing one device.
  std::vector<topology_fabric::Contribution> cs;
  {
    topology_fabric::Contribution c; c.provider = "cuda";
    topology_fabric::ContributedNode n; n.ref="cuda:uuid:x"; n.type=topology_fabric::NodeType::ACCELERATOR; n.name="GPU";
    n.native.pci_bus=1; n.native.pci_device=0; n.native.pci_function=0; n.native.pci_domain=0;
    n.provenance = topology_fabric::Provenance::discovered("cuda","cuDeviceGetAttribute", topology_fabric::Confidence::AUTHORITATIVE);
    c.nodes.push_back(std::move(n)); c.success=true; cs.push_back(std::move(c));
  }
  {
    topology_fabric::Contribution c; c.provider = "pci";
    topology_fabric::ContributedNode n; n.ref="pci:0000:01:00.0"; n.type=topology_fabric::NodeType::PCI_DEVICE; n.name="PCI GPU";
    n.native.pci_bus=1; n.native.pci_device=0; n.native.pci_function=0; n.native.pci_domain=0; n.native.vendor_id=0x10DE;
    n.provenance = topology_fabric::Provenance::discovered("pci","CM_Get_Device_ID", topology_fabric::Confidence::HIGH);
    c.nodes.push_back(std::move(n)); c.success=true; cs.push_back(std::move(c));
  }
  auto mg = topology_fabric::merge_contributions(cs, {});
  std::cout << "merged nodes=" << mg.nodes.size() << " (input=" << mg.merged_count << ") conflicts=" << mg.conflicts.size() << "\n";
  for (const auto& [id, n] : mg.nodes) std::cout << "  node " << n.name << " type=" << std::string(topology_fabric::to_string(n.type))
      << " vendor=" << (n.native.vendor_id?std::to_string(*n.native.vendor_id):"?") << "\n";
  return 0;
}