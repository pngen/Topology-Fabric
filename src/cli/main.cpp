#include "topology_fabric/runtime.hpp"
#include "topology_fabric/serialization.hpp"
#include "topology_fabric/result.hpp"
#include "topology_fabric/json.hpp"
#include "topology_fabric/version.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <chrono>
#include <cstring>
#include <algorithm>

using namespace topology_fabric;

namespace {

// Find a node by 32-hex id or by name substring.
TopologyNodeId resolve_id(const TopologySnapshot& snap, const std::string& token) {
  TopologyNodeId id;
  if (TopologyNodeId::try_from_hex(token, id)) return id;
  // name / display_name substring match, prefer exact.
  TopologyNodeId first;
  for (const auto& [nid, n] : snap.nodes()) {
    if (n.name == token || n.display_name == token) return nid;
    if (n.name.find(token) != std::string::npos) { if (first.is_null()) first = nid; }
  }
  return first;
}



json::Value path_to_json(const TopologySnapshot& snap, const TopologyPath& p) {
  json::Value o = json::Value::object();
  o.set("found", json::Value(p.found));
  o.set("source", json::Value(p.source.to_hex()));
  o.set("destination", json::Value(p.destination.to_hex()));
  o.set("hop_count", json::Value(p.hop_count));
  o.set("path_class", json::Value(std::string(to_string(p.path_class))));
  o.set("locality", json::Value(std::string(to_string(p.locality))));
  o.set("total_cost", json::Value(p.total_cost));
  o.set("estimated_bandwidth_bps", json::Value(p.estimated_bandwidth_bps));
  o.set("estimated_latency_ns", json::Value(p.estimated_latency_ns));
  json::Array segs;
  for (auto& s : p.segments) {
    json::Value sj = json::Value::object();
    sj.set("from", json::Value(s.from.to_hex()));
    sj.set("to", json::Value(s.to.to_hex()));
    sj.set("type", json::Value(std::string(to_string(s.edge_type))));
    sj.set("cost", json::Value(s.edge_cost));
    segs.push_back(std::move(sj));
  }
  o.set("segments", std::move(segs));
  return o;
}

json::Value rank_to_json(const RankResult& rr, const TopologySnapshot& snap) {
  json::Value o = json::Value::object();
  o.set("source", json::Value(rr.source.to_hex()));
  json::Array arr;
  for (auto& e : rr.ranked) {
    json::Value rj = json::Value::object();
    const auto* n = snap.find_node(e.id);
    rj.set("id", json::Value(e.id.to_hex()));
    rj.set("name", json::Value(n ? n->name : std::string()));
    rj.set("cost", json::Value(e.cost));
    rj.set("path_class", json::Value(std::string(to_string(e.path_class))));
    rj.set("locality", json::Value(std::string(to_string(e.locality))));
    json::Array reasons;
    for (auto& r : e.reasons) reasons.push_back(json::Value(r));
    rj.set("reasons", std::move(reasons));
    arr.push_back(std::move(rj));
  }
  o.set("ranked", std::move(arr));
  return o;
}

bool has_opt(const std::vector<std::string>& a, const std::string& o) {
  return std::find(a.begin(), a.end(), o) != a.end();
}
std::string opt_val(const std::vector<std::string>& a, const std::string& o, const std::string& def = {}) {
  for (size_t i = 0; i + 1 < a.size(); ++i) if (a[i] == o) return a[i + 1];
  return def;
}

int usage(const char* argv0) {
  std::cout << "Topology Fabric " << kVersionString << "\n"
            << "A vendor-neutral runtime for hardware/interconnect topology discovery, modeling,\n"
            << "measuring, validating, scoring, and exposure.\n\n"
            << "Usage: " << argv0 << " <command> [options] [--json]\n\nCommands:\n"
            << "  info | discover | nodes | edges | devices | cpus | numa | pci | accelerators\n"
            << "  storage | network | path | distance | rank | explain | snapshot | diff\n"
            << "  validate | measure | stats | selftest | benchmark\n\nOptions:\n"
            << "  --from <id|name>  --to <id|name>  --source <id|name>\n"
            << "  --type <nodetype>  --max <n>  --json\n";
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) return usage(argv[0]);
  std::string cmd = argv[1];
  std::vector<std::string> args;
  for (int i = 2; i < argc; ++i) args.push_back(argv[i]);
  bool json = has_opt(args, "--json");

    TopologyRuntime rt;
    rt.register_builtin_providers();
    auto snap = rt.discover();
    auto& sn = *snap;

  try {
    if (cmd == "info" || cmd == "discover") {
      if (json) { std::cout << rt.serialize(sn, true) << "\n"; }
      else {
        if (cmd == "discover") std::cout << "Discovery run:\n";
        std::cout << "  product:      " << kProductName << " " << kVersionString << "\n";
        std::cout << "  snapshot:     " << sn.metadata().snapshot_id << "\n";
        std::cout << "  generation:   " << sn.metadata().generation << "\n";
        std::cout << "  machine:      " << sn.metadata().machine_identity << "\n";
        std::cout << "  nodes:        " << sn.node_count() << "\n";
        std::cout << "  edges:        " << sn.edge_count() << "\n";
        std::cout << "  partial:      " << (sn.metadata().partial_discovery ? "yes" : "no") << "\n";
        std::cout << "  valid:        " << (sn.validation().ok ? "yes" : "no") << "\n";
        std::cout << "  providers:    " << sn.metadata().provider_versions << "\n";
      }
    } else if (cmd == "nodes" || cmd == "devices" || cmd == "cpus" || cmd == "numa" ||
               cmd == "pci" || cmd == "accelerators" || cmd == "storage" || cmd == "network") {
      json::Array arr;
      NodeType filter = NodeType::UNKNOWN;
      bool anyFilter = true;
      if (cmd == "devices") { filter = NodeType::ACCELERATOR; anyFilter = true;
        // devices = any node with PCI identity or accelerator/storage/network
        for (const auto& [id, n] : sn.nodes()) {
          bool isDev = n.native.has_pci() || n.type == NodeType::ACCELERATOR ||
                       n.type == NodeType::STORAGE_DEVICE || n.type == NodeType::NETWORK_INTERFACE;
          if (!isDev) continue;
          json::Value o = json::Value::object();
          o.set("id", json::Value(id.to_hex()));
          o.set("type", json::Value(std::string(to_string(n.type))));
          o.set("name", json::Value(n.name));
          o.set("bdf", json::Value(n.native.pci_bdf_string()));
          arr.push_back(std::move(o));
        }
      } else if (cmd == "cpus") { filter = NodeType::CPU_THREAD; }
      else if (cmd == "numa") { filter = NodeType::NUMA_NODE; }
      else if (cmd == "pci") { filter = NodeType::PCI_DEVICE; }
      else if (cmd == "accelerators") { filter = NodeType::ACCELERATOR; }
      else if (cmd == "storage") { filter = NodeType::STORAGE_DEVICE; }
      else if (cmd == "network") { filter = NodeType::NETWORK_INTERFACE; }
      else if (cmd != "devices") { filter = NodeType::UNKNOWN; }

      if (cmd != "devices") {
        for (const auto& [id, n] : sn.nodes()) {
          if (cmd == "pci") {
            bool isPci = n.type == NodeType::PCI_ROOT || n.type == NodeType::PCI_BRIDGE || n.type == NodeType::PCI_DEVICE;
            if (!isPci) continue;
          } else if (n.type != filter) continue;
          json::Value o = json::Value::object();
          o.set("id", json::Value(id.to_hex()));
          o.set("type", json::Value(std::string(to_string(n.type))));
          o.set("name", json::Value(n.name));
          if (n.native.has_pci()) o.set("bdf", json::Value(n.native.pci_bdf_string()));
          arr.push_back(std::move(o));
        }
      }
      if (json) std::cout << json::dump(json::Value(std::move(arr)), json::WriteMode::Pretty) << "\n";
      else {
        for (const auto& it : arr) {
          std::cout << it.find("id")->as_string() << "  "
                    << it.find("type")->as_string() << "  "
                    << it.find("name")->as_string();
          if (auto* b = it.find("bdf")) std::cout << "  [" << b->as_string() << "]";
          std::cout << "\n";
        }
      }
    } else if (cmd == "edges") {
      json::Array arr;
      for (auto& e : sn.edges()) {
        json::Value o = json::Value::object();
        o.set("source", json::Value(e.source.to_hex()));
        o.set("target", json::Value(e.target.to_hex()));
        o.set("type", json::Value(std::string(to_string(e.type))));
        o.set("direction", json::Value(std::string(to_string(e.direction))));
        arr.push_back(std::move(o));
      }
      if (json) std::cout << json::dump(json::Value(std::move(arr)), json::WriteMode::Pretty) << "\n";
      else for (auto& e : sn.edges()) std::cout << e.source.to_hex() << " --" << std::string(to_string(e.type)) << "--> " << e.target.to_hex() << "\n";
    } else if (cmd == "path") {
      auto f = resolve_id(sn, opt_val(args, "--from"));
      auto t = resolve_id(sn, opt_val(args, "--to"));
      if (f.is_null() || t.is_null()) { std::cerr << "path requires resolvable --from and --to\n"; return 2; }
      auto p = lowest_cost_path(sn, f, t, rt.default_cost_weights());
      if (json) std::cout << json::dump(path_to_json(sn, p), json::WriteMode::Pretty) << "\n";
      else {
        if (!p.found) { std::cout << "no topology path\n"; return 0; }
        std::cout << "source: " << f.to_hex() << "\ndestination: " << t.to_hex() << "\n";
        std::cout << "class: " << std::string(to_string(p.path_class)) << "\n";
        std::cout << "locality: " << std::string(to_string(p.locality)) << "\n";
        std::cout << "hops: " << p.hop_count << "\n";
        std::cout << "cost: " << p.total_cost << "\n";
        std::cout << "bandwidth: " << p.estimated_bandwidth_bps << " B/s\n";
        std::cout << "latency: " << p.estimated_latency_ns << " ns\n";
        for (auto& seg : p.segments) std::cout << "  " << seg.from.to_hex() << " --" << std::string(to_string(seg.edge_type)) << "--> " << seg.to.to_hex() << "\n";
      }
    } else if (cmd == "distance") {
      auto f = resolve_id(sn, opt_val(args, "--from"));
      auto t = resolve_id(sn, opt_val(args, "--to"));
      auto d = distance_between(sn, f, t, rt.default_cost_weights());
      std::cout << "hops=" << d.graph_hops << " pci_depth=" << d.pci_depth << " root_crossings=" << d.root_crossings
                << " transitions=" << d.device_class_transitions << " normalized=" << d.normalized_score << "\n";
    } else if (cmd == "rank") {
      auto src = resolve_id(sn, opt_val(args, "--source"));
      if (src.is_null()) { std::cerr << "rank requires resolvable --source\n"; return 2; }
      std::string tstr = opt_val(args, "--type", "accelerator");
      NodeType type = node_type_from_string(tstr);
      size_t max = 0; { std::string m = opt_val(args, "--max"); if (!m.empty()) max = std::strtoull(m.c_str(), nullptr, 10); }
      auto rr = rank_candidates(sn, src, type, max, rt.default_cost_weights());
      if (json) std::cout << json::dump(rank_to_json(rr, sn), json::WriteMode::Pretty) << "\n";
      else for (auto& e : rr.ranked) std::cout << e.id.to_hex() << "  cost=" << e.cost << "  " << std::string(to_string(e.locality)) << "\n";
    } else if (cmd == "explain") {
      auto f = resolve_id(sn, opt_val(args, "--from"));
      auto t = resolve_id(sn, opt_val(args, "--to"));
      auto ex = explain(sn, f, t, rt.default_cost_weights());
      std::cout << "summary: " << ex.summary << "\n";
      for (auto& factor : ex.factors) std::cout << "  " << factor.name << ": " << factor.value << "  (" << factor.note << ")\n";
    } else if (cmd == "snapshot") {
      if (json) std::cout << rt.serialize(sn, true) << "\n";
      else std::cout << "id=" << sn.metadata().snapshot_id << " generation=" << sn.metadata().generation
                     << " nodes=" << sn.node_count() << " edges=" << sn.edge_count() << "\n";
    } else if (cmd == "diff") {
      auto first = rt.current();
      auto second = rt.discover();
      auto d = compare_snapshots(*first, *second);
      if (json) {
        json::Value o = json::Value::object();
        o.set("generation_before", json::Value(d.generation_before));
        o.set("generation_after", json::Value(d.generation_after));
        o.set("material", json::Value(d.material_change));
        json::Array arr;
        for (auto& e : d.events) {
          json::Value ej = json::Value::object();
          ej.set("kind", json::Value(std::string(to_string(e.kind))));
          ej.set("node", json::Value(e.node.to_hex()));
          ej.set("key", json::Value(e.key));
          arr.push_back(std::move(ej));
        }
        o.set("events", std::move(arr));
        std::cout << json::dump(o, json::WriteMode::Pretty) << "\n";
      } else {
        std::cout << "generation " << d.generation_before << " -> " << d.generation_after
                  << " material=" << (d.material_change ? "yes" : "no") << " events=" << d.events.size() << "\n";
        for (auto& e : d.events) std::cout << "  " << std::string(to_string(e.kind)) << " " << e.node.to_hex() << "\n";
      }
    } else if (cmd == "validate") {
      if (json) {
        json::Value o = json::Value::object();
        o.set("ok", json::Value(sn.validation().ok));
        json::Array errs; for (auto& e : sn.validation().errors) errs.push_back(json::Value(e)); o.set("errors", std::move(errs));
        json::Array ws; for (auto& w : sn.validation().warnings) ws.push_back(json::Value(w)); o.set("warnings", std::move(ws));
        std::cout << json::dump(o, json::WriteMode::Pretty) << "\n";
      } else {
        std::cout << "valid: " << (sn.validation().ok ? "yes" : "no") << "\n";
        for (auto& e : sn.validation().errors) std::cout << "  error: " << e << "\n";
        for (auto& w : sn.validation().warnings) std::cout << "  warning: " << w << "\n";
      }
      return sn.validation().ok ? 0 : 3;
    } else if (cmd == "measure") {
      // Safe bounded local memcpy microbenchmark (observational; no graph mutation).
      const size_t MB = 32;
      size_t bytes = MB * 1024 * 1024;
      std::vector<unsigned char> a(bytes), b(bytes);
      for (size_t i = 0; i < bytes; ++i) a[i] = static_cast<unsigned char>(i);
      auto t0 = std::chrono::steady_clock::now();
      for (int iter = 0; iter < 40; ++iter) std::memcpy(b.data(), a.data(), bytes);
      auto t1 = std::chrono::steady_clock::now();
      double secs = std::chrono::duration<double>(t1 - t0).count();
      double gbps = (static_cast<double>(bytes) * 40 * 2) / secs / 1e9;
      std::cout << "host_memcpy: " << MB << " MiB x40 (r+w) -> " << gbps << " GiB/s\n";
      rt.telemetry().record_measurement_run();
    } else if (cmd == "stats") {
      auto ts = rt.telemetry().snapshot();
      if (json) {
        json::Value o = json::Value::object();
        o.set("discovery_runs", json::Value(ts.discovery_runs));
        o.set("nodes_total", json::Value(ts.nodes_total));
        o.set("edges_total", json::Value(ts.edges_total));
        o.set("provider_successes", json::Value(ts.provider_successes));
        o.set("provider_failures", json::Value(ts.provider_failures));
        o.set("partial_discoveries", json::Value(ts.partial_discoveries));
        o.set("merge_conflicts", json::Value(ts.merge_conflicts));
        o.set("snapshots_created", json::Value(ts.snapshots_created));
        o.set("generation_changes", json::Value(ts.generation_changes));
        o.set("discovery_total_ms", json::Value(ts.discovery_total_ms));
        std::cout << json::dump(o, json::WriteMode::Pretty) << "\n";
      } else {
        std::cout << "discovery_runs=" << ts.discovery_runs << " nodes=" << ts.nodes_total << " edges=" << ts.edges_total << "\n";
        std::cout << "providers: success=" << ts.provider_successes << " fail=" << ts.provider_failures << " partial=" << ts.partial_discoveries << "\n";
        std::cout << "merge_conflicts=" << ts.merge_conflicts << " snapshots=" << ts.snapshots_created << " gen_changes=" << ts.generation_changes << "\n";
      }
    } else if (cmd == "selftest") {
      int rc = 0;
      // Minimal in-process self-check: build a tiny graph and run a path.
      try {
        SnapshotBuilder b;
        TopologyNode n1, n2;
        n1.type = NodeType::CPU_THREAD; n1.name = "t0"; n1.id = derive_node_id("tf", "node", "x0");
        n2.type = NodeType::ACCELERATOR; n2.name = "g0"; n2.id = derive_node_id("tf", "node", "x1");
        b.add_node(n1); b.add_node(n2);
        TopologyEdge e; e.source = n1.id; e.target = n2.id; e.type = EdgeType::CONNECTED_TO; e.direction = EdgeDirection::UNDIRECTED;
        b.add_edge(e);
        auto mini = b.take();
        auto p = shortest_path(*mini, n1.id, n2.id);
        if (!p.found) { std::cerr << "selftest: path not found\n"; rc = 1; }
        std::string js = serialize_snapshot_json(*mini);
        auto back = deserialize_snapshot_json(js);
        if (back->node_count() != 2) { std::cerr << "selftest: roundtrip mismatch\n"; rc = 1; }
        if (rc == 0) std::cout << "selftest: OK\n";
      } catch (const std::exception& ex) { std::cerr << "selftest failed: " << ex.what() << "\n"; rc = 1; }
      return rc;
    } else if (cmd == "benchmark") {
      // Small in-process benchmark: repeated path queries over the current snapshot.
      auto t0 = std::chrono::steady_clock::now();
      size_t queries = 0;
      std::vector<TopologyNodeId> ids;
      for (auto& [id, n] : sn.nodes()) ids.push_back(id);
      if (ids.size() >= 2) {
        for (int i = 0; i < 2000; ++i) {
          size_t a = static_cast<size_t>(i) % ids.size();
          size_t bIdx = (a + 1) % ids.size();
          lowest_cost_path(sn, ids[a], ids[bIdx], rt.default_cost_weights());
          ++queries;
        }
      }
      auto t1 = std::chrono::steady_clock::now();
      double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
      std::cout << "path_queries=" << queries << " total_ms=" << ms << " avg_us=" << (ms * 1000.0 / static_cast<double>(queries)) << "\n";
    } else {
      return usage(argv[0]);
    }
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  }
  return 0;
}