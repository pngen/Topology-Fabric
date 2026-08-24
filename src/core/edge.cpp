
#include "topology_fabric/edge.hpp"

namespace topology_fabric {

TopologyEdge::Key TopologyEdge::key() const {
  if (direction == EdgeDirection::UNDIRECTED) {
    // canonical order for undirected edges
    if (target < source) return Key{target, source, type};
    return Key{source, target, type};
  }
  return Key{source, target, type};
}

}  // namespace topology_fabric
