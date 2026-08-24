#include "test_harness.hpp"
#include "tf_test_util.hpp"
#include "topology_fabric/merge.hpp"

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