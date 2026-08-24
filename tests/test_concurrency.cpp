#include "test_harness.hpp"
#include "tf_test_util.hpp"
#include <thread>
#include <atomic>
#include <vector>

using namespace topology_fabric;

TF_TEST(concurrency_parallel_queries) {
  auto snap = tf_test_util::build_snapshot(
      { {NodeType::MACHINE, "m", "m"},
        {NodeType::PCI_ROOT, "r", "r"},
        {NodeType::ACCELERATOR, "g0", "g0", 0, std::nullopt, std::nullopt, 0, 0, 0},
        {NodeType::ACCELERATOR, "g1", "g1", 0, std::nullopt, std::nullopt, 0, 1, 0},
        {NodeType::CPU_THREAD, "t0", "t0", 0} },
      { tf_test_util::edge("m", "r", EdgeType::CONTAINS),
        tf_test_util::edge("r", "g0", EdgeType::CONTAINS),
        tf_test_util::edge("r", "g1", EdgeType::CONTAINS),
        tf_test_util::edge("m", "t0", EdgeType::CONTAINS) });
  TopologyNodeId t0, g0;
  for (auto& [id, n] : snap->nodes()) { if (n.name=="t0") t0=id; else if (n.name=="g0") g0=id; }

  std::atomic<int> errors{0};
  std::atomic<bool> stop{false};
  std::vector<std::thread> threads;
  for (int i = 0; i < 8; ++i) {
    threads.emplace_back([&, t0, g0]() {
      for (int k = 0; k < 200; ++k) {
        if (stop.load()) return;
        auto p = lowest_cost_path(*snap, t0, g0, CostWeights{});
        if (!p.found) ++errors;
        auto rr = rank_candidates(*snap, t0, NodeType::ACCELERATOR, 0, CostWeights{});
        if (!rr.found) ++errors;
      }
    });
  }
  for (auto& th : threads) th.join();
  ASSERT(errors.load() == 0);
}

TF_TEST(concurrency_refresh_while_querying) {
  TopologyRuntime rt;
  // Build a runtime with only a fake set is hard; instead run discover on the built-in
  // providers on Windows while other threads query the current snapshot.
#ifdef _WIN32
  rt.register_builtin_providers();
  auto snap = rt.discover();
  ASSERT(snap != nullptr);
  std::atomic<int> errors{0};
  std::atomic<bool> stop{false};
  std::vector<std::thread> threads;
  for (int i = 0; i < 4; ++i) {
    threads.emplace_back([&]() {
      for (int k = 0; k < 100; ++k) {
        if (stop.load()) return;
        auto s = rt.current();
        if (!s) { ++errors; continue; }
        // A quick path query on the current snapshot; must not crash if snapshot
        // is swapped concurrently (it is immutable).
        for (auto& [id, n] : s->nodes()) {
          if (n.type == NodeType::ACCELERATOR) {
            shortest_path(*s, id, id);
            break;
          }
        }
      }
    });
  }
  std::thread refresher([&]() {
    for (int k = 0; k < 3; ++k) { rt.refresh(); }
    stop.store(true);
  });
  for (auto& th : threads) th.join();
  refresher.join();
  ASSERT(errors.load() == 0);
#endif
}