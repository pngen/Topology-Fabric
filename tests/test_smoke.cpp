#include "test_harness.hpp"
#include "tf_test_util.hpp"
#include "topology_fabric/version.hpp"
#include <string>

using namespace topology_fabric;

TF_TEST(smoke_version) {
  ASSERT_EQ(std::string(kVersionString), "1.0.0");
  ASSERT(kVersionMajor == 1);
}

TF_TEST(smoke_snapshot_build) {
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"},
        {NodeType::ACCELERATOR, "g0", "g0", std::nullopt, std::nullopt, std::nullopt, 1, 0, 0},
        {NodeType::CPU_THREAD, "t0", "t0"} },
      { tf_test_util::edge("m", "g0", EdgeType::CONTAINS),
        tf_test_util::edge("m", "t0", EdgeType::CONTAINS) });
  ASSERT(snap->node_count() == 3);
  ASSERT(snap->edge_count() == 2);
  ASSERT(snap->validation().ok);
}

TF_TEST(smoke_local_discovery) {
#ifdef _WIN32
  auto snap = tf_test_util::discover_local();
  ASSERT(snap != nullptr);
  ASSERT(snap->node_count() > 0);
  ASSERT(snap->validation().ok);
  std::string accName;
  bool hasAccel = false;
  for (const auto& [id, n] : snap->nodes()) {
    if (n.type == NodeType::ACCELERATOR) { hasAccel = true; accName = n.name; break; }
  }
  ASSERT(hasAccel);  // The validation host has an NVIDIA RTX 5090.
  ASSERT(!accName.empty());
#else
  // Non-Windows: discovery degrades gracefully.
  auto snap = tf_test_util::discover_local();
  ASSERT(snap != nullptr);
#endif
}

TF_TEST(smoke_discovery_isolated) {
  // Two discovery runs should produce deterministic node/edge structure on the real host
  // (ids are derived deterministically from native identity).
#ifdef _WIN32
  auto a = tf_test_util::discover_local();
  auto b = tf_test_util::discover_local();
  ASSERT(a->node_count() == b->node_count());
  ASSERT(a->edge_count() == b->edge_count());
#endif
}