
// TopologyFabric/serialization.hpp - versioned topology snapshot serialization.
#pragma once
#include <memory>
#include <string>
#include "topology_fabric/snapshot.hpp"
#include "topology_fabric/json.hpp"

namespace topology_fabric {

inline constexpr const char* kSnapshotSchemaVersion = "1.0.0";

// Serialize a snapshot to JSON text.
std::string serialize_snapshot_json(const TopologySnapshot& snap, bool pretty = false);

// Deserialize a snapshot from JSON text. Bounds-limited and reject-on-malformed.
// Throws TopologyError on malformed/oversized input.
std::shared_ptr<const TopologySnapshot> deserialize_snapshot_json(const std::string& json,
                                                                  const Bounds& bounds = {});

// Import a synthetic/imported snapshot from JSON. Marks synthetic=true.
// Throws TopologyError on malformed/oversized input.
std::shared_ptr<const TopologySnapshot> import_synthetic_json(const std::string& json,
                                                             const Bounds& bounds = {});

namespace detail {
json::Value snapshot_to_json(const TopologySnapshot& snap);
json::Value property_to_json(const PropertyValue& v);
PropertyValue property_from_json(const json::Value& v);
json::Value native_to_json(const NativeIdentity& n);
NativeIdentity native_from_json(const json::Value& v);
json::Value provenance_to_json(const Provenance& p);
Provenance provenance_from_json(const json::Value& v);
}  // namespace detail

}  // namespace topology_fabric
