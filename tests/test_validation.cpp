#include "test_harness.hpp"
#include "tf_test_util.hpp"
#include "topology_fabric/snapshot.hpp"

using namespace topology_fabric;

TF_TEST(validation_dangling_edge) {
  SnapshotBuilder b;
  TopologyNode a; a.id = derive_node_id("t","n","a"); a.type = NodeType::MACHINE; a.name="a";
  b.add_node(a);
  TopologyEdge e; e.source = a.id; e.target = derive_node_id("t","n","zzz"); e.type = EdgeType::CONNECTED_TO;
  b.add_edge(e);
  auto snap = b.take();
  ASSERT(!snap->validation().ok);
}

TF_TEST(validation_containment_cycle) {
  // a CONTAINS b, b CONTAINS a -> cycle.
  SnapshotBuilder b;
  TopologyNode a; a.id = derive_node_id("t","n","a"); a.type = NodeType::MACHINE; a.name="a";
  TopologyNode c; c.id = derive_node_id("t","n","c"); c.type = NodeType::CPU_THREAD; c.name="c";
  b.add_node(a); b.add_node(c);
  TopologyEdge e1; e1.source=a.id; e1.target=c.id; e1.type=EdgeType::CONTAINS;
  TopologyEdge e2; e2.source=c.id; e2.target=a.id; e2.type=EdgeType::CONTAINS;
  b.add_edge(e1); b.add_edge(e2);
  auto snap = b.take();
  ASSERT(!snap->validation().ok);
}

TF_TEST(validation_self_containment) {
  SnapshotBuilder b;
  TopologyNode a; a.id = derive_node_id("t","n","a"); a.type = NodeType::MACHINE; a.name="a";
  b.add_node(a);
  TopologyEdge e; e.source=a.id; e.target=a.id; e.type=EdgeType::CONTAINS;
  b.add_edge(e);
  auto snap = b.take();
  ASSERT(!snap->validation().ok);
}

TF_TEST(validation_duplicate_native_identity) {
  SnapshotBuilder b;
  TopologyNode a; a.id = derive_node_id("t","n","a"); a.type = NodeType::ACCELERATOR; a.name="g0";
  a.native.pci_bus=1; a.native.pci_device=0; a.native.pci_function=0; a.native.pci_domain=0;
  TopologyNode c; c.id = derive_node_id("t","n","c"); c.type = NodeType::ACCELERATOR; c.name="g1";
  c.native.pci_bus=1; c.native.pci_device=0; c.native.pci_function=0; c.native.pci_domain=0;  // duplicate BDF
  b.add_node(a); b.add_node(c);
  auto snap = b.take();
  ASSERT(!snap->validation().ok);
}

TF_TEST(validation_unknown_type_warns_ok) {
  SnapshotBuilder b;
  TopologyNode a; a.id = derive_node_id("t","n","a"); a.type = NodeType::UNKNOWN; a.name="mystery"; a.category="";
  b.add_node(a);
  auto snap = b.take();
  ASSERT(snap->validation().ok);  // unknown type is a warning, not an error
}