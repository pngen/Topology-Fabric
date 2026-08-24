# Serialization

Topology Fabric serializes a snapshot to bounded, versioned JSON and deserializes it back. This
document describes the JSON format, schema versioning, and the round-trip guarantees as implemented
in serialization.hpp, serialization.cpp, and json.hpp.

## Schema version

static constexpr const char* kSnapshotSchemaVersion = "1.0.0";

The runtime's public functions use the built-in bounded JSON value (json.hpp), not a third-party
library. See SECURITY.md for the bounds and rejection behavior.

## Public functions

- std::string serialize_snapshot_json(const TopologySnapshot& snap, bool pretty = false) —
  emits JSON.
- std::shared_ptr<const TopologySnapshot> deserialize_snapshot_json(const std::string& json,
  const Bounds& bounds = {}) — parses and rebuilds. Throws TopologyError(MALFORMED_DATA) on
  malformed input and TopologyError(OVERSIZED) when the input exceeds max_snapshot_bytes or a
  node/edge count exceeds the bound.
- std::shared_ptr<const TopologySnapshot> import_synthetic_json(const std::string& json,
  const Bounds& bounds = {}) — deserializes and keeps the document's synthetic flag.

TopologyRuntime::serialize(snap, pretty), deserialize(json), import_synthetic(json) wrap these and
record import telemetry (record_imported_snapshot(ok) on success, malformed_imports_rejected on
failure).

## Document structure

The root is an object with:

- "schema": kSnapshotSchemaVersion
- "snapshot_id", "generation", "created_ms", "machine_identity", "provider_versions"
- "format": "topology_fabric_snapshot"
- "nodes": array of node objects
- "edges": array of edge objects
- "warnings": array
- "partial_discovery": bool
- "synthetic": bool
- "validation": { "ok", "errors": [], "warnings": [] }

## Node object

Each node has:

- "id": 32-hex-char TopologyNodeId string
- "type": NodeType string (to_string)
- "category", "name", "display_name"
- "native": the NativeIdentity object
- "capabilities": uint64 bitmask
- "confidence": Confidence string
- "provenance": the Provenance object
- "synthetic": bool
- "properties": object of PropertyValue -> JSON

## NativeIdentity object

native_to_json emits pci_domain, pci_bus, pci_device, pci_function, vendor_id, device_id,
subsystem_id, subsystem_vendor_id (each uint or null), numa_node, cpu_package, core_id,
processor_group, logical_processor_index, cuda_ordinal (uint or null), plus string fields
cuda_uuid, name, network_interface_name, network_hardware_id, storage_id, storage_device_path,
machine_name, os_version. An absent optional serializes as null.

## Provenance object

provenance_to_json emits kind, confidence, provider, api, detail, timestamp_ms, schema_version,
provider_version.

## Edge object

Each edge has source, target (hex ids), type, direction, confidence, provenance, and optional
width, pcie_generation, bandwidth_bps, latency_ns, hop_count, locality_score, accessible,
peer_capability, policy_version, and properties.

## PropertyValue <-> JSON

property_to_json maps the variant: nullptr -> null, bool -> bool, int64_t -> int (Int),
uint64_t -> uint (Uint), double -> Double, string -> String, vector<string> -> array of strings,
vector<int64_t> -> array of integers. property_from_json reverses it, decoding an array as a
string array when its first element is a string, else as an int array; a JSON array of mixed or
object elements falls back to PropertyValue(nullptr).

## Round-trip fidelities

- The immutable snapshot is rebuildable exactly from its JSON for the fields that are serialized
  (id, type, native, provenance, confidence, capabilities, properties, edges, validation,
  schema). Deserialization runs through SnapshotBuilder, which re-enforces Bounds and re-runs
  validation; a snapshot that was valid remains valid after a round trip.
- Node ids are stable 32-hex strings; both try_from_hex (returns false on any malformed hex) and
  from_hex (throws std::invalid_argument) are used on the read path.
- Pretty mode uses json::WriteMode::Pretty (two-space indentation); compact mode uses
  WriteMode::Compact.

## Bounded JSON parser

json.hpp implements a bounded parser with ParseOptions: max_depth = 128, max_nodes = 4<<20
(4,194,304 total values), max_string_bytes = 1<<24 (16 MiB per string), and
allow_nan_inf = false. ParseError enumerates UnexpectedChar, UnexpectedEnd, TrailGarbage,
DepthExceeded, SizeExceeded, InvalidNumber, InvalidEscape, InvalidUtf8. Any of these yields a
non-ok ParseResult and, at the snapshot layer, a TopologyError(MALFORMED_DATA). Empty or
wrong-format documents are rejected (format not "topology_fabric_snapshot", missing nodes/edges
array, node without an id, node entry not an object, etc.). See SECURITY.md.
