
#include "topology_fabric/provenance.hpp"

namespace topology_fabric {

Provenance Provenance::discovered(std::string provider, std::string api, Confidence c,
                                  std::string provider_version, std::string detail) {
  Provenance p;
  p.kind = ProvenanceKind::DISCOVERED;
  p.provider = std::move(provider);
  p.api = std::move(api);
  p.confidence = c;
  p.provider_version = std::move(provider_version);
  p.detail = std::move(detail);
  return p;
}

Provenance Provenance::inferred(std::string provider, std::string api, Confidence c,
                                std::string detail) {
  Provenance p;
  p.kind = ProvenanceKind::INFERRED;
  p.provider = std::move(provider);
  p.api = std::move(api);
  p.confidence = c;
  p.detail = std::move(detail);
  return p;
}

Provenance Provenance::measured(std::string provider, std::string api, Confidence c,
                                std::string provider_version, std::string detail) {
  Provenance p;
  p.kind = ProvenanceKind::MEASURED;
  p.provider = std::move(provider);
  p.api = std::move(api);
  p.confidence = c;
  p.provider_version = std::move(provider_version);
  p.detail = std::move(detail);
  return p;
}

Provenance Provenance::user_supplied(Confidence c, std::string detail) {
  Provenance p;
  p.kind = ProvenanceKind::USER_SUPPLIED;
  p.confidence = c;
  p.detail = std::move(detail);
  return p;
}

}  // namespace topology_fabric
