
// TopologyFabric/confidence.hpp - explicit confidence model.
#pragma once
#include "topology_fabric/types.hpp"
namespace topology_fabric {
// Confidence is defined in types.hpp (Confidence enum) plus a small helper set.
struct ConfidenceRanking {
  static int rank(Confidence c) noexcept {
    switch (c) {
      case Confidence::AUTHORITATIVE: return 5;
      case Confidence::HIGH:          return 4;
      case Confidence::MEDIUM:        return 3;
      case Confidence::LOW:           return 2;
      case Confidence::UNKNOWN:       return 1;
    }
    return 0;
  }
  static Confidence maximum(Confidence a, Confidence b) noexcept {
    return rank(a) >= rank(b) ? a : b;
  }
  // Confidence is only ever decreased on merge, never silently upgraded.
  static Confidence minimum(Confidence a, Confidence b) noexcept {
    return rank(a) <= rank(b) ? a : b;
  }
};
}  // namespace topology_fabric
