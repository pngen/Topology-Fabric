
#include "topology_fabric/provider.hpp"
#include "topology_fabric/provenance.hpp"
#include "windows_util.hpp"
#include <sstream>

namespace topology_fabric {
namespace {

#ifdef _WIN32
class HostProvider : public TopologyProvider {
 public:
  std::string name() const override { return "host"; }
  std::string version() const override { return "1.0.0"; }
  Contribution discover(const DiscoveryContext& ctx) override {
    Contribution c;
    c.provider = name();
    c.version = version();
    mark_available(true);

    char mbuf[256] = {};
    std::string machine;
    DWORD size = 256;
    if (::GetComputerNameExA(ComputerNameDnsHostname, mbuf, &size)) machine = mbuf;
    else if (::GetComputerNameA(mbuf, &size)) machine = mbuf;

    // OS version via RtlGetVersion for accurate builds.
    std::string os = "windows";
    {
      typedef LONG (WINAPI *RtlGetVersionFn)(PRTL_OSVERSIONINFOW);
      HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
      if (ntdll) {
        auto fn = (RtlGetVersionFn)::GetProcAddress(ntdll, "RtlGetVersion");
        if (fn) {
          RTL_OSVERSIONINFOW v{}; v.dwOSVersionInfoSize = sizeof(v);
          if (fn(&v) == 0) {
            std::ostringstream osx;
            osx << "windows " << v.dwMajorVersion << "." << v.dwMinorVersion << "." << v.dwBuildNumber;
            os = osx.str();
          }
        }
      }
    }

    DWORD grp = ::GetActiveProcessorGroupCount();
    DWORD logical = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);

    MEMORYSTATUSEX mem{}; mem.dwLength = sizeof(mem);
    bool memOk = ::GlobalMemoryStatusEx(&mem) != 0;

    ContributedNode node;
    node.ref = std::string("machine:") + machine;
    node.type = NodeType::MACHINE;
    node.name = machine.empty() ? "machine" : machine;
    node.native.machine_name = machine;
    node.native.os_version = os;
    node.provenance = Provenance::discovered("windows.host", "GetComputerNameEx/RtlGetVersion/GetActiveProcessorCount/GlobalMemoryStatusEx",
                                             Confidence::HIGH, "10.0");
    node.capabilities = Capability::CPU_ADDRESSABLE | Capability::NUMA_LOCAL | Capability::SHARED_MEMORY;
    node.properties.emplace("os.version", PropertyValue(os));
    node.properties.emplace("machine.name", PropertyValue(machine));
    node.properties.emplace("processor.logical_count", PropertyValue(static_cast<int64_t>(logical)));
    node.properties.emplace("processor.group_count", PropertyValue(static_cast<int64_t>(grp)));
    if (memOk) {
      node.properties.emplace("memory.total_bytes", PropertyValue(static_cast<uint64_t>(mem.ullTotalPhys)));
      node.properties.emplace("memory.available_bytes", PropertyValue(static_cast<uint64_t>(mem.ullAvailPhys)));
      node.properties.emplace("memory.usage_percent", PropertyValue(static_cast<double>(mem.dwMemoryLoad)));
    }
    c.nodes.push_back(std::move(node));
    c.partial = false;
    c.success = true;
    return c;
  }
};
#endif

// Non-Windows fallback.
#ifndef _WIN32
class HostProvider : public TopologyProvider {
 public:
  std::string name() const override { return "host"; }
  std::string version() const override { return "1.0.0"; }
  Contribution discover(const DiscoveryContext&) override {
    Contribution c; c.provider = name(); c.version = version();
    c.warnings.push_back("host provider is only implemented on Windows");
    c.partial = true; c.success = false;
    return c;
  }
};
#endif

}  // namespace
std::shared_ptr<TopologyProvider> create_host_provider() {
  return std::make_shared<HostProvider>();
}

}  // namespace topology_fabric