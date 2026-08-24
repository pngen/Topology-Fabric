
#include "topology_fabric/merge.hpp"
#include "topology_fabric/node_id.hpp"
#include "topology_fabric/confidence.hpp"
#include <algorithm>
#include <unordered_set>

namespace topology_fabric {
namespace {

int type_rank(NodeType t) noexcept {
  switch (t) {
    case NodeType::ACCELERATOR_MEMORY_DOMAIN: return 9;
    case NodeType::ACCELERATOR: return 8;
    case NodeType::NETWORK_INTERFACE: return 7;
    case NodeType::STORAGE_DEVICE: return 6;
    case NodeType::SHARED_MEMORY_DOMAIN: return 6;
    case NodeType::PCI_DEVICE: return 5;
    case NodeType::PCI_BRIDGE: return 4;
    case NodeType::PCI_ROOT: return 3;
    case NodeType::CPU_PACKAGE: return 2;
    case NodeType::CPU_CORE: return 2;
    case NodeType::CPU_THREAD: return 2;
    case NodeType::NUMA_NODE: return 1;
    case NodeType::HOST_MEMORY_DOMAIN: return 1;
    case NodeType::MACHINE: return 1;
    case NodeType::UNKNOWN: return 0;
    case NodeType::EXTENSION: return 0;
  }
  return 0;
}

std::string group_key(const ContributedNode& n) {
  std::string k = n.native.canonical_key();
  if (k.empty()) k = std::string("type:") + std::string(to_string(n.type)) + ":" + n.name;
  return k;
}

struct Member {
  const ContributedNode* node;
  size_t ci;
  size_t idx;
};

bool better_type(NodeType a, NodeType b) noexcept { return type_rank(a) > type_rank(b); }

bool higher_confidence(Confidence a, Confidence b) noexcept {
  return ConfidenceRanking::rank(a) > ConfidenceRanking::rank(b);
}

// Merge NativeIdentity fields, favoring higher-confidence members (members pre-sorted
// best-first); first non-null wins.
NativeIdentity merge_native(const std::vector<Member>& members) {
  NativeIdentity out;
  for (auto& m : members) {
    const auto& n = m.node->native;
    if (!out.pci_domain) out.pci_domain = n.pci_domain;
    if (!out.pci_bus) out.pci_bus = n.pci_bus;
    if (!out.pci_device) out.pci_device = n.pci_device;
    if (!out.pci_function) out.pci_function = n.pci_function;
    if (!out.vendor_id) out.vendor_id = n.vendor_id;
    if (!out.device_id) out.device_id = n.device_id;
    if (!out.subsystem_id) out.subsystem_id = n.subsystem_id;
    if (!out.subsystem_vendor_id) out.subsystem_vendor_id = n.subsystem_vendor_id;
    if (!out.numa_node) out.numa_node = n.numa_node;
    if (!out.cpu_package) out.cpu_package = n.cpu_package;
    if (!out.core_id) out.core_id = n.core_id;
    if (!out.processor_group) out.processor_group = n.processor_group;
    if (!out.logical_processor_index) out.logical_processor_index = n.logical_processor_index;
    if (!out.cuda_ordinal) out.cuda_ordinal = n.cuda_ordinal;
    if (out.cuda_uuid.empty()) out.cuda_uuid = n.cuda_uuid;
    if (out.name.empty()) out.name = n.name;
    if (out.network_interface_name.empty()) out.network_interface_name = n.network_interface_name;
    if (out.network_hardware_id.empty()) out.network_hardware_id = n.network_hardware_id;
    if (out.storage_id.empty()) out.storage_id = n.storage_id;
    if (out.storage_device_path.empty()) out.storage_device_path = n.storage_device_path;
    if (out.machine_name.empty()) out.machine_name = n.machine_name;
    if (out.os_version.empty()) out.os_version = n.os_version;
  }
  return out;
}

}  // namespace

MergedGraph merge_contributions(const std::vector<Contribution>& contributions,
                                const Bounds& bounds) {
  MergedGraph mg;
  size_t totalInputNodes = 0;
  for (const auto& c : contributions) totalInputNodes += c.nodes.size();
  mg.merged_count = totalInputNodes;

  // 1. Group contributed nodes by canonical merge key.
  struct GroupData {
    std::vector<Member> members;
  };
  std::unordered_map<std::string, GroupData> groups;
  std::vector<std::tuple<size_t, std::string, std::string>> refMap;  // (ci, ref, group_key)

  for (size_t ci = 0; ci < contributions.size(); ++ci) {
    const auto& c = contributions[ci];
    for (size_t i = 0; i < c.nodes.size(); ++i) {
      const auto& cn = c.nodes[i];
      std::string key = group_key(cn);
      groups[key].members.push_back(Member{&cn, ci, i});
      // Bound check.
      if (groups.size() > bounds.max_nodes) {
        mg.warnings.push_back("max_nodes bound exceeded while merging; stopping node ingestion");
        break;
      }
    }
  }
  for (auto& [key, gd] : groups) {
    for (auto& m : gd.members) refMap.push_back({m.ci, m.node->ref, key});
  }

  // 2. Resolve resolved type per group, order members best-first (confidence).
  std::vector<std::pair<std::string, GroupData>> ordered(groups.begin(), groups.end());
  std::sort(ordered.begin(), ordered.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

  std::unordered_map<std::string, TopologyNodeId> groupNodeId;
  for (auto& [key, gd] : ordered) {
    auto& members = gd.members;
    std::stable_sort(members.begin(), members.end(), [](const Member& a, const Member& b) {
      int ca = ConfidenceRanking::rank(a.node->provenance.confidence);
      int cb = ConfidenceRanking::rank(b.node->provenance.confidence);
      if (ca != cb) return ca > cb;
      if (a.ci != b.ci) return a.ci < b.ci;
      return a.idx < b.idx;
    });
    // Resolved type = best type among members.
    NodeType best = members.front().node->type;
    for (auto& m : members) if (better_type(m.node->type, best)) best = m.node->type;

    if (mg.nodes.size() >= bounds.max_nodes) {
      mg.warnings.push_back("max_nodes exceeded; dropping group '" + key + "'");
      continue;
    }

    TopologyNode node;
    node.id = derive_node_id("touchstone", "node", key);
    node.type = best;
    node.category = "merged";
    // Name: prefer the best-type member's name, else longest non-empty.
    std::string bestName;
    { const ContributedNode* p = nullptr;
      for (auto& m : members) if (m.node->type == best) { p = m.node; break; }
      if (p) bestName = p->name;
      if (bestName.empty()) for (auto& m : members) if (m.node->name.size() > bestName.size()) bestName = m.node->name; }
    node.name = bestName;
    node.display_name = bestName.empty() ? node.id.to_hex() : bestName;
    node.native = merge_native(members);
    Capability caps = Capability::NONE;
    Confidence conf = Confidence::UNKNOWN;
    bool synthetic = false;
    for (auto& m : members) {
      caps = caps | m.node->capabilities;
      conf = ConfidenceRanking::maximum(conf, m.node->provenance.confidence);
      if (m.node->synthetic) synthetic = true;
    }
    node.capabilities = caps;
    node.confidence = conf;
    node.provenance = members.front().node->provenance;
    node.synthetic = synthetic;
    // Properties: best-confidence first already; keep first value per key and record conflicts.
    std::vector<std::string> providers;
    for (auto& m : members) {
      providers.push_back(m.node->provenance.provider + ":" + m.node->ref);
      for (const auto& [k, v] : m.node->properties) {
        auto it = node.properties.find(k);
        if (it == node.properties.end()) {
          node.properties.emplace(k, v);
        } else {
          mg.conflicts.push_back(std::string("property '") + k + "' conflict on group '" + key +
                                 "'; keeping higher-confidence value");
        }
      }
    }
    node.properties.emplace("tf.merged_providers",
                            PropertyValue::make_string_array(std::move(providers)));
    node.properties.emplace("tf.merged_members", PropertyValue(static_cast<int64_t>(members.size())));
    groupNodeId[key] = node.id;
    mg.nodes.emplace(node.id, std::move(node));
  }

  // 3. Resolve edges.
  // Build global ref -> node id (deterministic: first group that declared the ref).
  std::unordered_map<std::string, TopologyNodeId> refToId;
  // Prefer canonical group keys.
  for (auto& [key, gid] : groupNodeId) refToId.try_emplace(key, gid);
  for (auto& [ci, ref, key] : refMap) refToId.try_emplace(ref, groupNodeId[key]);

  for (size_t ci = 0; ci < contributions.size(); ++ci) {
    const auto& c = contributions[ci];
    for (const auto& ce : c.edges) {
      auto fit = refToId.find(ce.from_ref);
      auto tit = refToId.find(ce.to_ref);
      if (fit == refToId.end() || tit == refToId.end()) {
        mg.warnings.push_back("dangling edge dropped: '" + ce.from_ref + "' -> '" +
                              ce.to_ref + "' (unresolvable endpoint)");
        continue;
      }
      if (mg.edges.size() >= bounds.max_edges) {
        mg.warnings.push_back("max_edges exceeded; dropping remaining edges");
        break;
      }
      TopologyEdge e;
      e.source = fit->second;
      e.target = tit->second;
      e.type = ce.type;
      e.direction = ce.direction;
      e.provenance = ce.provenance;
      e.confidence = ce.confidence;
      e.width = ce.width;
      e.pcie_generation = ce.pcie_generation;
      e.bandwidth_bytes_per_sec = ce.bandwidth_bytes_per_sec;
      e.latency_ns = ce.latency_ns;
      e.hop_count = ce.hop_count;
      e.peer_capability = ce.peer_capability;
      e.properties = ce.properties;
      mg.edges.push_back(std::move(e));
    }
  }
  // Deduplicate identical edges (same key + direction).
  std::unordered_set<TopologyEdge::Key> seen;
  std::vector<TopologyEdge> dedup;
  for (auto& e : mg.edges) {
    if (seen.insert(e.key()).second) dedup.push_back(std::move(e));
  }
  mg.edges = std::move(dedup);
  return mg;
}

}  // namespace topology_fabric