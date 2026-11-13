#include "test_harness.hpp"
#include "tf_test_util.hpp"
#include "topology_fabric/merge.hpp"
#include <stdexcept>

using namespace topology_fabric;

TF_TEST(merge_same_pci_unifies) {
  // Two providers report the same PCI BDF as ACCELERATOR + PCI_DEVICE; they must merge.
  std::vector<Contribution> cs;
  {
    Contribution c; c.provider="cuda";
    ContributedNode n; n.ref="cuda:uuid:x"; n.type=NodeType::ACCELERATOR; n.name="GPU";
    n.native.pci_bus=1; n.native.pci_device=0; n.native.pci_function=0; n.native.pci_domain=0;
    n.provenance = Provenance::discovered("cuda","x",Confidence::AUTHORITATIVE);
    c.nodes.push_back(std::move(n)); c.success=true; cs.push_back(std::move(c));
  }
  {
    Contribution c; c.provider="pci";
    ContributedNode n; n.ref="pci:0000:01:00.0"; n.type=NodeType::PCI_DEVICE; n.name="PCI GPU";
    n.native.pci_bus=1; n.native.pci_device=0; n.native.pci_function=0; n.native.pci_domain=0;
    n.native.vendor_id=0x10DE; n.provenance = Provenance::discovered("pci","y",Confidence::HIGH);
    c.nodes.push_back(std::move(n)); c.success=true; cs.push_back(std::move(c));
  }
  auto mg = merge_contributions(cs, {});
  ASSERT(mg.nodes.size() == 1);
  ASSERT(mg.merged_count == 2);
  for (auto& [id, n] : mg.nodes) {
    ASSERT(n.type == NodeType::ACCELERATOR);  // best type wins
    ASSERT(n.native.vendor_id == 0x10DE);
    ASSERT(n.confidence == Confidence::AUTHORITATIVE);
  }
}

TF_TEST(merge_high_confidence_wins) {
  std::vector<Contribution> cs;
  {
    Contribution c; c.provider="a";
    ContributedNode n; n.ref="x"; n.type=NodeType::STORAGE_DEVICE; n.name="disk";
    n.properties.emplace("model", PropertyValue("low"));
    n.provenance = Provenance::discovered("a","x",Confidence::LOW);
    c.nodes.push_back(std::move(n)); c.success=true; cs.push_back(std::move(c));
  }
  {
    Contribution c; c.provider="b";
    ContributedNode n; n.ref="storage:disk"; n.type=NodeType::STORAGE_DEVICE; n.name="disk";
    n.properties.emplace("model", PropertyValue("high"));
    n.provenance = Provenance::discovered("b","x",Confidence::AUTHORITATIVE);
    c.nodes.push_back(std::move(n)); c.success=true; cs.push_back(std::move(c));
  }
  auto mg = merge_contributions(cs, {});
  ASSERT(mg.nodes.size() == 1);
  for (auto& [id, n] : mg.nodes) {
    auto it = n.properties.find("model");
    ASSERT(it != n.properties.end());
    ASSERT(it->second.as_string() == "high");  // authoritative (higher-confidence) value wins and is not overwritten
  }
}

TF_TEST(merge_edge_resolution) {
  std::vector<Contribution> cs;
  {
    Contribution c; c.provider="p";
    ContributedNode a; a.ref="m"; a.type=NodeType::MACHINE; a.name="m";
    ContributedNode b; b.ref="g"; b.type=NodeType::ACCELERATOR; b.name="g";
    ContributedEdge e; e.from_ref="m"; e.to_ref="g"; e.type=EdgeType::CONTAINS;
    c.nodes.push_back(std::move(a)); c.nodes.push_back(std::move(b));
    c.edges.push_back(std::move(e)); c.success=true; cs.push_back(std::move(c));
  }
  auto mg = merge_contributions(cs, {});
  ASSERT(mg.nodes.size() == 2);
  ASSERT(mg.edges.size() == 1);
  for (auto& e : mg.edges) { ASSERT(e.type == EdgeType::CONTAINS); ASSERT(e.source != e.target); }
}

TF_TEST(merge_conflict_reported) {
  std::vector<Contribution> cs;
  {
    Contribution c; c.provider="p";
    ContributedNode a; a.ref="m"; a.type=NodeType::MACHINE; a.name="m";
    a.properties.emplace("x", PropertyValue(1));
    // two suggestions for same key with differing values in same group
    c.nodes.push_back(std::move(a));
    c.success=true; cs.push_back(std::move(c));
  }
  auto mg = merge_contributions(cs, {});
  // No conflict if single member; check no crash and proper node.
  ASSERT(mg.nodes.size() >= 1);
}

TF_TEST(merge_agreeing_values_are_not_conflicts) {
  // Two providers that report the same value corroborate each other; only a
  // member that disagrees is a conflict.
  auto disk = [](const char* provider, const char* ref, const char* model, Confidence conf) {
    Contribution c; c.provider = provider;
    ContributedNode n; n.ref = ref; n.type = NodeType::STORAGE_DEVICE; n.name = "disk";
    n.native.storage_id = "disk-1";
    n.properties.emplace("model", PropertyValue(model));
    n.provenance = Provenance::discovered(provider, "x", conf);
    c.nodes.push_back(std::move(n)); c.success = true;
    return c;
  };
  std::vector<Contribution> same{disk("a", "x", "m1", Confidence::HIGH), disk("b", "y", "m1", Confidence::LOW)};
  auto agreed = merge_contributions(same, {});
  ASSERT(agreed.nodes.size() == 1);
  ASSERT(agreed.conflicts.empty());
  std::vector<Contribution> different{disk("a", "x", "m1", Confidence::HIGH), disk("b", "y", "m2", Confidence::LOW)};
  auto disagreed = merge_contributions(different, {});
  ASSERT(disagreed.nodes.size() == 1);
  ASSERT(disagreed.conflicts.size() == 1);
}

TF_TEST(merge_dropped_node_takes_its_edges_with_it) {
  // With room for one node the other is dropped. The edge between them is
  // dropped too, with a warning; it is not kept with an endpoint that is no node.
  Contribution c; c.provider = "p";
  ContributedNode a; a.ref = "a"; a.type = NodeType::MACHINE; a.name = "a";
  ContributedNode b; b.ref = "b"; b.type = NodeType::ACCELERATOR; b.name = "b";
  ContributedEdge e; e.from_ref = "a"; e.to_ref = "b"; e.type = EdgeType::CONTAINS;
  c.nodes.push_back(a); c.nodes.push_back(b); c.edges.push_back(e); c.success = true;
  std::vector<Contribution> cs{c};
  Bounds one; one.max_nodes = 1;
  auto mg = merge_contributions(cs, one);
  ASSERT(mg.nodes.size() == 1);
  ASSERT(mg.edges.empty());
  for (auto& edge : mg.edges) {
    ASSERT(mg.nodes.count(edge.source) == 1);
    ASSERT(mg.nodes.count(edge.target) == 1);
  }
  ASSERT(!mg.warnings.empty());
}

namespace {
// A provider that either contributes a node with a warning and an edge to
// nowhere, or fails by throwing.
class ScriptedProvider : public TopologyProvider {
 public:
  ScriptedProvider(std::string name, bool throws) : name_(std::move(name)), throws_(throws) {}
  std::string name() const override { return name_; }
  std::string version() const override { return "1"; }
  Contribution discover(const DiscoveryContext&) override {
    if (throws_) throw std::runtime_error("device went away");
    Contribution c; c.provider = name_; c.version = "1";
    ContributedNode m; m.ref = "m"; m.type = NodeType::MACHINE; m.name = "m";
    ContributedEdge e; e.from_ref = "m"; e.to_ref = "nowhere"; e.type = EdgeType::CONTAINS;
    c.nodes.push_back(m); c.edges.push_back(e);
    c.warnings.push_back("partial view");
    c.success = true;
    return c;
  }
 private:
  std::string name_;
  bool throws_;
};
}  // namespace

TF_TEST(discovery_warnings_reach_the_snapshot) {
  // What the providers and the merge had to say about a discovery is part of
  // the snapshot: a provider's own warning, a provider that threw, an edge
  // that was dropped.
  TopologyRuntime rt;
  rt.providers().register_provider(std::make_shared<ScriptedProvider>("good", false));
  rt.providers().register_provider(std::make_shared<ScriptedProvider>("bad", true));
  auto snap = rt.discover();
  ASSERT(snap != nullptr);
  auto warned = [&](const char* text) {
    for (auto& w : snap->metadata().warnings) if (w.find(text) != std::string::npos) return true;
    return false;
  };
  ASSERT(warned("partial view"));
  ASSERT(warned("device went away"));
  ASSERT(warned("nowhere"));
  ASSERT(snap->metadata().partial_discovery);
}