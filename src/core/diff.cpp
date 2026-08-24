#include "topology_fabric/diff.hpp"
#include "topology_fabric/edge.hpp"
#include <unordered_set>
#include <algorithm>

namespace topology_fabric {

bool is_material_event(const DiffEvent& e) {
  switch (e.kind) {
    case DiffEventKind::NODE_ADDED:
    case DiffEventKind::NODE_REMOVED:
    case DiffEventKind::EDGE_ADDED:
    case DiffEventKind::EDGE_REMOVED:
    case DiffEventKind::CAPABILITY_CHANGED:
    case DiffEventKind::LOCALITY_CHANGED:
    case DiffEventKind::PROVIDER_CHANGED:
      return true;
    case DiffEventKind::PROPERTY_CHANGED:
      // Edge property changes reflect link-structure facts (material).
      // Node property changes are generally telemetry (non-material).
      return e.on_edge;
  }
  return false;
}

bool is_material_change(const TopologyDiff& diff) { return diff.material_change; }

TopologyDiff compare_snapshots(const TopologySnapshot& before, const TopologySnapshot& after) {
  TopologyDiff d;
  d.generation_before = before.metadata().generation;
  d.generation_after = after.metadata().generation;

  auto& A = before.nodes();
  auto& B = after.nodes();

  // Node additions/removals.
  for (const auto& [id, n] : B) {
    auto it = A.find(id);
    if (it == A.end()) {
      DiffEvent e; e.kind = DiffEventKind::NODE_ADDED; e.node = id;
      e.after = std::string(to_string(n.type));
      d.events.push_back(std::move(e));
    }
  }
  for (const auto& [id, n] : A) {
    if (!B.count(id)) {
      DiffEvent e; e.kind = DiffEventKind::NODE_REMOVED; e.node = id;
      e.before = std::string(to_string(n.type));
      d.events.push_back(std::move(e));
    }
  }

  // Common-node property/capability/locality/provider changes.
  for (const auto& [id, nb] : B) {
    auto it = A.find(id);
    if (it == A.end()) continue;
    const auto& na = it->second;
    if (to_uint(na.capabilities) != to_uint(nb.capabilities)) {
      DiffEvent e; e.kind = DiffEventKind::CAPABILITY_CHANGED; e.node = id;
      e.before = std::to_string(to_uint(na.capabilities)); e.after = std::to_string(to_uint(nb.capabilities));
      d.events.push_back(std::move(e));
    }
    bool localityChanged = na.native.numa_node != nb.native.numa_node ||
                           na.native.cpu_package != nb.native.cpu_package ||
                           na.native.core_id != nb.native.core_id;
    if (localityChanged) {
      DiffEvent e; e.kind = DiffEventKind::LOCALITY_CHANGED; e.node = id;
      e.before = na.native.canonical_key(); e.after = nb.native.canonical_key();
      d.events.push_back(std::move(e));
    }
    if (na.provenance.provider != nb.provenance.provider) {
      DiffEvent e; e.kind = DiffEventKind::PROVIDER_CHANGED; e.node = id;
      e.before = na.provenance.provider; e.after = nb.provenance.provider;
      d.events.push_back(std::move(e));
    }
    for (const auto& [k, vb] : nb.properties) {
      auto pit = na.properties.find(k);
      if (pit == na.properties.end() || !(pit->second == vb)) {
        DiffEvent e; e.kind = DiffEventKind::PROPERTY_CHANGED; e.node = id; e.key = k;
        e.on_edge = false;
        d.events.push_back(std::move(e));
      }
    }
  }

  // Edge set diff (canonical keys).
  auto& EA = before.edges();
  auto& EB = after.edges();
  std::unordered_set<TopologyEdge::Key> ka, kb;
  for (auto& e : EA) ka.insert(e.key());
  for (auto& e : EB) kb.insert(e.key());
  for (auto& e : EB) if (!ka.count(e.key())) {
    DiffEvent ev; ev.kind = DiffEventKind::EDGE_ADDED; ev.node = e.source; ev.node2 = e.target;
    ev.key = std::string(to_string(e.type)); ev.on_edge = true; d.events.push_back(std::move(ev));
  }
  for (auto& e : EA) if (!kb.count(e.key())) {
    DiffEvent ev; ev.kind = DiffEventKind::EDGE_REMOVED; ev.node = e.source; ev.node2 = e.target;
    ev.key = std::string(to_string(e.type)); ev.on_edge = true; d.events.push_back(std::move(ev));
  }
  // Edge property changes (same key -> compare link facts).
  for (auto& e : EB) {
    const TopologyEdge* ea = nullptr;
    for (auto& ae : EA) if (ae.key() == e.key()) { ea = &ae; break; }
    if (!ea) continue;
    bool changed = (ea->bandwidth_bytes_per_sec != e.bandwidth_bytes_per_sec) ||
                   (ea->latency_ns != e.latency_ns) || (ea->width != e.width) ||
                   (ea->pcie_generation != e.pcie_generation) || (ea->accessible != e.accessible);
    if (changed) {
      DiffEvent ev; ev.kind = DiffEventKind::PROPERTY_CHANGED; ev.node = e.source; ev.node2 = e.target;
      ev.key = "edge.link"; ev.on_edge = true; d.events.push_back(std::move(ev));
    }
  }

  // Deterministic ordering of events.
  std::sort(d.events.begin(), d.events.end(), [](const DiffEvent& a, const DiffEvent& b) {
    if (a.kind != b.kind) return static_cast<int>(a.kind) < static_cast<int>(b.kind);
    if (a.node != b.node) return a.node < b.node;
    if (a.node2 != b.node2) return a.node2 < b.node2;
    if (a.key != b.key) return a.key < b.key;
    return a.before < b.before;
  });
  d.material_change = false;
  for (auto& e : d.events) if (is_material_event(e)) { d.material_change = true; break; }
  return d;
}

}  // namespace topology_fabric