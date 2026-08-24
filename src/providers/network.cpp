#include "topology_fabric/provider.hpp"
#include "topology_fabric/provenance.hpp"
#include "topology_fabric/identity.hpp"
#include "provider_factories.hpp"
#include "windows_util.hpp"
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#endif

namespace topology_fabric {
namespace {

#ifdef _WIN32


static std::string mac_to_str(const unsigned char* addr, int len) {
  char buf[64] = {0};
  int pos = 0;
  for (int i = 0; i < len; ++i) {
    pos += std::snprintf(buf + pos, sizeof(buf) - static_cast<size_t>(pos), "%02x", addr[i]);
    if (i + 1 < len) buf[pos++] = ':';
  }
  return std::string(buf);
}

class NetworkProvider : public TopologyProvider {
 public:
  std::string name() const override { return "network"; }
  std::string version() const override { return "1.0.0"; }
  Contribution discover(const DiscoveryContext&) override {
    Contribution c;
    c.provider = name(); c.version = version();
    mark_available(true);
    auto prov = Provenance::discovered("windows.network", "GetAdaptersAddresses", Confidence::HIGH, "10.0");
    ULONG size = 0;
    DWORD r = ::GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST, nullptr, nullptr, &size);
    if (r != ERROR_BUFFER_OVERFLOW || size == 0) { c.warnings.push_back("GetAdaptersAddresses enumeration empty"); c.partial = true; c.success = false; return c; }
    std::vector<unsigned char> buf(size);
    auto* aa = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data());
    r = ::GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST, nullptr, aa, &size);
    if (r != ERROR_SUCCESS) { c.warnings.push_back("GetAdaptersAddresses failed"); c.partial = true; c.success = false; return c; }

    for (auto* a = aa; a; a = a->Next) {
      if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
      std::string name = a->FriendlyName ? winutil::widen_to_utf8(a->FriendlyName) : std::string("adapter-") + std::to_string(a->IfIndex);
      std::string desc = a->Description ? winutil::widen_to_utf8(a->Description) : std::string();
      std::string ref = "net:" + std::to_string(a->IfIndex);
      ContributedNode n;
      n.ref = ref;
      n.type = NodeType::NETWORK_INTERFACE;
      n.name = name;
      n.native.network_interface_name = name;
      if (a->PhysicalAddressLength <= 32)
        n.native.network_hardware_id = mac_to_str(a->PhysicalAddress, static_cast<int>(a->PhysicalAddressLength));
      n.provenance = prov;
      n.capabilities = Capability::NETWORK_CONNECTED | Capability::DMA_CAPABLE;
      n.properties.emplace("network.ifindex", PropertyValue(static_cast<int64_t>(a->IfIndex)));
      n.properties.emplace("network.mtu", PropertyValue(static_cast<int64_t>(a->Ipv4Metric == 0 ? a->Mtu : a->Mtu)));
      n.properties.emplace("network.oper_status", PropertyValue(static_cast<int64_t>(a->OperStatus)));
      n.properties.emplace("network.type", PropertyValue(static_cast<int64_t>(a->IfType)));
      if (!desc.empty()) n.properties.emplace("network.description", PropertyValue(desc));
      // IPv4/IPv6 addresses.
      std::vector<std::string> addrs;
      for (auto* u = a->FirstUnicastAddress; u; u = u->Next) {
        char host[128] = {0};
        if (u->Address.lpSockaddr->sa_family == AF_INET) {
          auto* sa = reinterpret_cast<const sockaddr_in*>(u->Address.lpSockaddr);
          ::inet_ntop(AF_INET, &sa->sin_addr, host, sizeof(host));
          addrs.emplace_back(host);
        } else if (u->Address.lpSockaddr->sa_family == AF_INET6) {
          auto* sa = reinterpret_cast<const sockaddr_in6*>(u->Address.lpSockaddr);
          ::inet_ntop(AF_INET6, &sa->sin6_addr, host, sizeof(host));
          addrs.emplace_back(host);
        }
      }
      if (!addrs.empty()) n.properties.emplace("network.addresses", PropertyValue::make_string_array(std::move(addrs)));
      c.nodes.push_back(std::move(n));
    }
    c.partial = true;  // PCI/NUMA association not resolved by this provider
    c.success = true;
    return c;
  }
};
#endif

#ifndef _WIN32
class NetworkProvider : public TopologyProvider {
 public:
  std::string name() const override { return "network"; }
  std::string version() const override { return "1.0.0"; }
  Contribution discover(const DiscoveryContext&) override {
    Contribution c; c.provider = name(); c.version = version();
    c.warnings.push_back("network provider only implemented on Windows");
    c.partial = true; c.success = false; return c;
  }
};
#endif

}  // namespace

std::shared_ptr<TopologyProvider> create_network_provider() {
  return std::make_shared<NetworkProvider>();
}

}  // namespace topology_fabric