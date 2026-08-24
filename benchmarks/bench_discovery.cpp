// benchmarks/bench_discovery.cpp - real local discovery timing.
#include "topology_fabric/runtime.hpp"
#include <chrono>
#include <cstdio>

int main() {
  topology_fabric::TopologyRuntime rt;
  rt.register_builtin_providers();
  auto t0 = std::chrono::steady_clock::now();
  auto snap = rt.discover();
  auto t1 = std::chrono::steady_clock::now();
  double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  std::printf("local discovery: %zu nodes, %zu edges, %.3f ms\n", snap->node_count(), snap->edge_count(), ms);
  // second discover (measure repeated timing)
  t0 = std::chrono::steady_clock::now();
  auto snap2 = rt.discover();
  t1 = std::chrono::steady_clock::now();
  ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  std::printf("re-discovery:   %zu nodes, %zu edges, %.3f ms (generation %llu)\n",
              snap2->node_count(), snap2->edge_count(), ms, (unsigned long long)snap2->metadata().generation);
  auto ts = rt.telemetry().snapshot();
  std::printf("telemetry: discovery_runs=%llu provider_success=%llu provider_fail=%llu\n",
              (unsigned long long)ts.discovery_runs, (unsigned long long)ts.provider_successes, (unsigned long long)ts.provider_failures);
  return 0;
}