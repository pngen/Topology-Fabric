
// TopologyFabric/registry.hpp - thread-safe provider registry with precedence.
#pragma once
#include <memory>
#include <mutex>
#include <unordered_map>
#include <string>
#include <vector>
#include "topology_fabric/provider.hpp"

namespace topology_fabric {

// Ordered, thread-safe registry of topology providers. Registration is
// last-writer-wins by provider name (used to override stubs with real backends);
// iteration order is deterministic (registration order).
class ProviderRegistry {
 public:
  ProviderRegistry() = default;
  ProviderRegistry(const ProviderRegistry&) = delete;
  ProviderRegistry& operator=(const ProviderRegistry&) = delete;

  bool register_provider(std::shared_ptr<TopologyProvider> p);
  bool unregister(const std::string& name);
  std::shared_ptr<TopologyProvider> find(const std::string& name) const;
  std::vector<std::shared_ptr<TopologyProvider>> all() const;
  size_t size() const noexcept;
  void clear() noexcept;

 private:
  mutable std::mutex mu_;
  std::unordered_map<std::string, std::shared_ptr<TopologyProvider>> byName_;
  std::vector<std::string> order_;   // insertion order
};

}  // namespace topology_fabric
