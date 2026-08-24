
// provider_factories.hpp - internal factories for built-in discovery providers.
#pragma once
#include "topology_fabric/provider.hpp"
#include <memory>
namespace topology_fabric {
std::shared_ptr<TopologyProvider> create_host_provider();
std::shared_ptr<TopologyProvider> create_cpu_numa_provider();
std::shared_ptr<TopologyProvider> create_pci_provider();
std::shared_ptr<TopologyProvider> create_cuda_provider();
std::shared_ptr<TopologyProvider> create_storage_provider();
std::shared_ptr<TopologyProvider> create_network_provider();
}  // namespace topology_fabric
