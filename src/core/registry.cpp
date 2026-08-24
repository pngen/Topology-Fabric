
#include "topology_fabric/registry.hpp"

namespace topology_fabric {

bool ProviderRegistry::register_provider(std::shared_ptr<TopologyProvider> p) {
  if (!p) return false;
  std::lock_guard<std::mutex> lk(mu_);
  std::string nm = p->name();
  bool isNew = byName_.find(nm) == byName_.end();
  byName_[nm] = std::move(p);
  if (isNew) order_.push_back(nm);
  return true;
}

bool ProviderRegistry::unregister(const std::string& name) {
  std::lock_guard<std::mutex> lk(mu_);
  auto it = byName_.find(name);
  if (it == byName_.end()) return false;
  byName_.erase(it);
  order_.erase(std::remove(order_.begin(), order_.end(), name), order_.end());
  return true;
}

std::shared_ptr<TopologyProvider> ProviderRegistry::find(const std::string& name) const {
  std::lock_guard<std::mutex> lk(mu_);
  auto it = byName_.find(name);
  return it == byName_.end() ? nullptr : it->second;
}

std::vector<std::shared_ptr<TopologyProvider>> ProviderRegistry::all() const {
  std::lock_guard<std::mutex> lk(mu_);
  std::vector<std::shared_ptr<TopologyProvider>> out;
  out.reserve(order_.size());
  for (auto& name : order_) {
    auto it = byName_.find(name);
    if (it != byName_.end()) out.push_back(it->second);
  }
  return out;
}

size_t ProviderRegistry::size() const noexcept {
  std::lock_guard<std::mutex> lk(mu_);
  return byName_.size();
}

void ProviderRegistry::clear() noexcept {
  std::lock_guard<std::mutex> lk(mu_);
  byName_.clear();
  order_.clear();
}

}  // namespace topology_fabric