#include "topology_fabric/provider.hpp"
#include "topology_fabric/provenance.hpp"
#include "provider_factories.hpp"
#include "windows_util.hpp"
#include <string>
#include <vector>
#include <cstdio>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace topology_fabric {
namespace {

#ifdef _WIN32


static std::string volume_name_of(const std::wstring& root) {
  wchar_t name[MAX_PATH + 1] = {0};
  wchar_t fs[MAX_PATH + 1] = {0};
  DWORD serial = 0;
  DWORD maxComp = 0;
  DWORD flags = 0;
  if (!::GetVolumeInformationW(root.c_str(), name, MAX_PATH, &serial, &maxComp, &flags, fs, MAX_PATH))
    return {};
  std::string label = winutil::widen_to_utf8(name);
  if (label.empty() && serial != 0) label = "vol-" + std::to_string(serial);
  return label;
}

class StorageProvider : public TopologyProvider {
 public:
  std::string name() const override { return "storage"; }
  std::string version() const override { return "1.0.0"; }
  Contribution discover(const DiscoveryContext&) override {
    Contribution c;
    c.provider = name(); c.version = version();
    mark_available(true);
    auto prov = Provenance::discovered("windows.storage", "GetLogicalDrives/GetDiskFreeSpaceEx/GetVolumeInformation", Confidence::HIGH, "10.0");
    DWORD mask = ::GetLogicalDrives();
    for (char ch = 'A'; ch <= 'Z'; ++ch) {
      if (!(mask & (1u << (ch - 'A')))) continue;
      std::wstring root = std::wstring(1, ch) + L":\\";
      ULARGE_INTEGER freeB{}, total{}, totalFree{};
      if (!::GetDiskFreeSpaceExW(root.c_str(), &freeB, &total, &totalFree)) continue;
      std::string letter(1, ch);
      std::string ref = "storage:vol:" + letter;
      std::string label = volume_name_of(root);
      wchar_t fs[MAX_PATH + 1] = {0};
      DWORD serial = 0, maxComp = 0, flags = 0;
      ::GetVolumeInformationW(root.c_str(), nullptr, 0, &serial, &maxComp, &flags, fs, MAX_PATH);
      ContributedNode n;
      n.ref = ref;
      n.type = NodeType::STORAGE_DEVICE;
      n.name = label.empty() ? ("Drive " + letter + ":") : (label + " (" + letter + ":)");
      n.native.storage_id = "vol:" + letter;
      n.native.storage_device_path = letter + ":\\";
      n.provenance = prov;
      n.capabilities = Capability::STORAGE_BACKED | Capability::DMA_CAPABLE;
      n.properties.emplace("storage.letter", PropertyValue(letter));
      n.properties.emplace("storage.total_bytes", PropertyValue(static_cast<uint64_t>(total.QuadPart)));
      n.properties.emplace("storage.free_bytes", PropertyValue(static_cast<uint64_t>(totalFree.QuadPart)));
      if (label.empty() == false) n.properties.emplace("storage.volume_label", PropertyValue(label));
      if (fs[0]) n.properties.emplace("storage.filesystem", PropertyValue(winutil::widen_to_utf8(fs)));
      c.nodes.push_back(std::move(n));
    }
    c.partial = true;  // physical-disk / PCI association is out of scope for this provider
    c.success = true;
    return c;
  }
};
#endif

#ifndef _WIN32
class StorageProvider : public TopologyProvider {
 public:
  std::string name() const override { return "storage"; }
  std::string version() const override { return "1.0.0"; }
  Contribution discover(const DiscoveryContext&) override {
    Contribution c; c.provider = name(); c.version = version();
    c.warnings.push_back("storage provider only implemented on Windows");
    c.partial = true; c.success = false; return c;
  }
};
#endif

}  // namespace

std::shared_ptr<TopologyProvider> create_storage_provider() {
  return std::make_shared<StorageProvider>();
}

}  // namespace topology_fabric