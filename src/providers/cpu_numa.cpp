#include "topology_fabric/provider.hpp"
#include "topology_fabric/provenance.hpp"
#include "provider_factories.hpp"
#include "windows_util.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <vector>
#include <string>
#endif

namespace topology_fabric {
namespace {

#ifdef _WIN32

class CpuNumaProvider : public TopologyProvider {
 public:
  std::string name() const override { return "cpu_numa"; }
  std::string version() const override { return "1.0.0"; }
  Contribution discover(const DiscoveryContext&) override {
    Contribution c;
    c.provider = name(); c.version = version();
    mark_available(true);

    char mbuf[256] = {};
    DWORD sz = 256;
    std::string machine;
    if (::GetComputerNameExA(ComputerNameDnsHostname, mbuf, &sz)) machine = mbuf;
    std::string machineRef = "machine:" + machine;

    std::vector<BYTE> buffer;
    DWORD bytes = 0;
    ::GetLogicalProcessorInformationEx(RelationAll, nullptr, &bytes);
    if (bytes == 0) { c.warnings.push_back(winutil::last_error("GetLogicalProcessorInformationEx")); c.partial = true; return c; }
    buffer.resize(bytes);
    if (!::GetLogicalProcessorInformationEx(RelationAll, reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()), &bytes)) {
      c.warnings.push_back(winutil::last_error("GetLogicalProcessorInformationEx")); c.partial = true; return c;
    }

    std::vector<std::vector<GROUP_AFFINITY>> packages;
    std::vector<std::vector<GROUP_AFFINITY>> numa;
    std::vector<uint32_t> numaNumbers;
    std::vector<std::vector<GROUP_AFFINITY>> cores;

    auto parse = [&](PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX p) {
      switch (p->Relationship) {
        case RelationProcessorCore: {
          auto& proc = p->Processor;
          std::vector<GROUP_AFFINITY> aff;
          for (WORD g = 0; g < proc.GroupCount && g < 32; ++g) aff.push_back(proc.GroupMask[g]);
          cores.push_back(std::move(aff));
          break;
        }
        case RelationProcessorPackage: {
          auto& proc = p->Processor;
          std::vector<GROUP_AFFINITY> aff;
          for (WORD g = 0; g < proc.GroupCount && g < 32; ++g) aff.push_back(proc.GroupMask[g]);
          packages.push_back(std::move(aff));
          break;
        }
        case RelationNumaNode: {
          auto& n = p->NumaNode;
          std::vector<GROUP_AFFINITY> aff;
          for (WORD g = 0; g < n.GroupCount && g < 32; ++g) aff.push_back(n.GroupMasks[g]);
          numa.push_back(std::move(aff));
          numaNumbers.push_back(n.NodeNumber);
          break;
        }
        default: break;
      }
    };
    BYTE* ptr = buffer.data();
    BYTE* end = buffer.data() + bytes;
    while (ptr < end) {
      auto p = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(ptr);
      parse(p);
      if (p->Size == 0) break;
      ptr += p->Size;
    }

    auto contains = [](const std::vector<GROUP_AFFINITY>& affs, WORD grp, KAFFINITY mask) -> bool {
      for (auto& a : affs) if (a.Group == grp && (a.Mask & mask)) return true;
      return false;
    };
    auto package_of = [&](WORD grp, KAFFINITY mask) -> int {
      for (size_t i = 0; i < packages.size(); ++i) if (contains(packages[i], grp, mask)) return static_cast<int>(i);
      return -1;
    };
    auto numa_of = [&](WORD grp, KAFFINITY mask) -> int {
      for (size_t i = 0; i < numa.size(); ++i) if (contains(numa[i], grp, mask)) return static_cast<int>(i);
      return -1;
    };

    auto prov = Provenance::discovered("windows.cpu_numa", "GetLogicalProcessorInformationEx", Confidence::AUTHORITATIVE, "10.0");

    auto add_contains = [&](const std::string& fr, const std::string& tr) {
      ContributedEdge e; e.from_ref = fr; e.to_ref = tr; e.type = EdgeType::CONTAINS; e.direction = EdgeDirection::DIRECTED;
      e.provenance = prov; e.confidence = Confidence::HIGH; c.edges.push_back(std::move(e));
    };
    auto add_local = [&](const std::string& fr, const std::string& tr) {
      ContributedEdge e; e.from_ref = fr; e.to_ref = tr; e.type = EdgeType::LOCAL_TO; e.direction = EdgeDirection::UNDIRECTED;
      e.provenance = prov; e.confidence = Confidence::MEDIUM; c.edges.push_back(std::move(e));
    };

    for (size_t ni = 0; ni < numa.size(); ++ni) {
      std::string numaRef = "numa:" + std::to_string(numaNumbers[ni]);
      ContributedNode n; n.ref = numaRef; n.type = NodeType::NUMA_NODE;
      n.name = "NUMA " + std::to_string(numaNumbers[ni]); n.native.numa_node = numaNumbers[ni];
      n.provenance = prov; n.capabilities = Capability::CPU_ADDRESSABLE | Capability::NUMA_LOCAL;
      c.nodes.push_back(std::move(n)); add_contains(machineRef, numaRef);
      std::string memRef = "memdom:" + std::to_string(numaNumbers[ni]);
      ContributedNode m; m.ref = memRef; m.type = NodeType::HOST_MEMORY_DOMAIN;
      m.name = "Host memory domain (NUMA " + std::to_string(numaNumbers[ni]) + ")"; m.native.numa_node = numaNumbers[ni];
      m.provenance = Provenance::inferred("windows.cpu_numa", "NUMA_NODE_RELATIONSHIP", Confidence::MEDIUM, "host memory domain per NUMA node");
      m.capabilities = Capability::CPU_ADDRESSABLE | Capability::NUMA_LOCAL | Capability::SHARED_MEMORY;
      c.nodes.push_back(std::move(m)); add_contains(numaRef, memRef);
    }

    for (size_t pi = 0; pi < packages.size(); ++pi) {
      std::string pkgRef = "pkg:" + std::to_string(pi);
      ContributedNode p; p.ref = pkgRef; p.type = NodeType::CPU_PACKAGE; p.name = "CPU Package " + std::to_string(pi);
      p.native.cpu_package = static_cast<uint32_t>(pi); p.provenance = prov;
      p.capabilities = Capability::CPU_ADDRESSABLE | Capability::NUMA_LOCAL;
      c.nodes.push_back(std::move(p)); add_contains(machineRef, pkgRef);
    }

    int threadCounter = 0;
    for (size_t ci = 0; ci < cores.size(); ++ci) {
      std::string coreRef = "core:" + std::to_string(ci);
      ContributedNode cn; cn.ref = coreRef; cn.type = NodeType::CPU_CORE; cn.name = "Core " + std::to_string(ci);
      cn.provenance = prov; cn.capabilities = Capability::CPU_ADDRESSABLE; cn.native.core_id = static_cast<uint32_t>(ci);
      for (auto& a : cores[ci]) { int p = package_of(a.Group, a.Mask); if (p >= 0) { cn.native.cpu_package = static_cast<uint32_t>(p); break; } }
      c.nodes.push_back(std::move(cn));
      std::string parentPkg = cn.native.cpu_package ? ("pkg:" + std::to_string(*cn.native.cpu_package)) : machineRef;
      add_contains(parentPkg, coreRef);
      for (auto& a : cores[ci]) {
        KAFFINITY mask = a.Mask; WORD grp = a.Group;
        for (int bit = 0; bit < 64; ++bit) {
          KAFFINITY one = (static_cast<KAFFINITY>(1) << bit);
          if (!(mask & one)) continue;
          int pkg = package_of(grp, one); int numa_idx = numa_of(grp, one);
          std::string tRef = "thread:g" + std::to_string(grp) + ":" + std::to_string(bit);
          ContributedNode t; t.ref = tRef; t.type = NodeType::CPU_THREAD;
          t.name = "CPU " + std::to_string(threadCounter); t.native.logical_processor_index = static_cast<uint32_t>(threadCounter);
          t.native.processor_group = static_cast<uint32_t>(grp); t.native.core_id = static_cast<uint32_t>(ci);
          if (pkg >= 0) t.native.cpu_package = static_cast<uint32_t>(pkg);
          if (numa_idx >= 0) t.native.numa_node = numaNumbers[static_cast<size_t>(numa_idx)];
          t.provenance = prov; t.capabilities = Capability::CPU_ADDRESSABLE;
          c.nodes.push_back(std::move(t)); add_contains(coreRef, tRef);
          if (numa_idx >= 0) add_local(tRef, "numa:" + std::to_string(numaNumbers[static_cast<size_t>(numa_idx)]));
          ++threadCounter;
        }
      }
    }
    c.partial = false; c.success = true;
    return c;
  }
};
#endif

#ifndef _WIN32
class CpuNumaProvider : public TopologyProvider {
 public:
  std::string name() const override { return "cpu_numa"; }
  std::string version() const override { return "1.0.0"; }
  Contribution discover(const DiscoveryContext&) override {
    Contribution c; c.provider = name(); c.version = version();
    c.warnings.push_back("cpu_numa provider only implemented on Windows");
    c.partial = true; c.success = false; return c;
  }
};
#endif

}  // namespace

std::shared_ptr<TopologyProvider> create_cpu_numa_provider() {
  return std::make_shared<CpuNumaProvider>();
}

}  // namespace topology_fabric