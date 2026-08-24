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
#define INITGUID
#include <windows.h>
#include <cfgmgr32.h>
#include <devpkey.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <cctype>
#endif

namespace topology_fabric {
namespace {

#ifdef _WIN32

static bool parse_hex16(const std::string& s, uint16_t& out) {
  if (s.empty()) return false;
  uint32_t v = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i]; uint32_t d;
    if (c >= '0' && c <= '9') d = static_cast<uint32_t>(c - '0');
    else if (c >= 'a' && c <= 'f') d = static_cast<uint32_t>(c - 'a' + 10);
    else if (c >= 'A' && c <= 'F') d = static_cast<uint32_t>(c - 'A' + 10);
    else return false;
    v = v * 16 + d;
  }
  if (v > 0xFFFF) return false;
  out = static_cast<uint16_t>(v);
  return true;
}

static bool parse_location(const std::string& s, uint16_t& bus, uint16_t& dev, uint16_t& func) {
  bus = 0; dev = 0; func = 0;
  auto get_int = [&](const char* key, uint16_t& out) -> bool {
    std::string hay = s;
    std::string k(key);
    auto pos = k == "bus" ? hay.rfind(k) : hay.find(k);
    if (pos == std::string::npos) return false;
    size_t i = pos + k.size();
    while (i < hay.size() && (hay[i] == ' ' || hay[i] == '\t' || hay[i] == ':')) ++i;
    std::string digits;
    while (i < hay.size() && std::isdigit(static_cast<unsigned char>(hay[i]))) digits.push_back(hay[i++]);
    if (digits.empty()) return false;
    long v = std::strtol(digits.c_str(), nullptr, 10);
    if (v < 0 || v > 0xFFFF) return false;
    out = static_cast<uint16_t>(v);
    return true;
  };
  bool b = get_int("bus", bus);
  bool d = get_int("device", dev);
  bool f = get_int("function", func);
  return b && d && f;
}

static std::optional<std::string> get_location(DEVINST dev) {
  DEVPROPTYPE type = 0;
  ULONG size = 0;
  CONFIGRET cr = CM_Get_DevNode_PropertyW(dev, &DEVPKEY_Device_LocationInfo, &type, nullptr, &size, 0);
  if (cr != CR_SUCCESS || size == 0) return std::nullopt;
  std::vector<wchar_t> buf(size / sizeof(wchar_t) + 1, L'\0');
  cr = CM_Get_DevNode_PropertyW(dev, &DEVPKEY_Device_LocationInfo, &type, reinterpret_cast<PBYTE>(buf.data()), &size, 0);
  if (cr != CR_SUCCESS) return std::nullopt;
  return winutil::widen_to_utf8(buf.data());
}

class PciProvider : public TopologyProvider {
 public:
  std::string name() const override { return "pci"; }
  std::string version() const override { return "1.0.0"; }
  Contribution discover(const DiscoveryContext&) override {
    Contribution c;
    c.provider = name(); c.version = version();
    mark_available(true);

    ULONG listSize = 0;
    CONFIGRET cr = CM_Get_Device_ID_List_SizeW(&listSize, nullptr, CM_GETIDLIST_FILTER_PRESENT);
    if (cr != CR_SUCCESS || listSize == 0) { c.warnings.push_back(winutil::last_error("CM_Get_Device_ID_List_SizeW")); c.partial = true; return c; }
    std::vector<wchar_t> ids(listSize + 2, L'\0');
    cr = CM_Get_Device_ID_ListW(nullptr, ids.data(), static_cast<ULONG>(ids.size()), CM_GETIDLIST_FILTER_PRESENT);
    if (cr != CR_SUCCESS) { c.warnings.push_back(winutil::last_error("CM_Get_Device_ID_ListW")); c.partial = true; return c; }

    std::unordered_map<std::string, DEVINST> devinsts;
    std::unordered_map<std::string, std::string> parentOf;
    std::vector<std::string> pciIds;
    std::unordered_map<std::string, uint16_t> vendor, device, sub;
    std::unordered_map<std::string, uint16_t> pbus, pdev, pfunc;

    for (const wchar_t* p = ids.data(); *p; ) {
      std::string id = winutil::widen_to_utf8(p);
      while (*p) ++p;
      ++p;
      if (id.empty()) continue;
      if (id.rfind("PCI\\", 0) != 0) continue;

      DEVINST dev = 0;
      std::wstring wid = winutil::utf8_to_widen(id);
      if (CM_Locate_DevNodeW(&dev, wid.data(), CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS) continue;
      devinsts[id] = dev;
      pciIds.push_back(id);

      std::size_t pos = std::string::npos;
      if ((pos = id.find("VEN_")) != std::string::npos) { uint16_t v = 0; if (pos + 8 <= id.size() && parse_hex16(id.substr(pos + 4, 4), v)) vendor[id] = v; }
      if ((pos = id.find("DEV_")) != std::string::npos) { uint16_t v = 0; if (pos + 8 <= id.size() && parse_hex16(id.substr(pos + 4, 4), v)) device[id] = v; }
      if ((pos = id.find("SUBSYS_")) != std::string::npos) { uint16_t v = 0; if (pos + 11 <= id.size() && parse_hex16(id.substr(pos + 7, 4), v)) sub[id] = v; }

      DEVINST parent = 0;
      if (CM_Get_Parent(&parent, dev, 0) == CR_SUCCESS && parent) {
        wchar_t pbuf[600] = {0};
        if (CM_Get_Device_IDW(parent, pbuf, 600, 0) == CR_SUCCESS) {
          std::string pid = winutil::widen_to_utf8(pbuf);
          parentOf[id] = pid;
        }
      }
      auto loc = get_location(dev);
            if (loc) { uint16_t b, dd, ff; if (parse_location(*loc, b, dd, ff)) { pbus[id] = b; pdev[id] = dd; pfunc[id] = ff; } }
    }

    std::string machineName;
    { char mb[256] = {}; DWORD sz = 256; if (::GetComputerNameExA(ComputerNameDnsHostname, mb, &sz)) machineName = mb; }
    std::string machineRef = "machine:" + machineName;
    auto prov = Provenance::discovered("windows.pci", "CM_Get_Device_ID_List/CM_Get_Parent", Confidence::HIGH, "10.0");

    auto add_contains = [&](const std::string& fr, const std::string& tr) {
      ContributedEdge e; e.from_ref = fr; e.to_ref = tr; e.type = EdgeType::CONTAINS; e.direction = EdgeDirection::DIRECTED;
      e.provenance = prov; e.confidence = Confidence::HIGH; c.edges.push_back(std::move(e));
    };

    std::unordered_map<std::string, bool> isParent;
    for (auto& [id, pid] : parentOf) if (devinsts.count(pid)) isParent[pid] = true;

    std::unordered_map<std::string, std::string> refOf;
    for (auto& id : pciIds) {
      bool hasPciParent = parentOf.count(id) && devinsts.count(parentOf[id]);
      bool isParentNode = isParent[id];
      std::string ref = "pci:" + id;
      refOf[id] = ref;
      ContributedNode n;
      n.ref = ref;
      // Root = topmost PCI entity (has downstream PCI children, but its parent is not a PCI device).
      // Bridge = PCI entity that is itself under a PCI device and has PCI children.
      // Endpoint = leaf PCI function (no PCI children).
      if (isParentNode && !hasPciParent) n.type = NodeType::PCI_ROOT;
      else if (isParentNode) n.type = NodeType::PCI_BRIDGE;
      else n.type = NodeType::PCI_DEVICE;
      n.name = "PCI " + id;
      if (vendor.count(id)) n.native.vendor_id = vendor[id];
      if (device.count(id)) n.native.device_id = device[id];
      if (sub.count(id)) n.native.subsystem_id = sub[id];
      n.native.pci_domain = 0;
      if (pbus.count(id)) n.native.pci_bus = pbus[id];
      if (pdev.count(id)) n.native.pci_device = pdev[id];
      if (pfunc.count(id)) n.native.pci_function = pfunc[id];
      n.provenance = prov;
      n.capabilities = Capability::PCI_CONNECTED | Capability::DMA_CAPABLE;
      c.nodes.push_back(std::move(n));
    }
    for (auto& id : pciIds) {
      auto it = parentOf.find(id);
      if (it != parentOf.end() && devinsts.count(it->second)) add_contains(refOf[it->second], refOf[id]);
      else add_contains(machineRef, refOf[id]);
    }
    c.partial = true;
    c.success = true;
    return c;
  }
};
#endif

#ifndef _WIN32
class PciProvider : public TopologyProvider {
 public:
  std::string name() const override { return "pci"; }
  std::string version() const override { return "1.0.0"; }
  Contribution discover(const DiscoveryContext&) override {
    Contribution c; c.provider = name(); c.version = version();
    c.warnings.push_back("pci provider only implemented on Windows");
    c.partial = true; c.success = false; return c;
  }
};
#endif

}  // namespace

std::shared_ptr<TopologyProvider> create_pci_provider() {
  return std::make_shared<PciProvider>();
}

}  // namespace topology_fabric