
// TopologyFabric/provenance.hpp - provenance of every nontrivial topology fact.
#pragma once
#include <cstdint>
#include <string>
#include "topology_fabric/types.hpp"
#include "topology_fabric/confidence.hpp"

namespace topology_fabric {

// Describes how, where, and with what certainty a topology fact was established.
struct Provenance {
  ProvenanceKind kind = ProvenanceKind::UNKNOWN;
  Confidence confidence = Confidence::UNKNOWN;
  std::string provider;           // e.g. "windows.pci", "cuda"
  std::string api;                // e.g. "GetLogicalProcessorInformationEx", "cudaGetDeviceProperties"
  std::string detail;             // free-form note
  int64_t timestamp_ms = 0;       // epoch ms (0 = unset)
  std::string schema_version = "1.0";
  std::string provider_version;   // e.g. "12.9"

  static Provenance discovered(std::string provider, std::string api, Confidence c,
                               std::string provider_version = {}, std::string detail = {});
  static Provenance inferred(std::string provider, std::string api, Confidence c,
                             std::string detail = {});
  static Provenance measured(std::string provider, std::string api, Confidence c,
                             std::string provider_version = {}, std::string detail = {});
  static Provenance user_supplied(Confidence c, std::string detail = {});
  static Provenance unknown() { return Provenance{}; }

  bool operator==(const Provenance&) const noexcept = default;
};

}  // namespace topology_fabric
