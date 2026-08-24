#include "topology_fabric/provider.hpp"
#include "topology_fabric/provenance.hpp"
#include <string>
#include <vector>
#include <cstdint>
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

// Stable CUDA driver-API attribute ids (ABI-stable driver constants).
enum CudaAttr {
  kMaxThreadsPerBlock = 1,
  kTotalConstantMemory = 9,
  kMultiprocessorCount = 16,
  kCanMapHostMemory = 19,
  kConcurrentKernels = 31,
  kPciBusId = 33,
  kPciDeviceId = 34,
  kMaxThreadsPerMultiprocessor = 39,
  kAsyncEngineCount = 40,
  kUnifiedAddressing = 41,
  kPciDomainId = 50,
  kComputeCapabilityMajor = 75,
  kComputeCapabilityMinor = 76,
  kConcurrentManagedAccess = 81,
  kManagedMemory = 83,
  kMultiGpuBoard = 84,
  kPageableMemoryAccess = 88,
};

#ifdef _WIN32
struct CuUuid { unsigned char x[16]; };
typedef int CUdevice;
typedef int CUresult;
#define TF_OK 0

// Dynamically loads nvcuda.dll and resolves only the functions we need.
class CudaDriver {
 public:
  CudaDriver() {
    HMODULE h = LoadLibraryW(L"nvcuda.dll");
    if (!h) return;
    hm_ = h;
    loaded_ = true;
    cuInit = (CuInitFn)resolve("cuInit");
    cuDeviceGetCount = (CntFn)resolve("cuDeviceGetCount");
    cuDeviceGet = (DevFn)resolve("cuDeviceGet");
    cuDeviceGetName = (NameFn)resolve("cuDeviceGetName");
    cuDeviceGetAttribute = (AttrFn)resolve("cuDeviceGetAttribute");
    cuDeviceTotalMem = (MemFn)resolve("cuDeviceTotalMem");
    cuDeviceComputeCapability = (CapsFn)resolve("cuDeviceComputeCapability");
    cuDeviceCanAccessPeer = (PeerFn)resolve("cuDeviceCanAccessPeer");
    cuDeviceGetUuid = (UuidFn)resolve("cuDeviceGetUuid");
    cuDeviceGetPCIBusId = (PciStrFn)resolve("cuDeviceGetPCIBusId");
    // Initialize the driver.
    if (cuInit && cuInit(0) != TF_OK) { initialized_ = false; }
    else if (cuInit) { initialized_ = true; }
  }
  ~CudaDriver() { if (hm_) FreeLibrary(hm_); }

  bool ok() const { return loaded_ && initialized_ && cuDeviceGetCount; }

  typedef CUresult (*CuInitFn)(unsigned int);
  typedef CUresult (*CntFn)(int*);
  typedef CUresult (*DevFn)(CUdevice*, int);
  typedef CUresult (*NameFn)(char*, int, CUdevice);
  typedef CUresult (*AttrFn)(int*, int, CUdevice);
  typedef CUresult (*MemFn)(size_t*, CUdevice);
  typedef CUresult (*CapsFn)(int*, int*, CUdevice);
  typedef CUresult (*PeerFn)(int*, CUdevice, CUdevice);
  typedef CUresult (*UuidFn)(CuUuid*, CUdevice);
  typedef CUresult (*PciStrFn)(char*, int, CUdevice);

  CuInitFn cuInit = nullptr;
  CntFn cuDeviceGetCount = nullptr;
  DevFn cuDeviceGet = nullptr;
  NameFn cuDeviceGetName = nullptr;
  AttrFn cuDeviceGetAttribute = nullptr;
  MemFn cuDeviceTotalMem = nullptr;
  CapsFn cuDeviceComputeCapability = nullptr;
  PeerFn cuDeviceCanAccessPeer = nullptr;
  UuidFn cuDeviceGetUuid = nullptr;
  PciStrFn cuDeviceGetPCIBusId = nullptr;

 private:
  HMODULE hm_ = nullptr;
  bool loaded_ = false;
  bool initialized_ = false;
  void* resolve(const char* name) { return (void*)GetProcAddress(hm_, name); }
};

bool get_attr(const CudaDriver& d, CUdevice dev, int attrib, int& out) {
  if (!d.cuDeviceGetAttribute) return false;
  return d.cuDeviceGetAttribute(&out, attrib, dev) == TF_OK;
}

class CudaProvider : public TopologyProvider {
 public:
  std::string name() const override { return "cuda"; }
  std::string version() const override { return "1.0.0"; }
  Contribution discover(const DiscoveryContext&) override {
    Contribution c;
    c.provider = name(); c.version = version();
    CudaDriver d;
    if (!d.ok()) {
      c.warnings.push_back("CUDA driver API (nvcuda.dll) not available; CUDA discovery skipped");
      c.partial = true; c.success = false;
      mark_available(false);
      return c;
    }
    mark_available(true);
    int count = 0;
    if (d.cuDeviceGetCount(&count) != TF_OK || count <= 0) {
      c.warnings.push_back("no CUDA devices reported");
      c.partial = true; c.success = true;
      return c;
    }
    auto prov = Provenance::discovered("cuda", "cuDeviceGetCount/cuDeviceGetAttribute", Confidence::AUTHORITATIVE, "driver");
    auto infer = Provenance::inferred("cuda", "cuDeviceGetAttribute", Confidence::MEDIUM, "accelerator memory domain");

    // Peer capability matrix (shape count x count). Observational by default.
    std::vector<std::vector<bool>> peer(count, std::vector<bool>(count, false));
    if (d.cuDeviceCanAccessPeer) {
      for (int i = 0; i < count; ++i) {
        CUdevice a = 0; if (d.cuDeviceGet(&a, i) != TF_OK) continue;
        for (int j = 0; j < count; ++j) {
          CUdevice b = 0; if (d.cuDeviceGet(&b, j) != TF_OK) continue;
          int acc = 0;
          if (d.cuDeviceCanAccessPeer(&acc, a, b) == TF_OK) peer[i][j] = (acc != 0);
        }
      }
    }

    std::vector<std::string> devRefs;
    for (int ord = 0; ord < count; ++ord) {
      CUdevice dev = 0;
      if (d.cuDeviceGet(&dev, ord) != TF_OK) continue;
      char name[256] = {0};
      d.cuDeviceGetName(name, sizeof(name), dev);
      std::string devName = name;
      char sbuf[32] = {0};
      if (d.cuDeviceGetPCIBusId) d.cuDeviceGetPCIBusId(sbuf, sizeof(sbuf), dev);
      CuUuid uuid{};
      bool haveUuid = d.cuDeviceGetUuid && d.cuDeviceGetUuid(&uuid, dev) == TF_OK;

      int major = 0, minor = 0; d.cuDeviceComputeCapability(&major, &minor, dev);
      int pciBus = 0, pciDev = 0, pciDom = 0;
      get_attr(d, dev, kPciBusId, pciBus);
      get_attr(d, dev, kPciDeviceId, pciDev);
      get_attr(d, dev, kPciDomainId, pciDom);
      size_t memBytes = 0; if (d.cuDeviceTotalMem) d.cuDeviceTotalMem(&memBytes, dev);
      int uva = 0, managed = 0, asyncEng = 0, concKern = 0, canMapHost = 0, pageable = 0;
      get_attr(d, dev, kUnifiedAddressing, uva);
      get_attr(d, dev, kManagedMemory, managed);
      get_attr(d, dev, kAsyncEngineCount, asyncEng);
      get_attr(d, dev, kConcurrentKernels, concKern);
      get_attr(d, dev, kCanMapHostMemory, canMapHost);
      get_attr(d, dev, kPageableMemoryAccess, pageable);

      std::string uuidStr;
      if (haveUuid) {
        char h[33]; for (int i = 0; i < 16; ++i) std::snprintf(h + 2 * i, 3, "%02x", uuid.x[i]);
        h[32] = '\0'; uuidStr = h;
      }
      std::string ref = haveUuid ? ("cuda:uuid:" + uuidStr) : ("cuda:ord:" + std::to_string(ord));

      ContributedNode n;
      n.ref = ref;
      devRefs.push_back(ref);
      n.type = NodeType::ACCELERATOR;
      n.name = devName.empty() ? ("CUDA device " + std::to_string(ord)) : devName;
      n.native.name = devName;
      n.native.cuda_ordinal = static_cast<uint32_t>(ord);
      n.native.cuda_uuid = uuidStr;
      n.native.pci_domain = static_cast<uint16_t>(pciDom);
      n.native.pci_bus = static_cast<uint16_t>(pciBus);
      n.native.pci_device = static_cast<uint16_t>(pciDev);
      n.native.pci_function = 0;
      n.provenance = prov;
      Capability caps = Capability::DEVICE_ADDRESSABLE | Capability::DMA_CAPABLE;
      if (uva) { caps = caps | Capability::CPU_ADDRESSABLE; }
      if (canMapHost) { caps = caps | Capability::STAGED_TRANSFER | Capability::DIRECT_TRANSFER; }
      if (managed) { caps = caps | Capability::SHARED_MEMORY | Capability::COHERENT; }
      bool anyPeer = false;
      for (int j = 0; j < count; ++j) if (peer[ord][j]) anyPeer = true;
      if (anyPeer) caps = caps | Capability::PEER_ACCESS;
      n.capabilities = caps;
      n.properties.emplace("cuda.ordinal", PropertyValue(static_cast<int64_t>(ord)));
      n.properties.emplace("cuda.compute_capability_major", PropertyValue(static_cast<int64_t>(major)));
      n.properties.emplace("cuda.compute_capability_minor", PropertyValue(static_cast<int64_t>(minor)));
            // The driver occasionally reports an implausible sentinel (e.g. 0xFFFFFFFF)
      // on WDDM; be honest and only claim a plausible total.
      const bool memSane = (memBytes > (1ull << 30) && memBytes < (1ull << 40) && memBytes != 0xFFFFFFFFull);
      if (memSane) { n.properties.emplace("cuda.total_memory", PropertyValue(static_cast<uint64_t>(memBytes))); }
      else { n.properties.emplace("cuda.total_memory_reported", PropertyValue(static_cast<uint64_t>(memBytes))); n.properties.emplace("cuda.total_memory_unreliable", PropertyValue(true)); }
      n.properties.emplace("cuda.unified_addressing", PropertyValue(uva != 0));
      n.properties.emplace("cuda.managed_memory", PropertyValue(managed != 0));
      n.properties.emplace("cuda.async_engine_count", PropertyValue(static_cast<int64_t>(asyncEng)));
      n.properties.emplace("cuda.concurrent_kernels", PropertyValue(concKern != 0));
      n.properties.emplace("cuda.can_map_host_memory", PropertyValue(canMapHost != 0));
      n.properties.emplace("cuda.pageable_memory_access", PropertyValue(pageable != 0));
      if (haveUuid) n.properties.emplace("cuda.uuid", PropertyValue(uuidStr));
      c.nodes.push_back(std::move(n));

      // Accelerator memory (VRAM) domain.
      std::string memRef = ref + ":mem";
      ContributedNode m;
      m.ref = memRef;
      m.type = NodeType::ACCELERATOR_MEMORY_DOMAIN;
      m.name = devName + " VRAM";
      m.native.cuda_ordinal = static_cast<uint32_t>(ord);
      m.provenance = infer;
      m.capabilities = Capability::DEVICE_ADDRESSABLE;
      m.properties.emplace("cuda.total_memory", PropertyValue(static_cast<uint64_t>(memBytes)));
      c.nodes.push_back(std::move(m));
      ContributedEdge e;
      e.from_ref = ref; e.to_ref = memRef; e.type = EdgeType::CONTAINS;
      e.direction = EdgeDirection::DIRECTED;
      e.provenance = prov; e.confidence = Confidence::AUTHORITATIVE;
      c.edges.push_back(std::move(e));
    }

    // Peer edges (undirected) when a peer pair is accessible.
    for (int i = 0; i < count; ++i) {
      for (int j = i + 1; j < count; ++j) {
        if (peer[i][j]) {
          ContributedEdge e;
          e.from_ref = devRefs[static_cast<size_t>(i)];
          e.to_ref = devRefs[static_cast<size_t>(j)];
          e.type = EdgeType::PEER_TO;
          e.direction = EdgeDirection::UNDIRECTED;
          e.provenance = Provenance::discovered("cuda", "cuDeviceCanAccessPeer", Confidence::AUTHORITATIVE, "driver");
          e.confidence = Confidence::AUTHORITATIVE;
          e.peer_capability = 1;
          c.edges.push_back(std::move(e));
        }
      }
    }
    c.partial = false;
    c.success = true;
    return c;
  }
};

#else

class CudaProvider : public TopologyProvider {
 public:
  std::string name() const override { return "cuda"; }
  std::string version() const override { return "1.0.0"; }
  Contribution discover(const DiscoveryContext&) override {
    Contribution c; c.provider = name(); c.version = version();
    c.warnings.push_back("CUDA provider only supported on Windows");
    c.partial = true; c.success = false; return c;
  }
};

#endif

}  // namespace

std::shared_ptr<TopologyProvider> create_cuda_provider() {
  return std::make_shared<CudaProvider>();
}

}  // namespace topology_fabric