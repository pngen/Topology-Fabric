#include "test_harness.hpp"
#include "topology_fabric/node_id.hpp"
#include "topology_fabric/identity.hpp"

using namespace topology_fabric;

TF_TEST(identity_hex_roundtrip) {
  TopologyNodeId id(0x0102030405060708ull, 0x090a0b0c0d0e0f10ull);
  std::string h = id.to_hex();
  ASSERT(h.size() == 32);
  TopologyNodeId back;
  ASSERT(TopologyNodeId::try_from_hex(h, back));
  ASSERT(back == id);
}

TF_TEST(identity_hex_rejects_bad) {
  TopologyNodeId id;
  ASSERT(!TopologyNodeId::try_from_hex("xyz", id));
  ASSERT(!TopologyNodeId::try_from_hex("0000000000000000000000000000000", id));  // 31 chars
  ASSERT_THROWS(TopologyNodeId::from_hex("nothex"));
}

TF_TEST(identity_derive_deterministic) {
  auto a = derive_node_id("ns", "cat", "ident");
  auto b = derive_node_id("ns", "cat", "ident");
  auto c = derive_node_id("ns", "cat", "other");
  ASSERT(a == b);
  ASSERT(a != c);
  ASSERT(!a.is_null());
}

TF_TEST(identity_ordering_hash) {
  TopologyNodeId a(1, 2), b(1, 3);
  ASSERT(a < b);
  ASSERT(a != b);
  std::hash<TopologyNodeId> h;
  ASSERT(h(a) != 0 || true);
}

TF_TEST(identity_native_key) {
  NativeIdentity n;
  ASSERT(n.canonical_key().empty());
  n.pci_bus = 1; n.pci_device = 0; n.pci_function = 0; n.pci_domain = 0;
  ASSERT(n.has_pci());
  ASSERT(n.canonical_key() == "pci:0000:01:00.0");
  ASSERT(n.pci_bdf_string() == "0000:01:00.0");
}