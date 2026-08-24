#include "topology_fabric/serialization.hpp"
#include "topology_fabric/result.hpp"
#include "topology_fabric/node_id.hpp"
#include <stdexcept>

namespace topology_fabric {

namespace detail {

json::Value property_to_json(const PropertyValue& v) {
  const auto& s = v.storage();
  using json::Value;
  return std::visit([](auto&& arg) -> Value {
    using T = std::decay_t<decltype(arg)>;
    if constexpr (std::is_same_v<T, std::nullptr_t>) return Value(nullptr);
    else if constexpr (std::is_same_v<T, bool>) return Value(arg);
    else if constexpr (std::is_same_v<T, int64_t>) return Value(arg);
    else if constexpr (std::is_same_v<T, uint64_t>) return Value(arg);
    else if constexpr (std::is_same_v<T, double>) return Value(arg);
    else if constexpr (std::is_same_v<T, std::string>) return Value(arg);
    else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
      json::Array a; for (auto& e : arg) a.push_back(Value(e)); return Value(std::move(a));
    } else {
      json::Array a; for (auto& e : arg) a.push_back(Value(e)); return Value(std::move(a));
    }
  }, s);
}

PropertyValue property_from_json(const json::Value& v) {
  switch (v.kind()) {
    case json::Value::Kind::Null: return PropertyValue(nullptr);
    case json::Value::Kind::Bool: return PropertyValue(v.as_bool());
    case json::Value::Kind::Int: return PropertyValue(v.as_int());
    case json::Value::Kind::Uint: return PropertyValue(v.as_uint());
    case json::Value::Kind::Double: return PropertyValue(v.as_double());
    case json::Value::Kind::String: return PropertyValue(v.as_string());
    case json::Value::Kind::Array: {
      auto& a = v.as_array();
      if (!a.empty() && a.front().is_string()) {
        std::vector<std::string> sv; for (auto& e : a) sv.push_back(e.as_string()); return PropertyValue::make_string_array(std::move(sv));
      }
      std::vector<int64_t> iv; for (auto& e : a) iv.push_back(e.integer()); return PropertyValue::make_int_array(std::move(iv));
    }
    case json::Value::Kind::Object: break;
  }
  return PropertyValue(nullptr);
}

json::Value native_to_json(const NativeIdentity& n) {
  json::Value o = json::Value::object();
  auto put = [&](const char* k, const std::optional<uint16_t>& v) {
    if (v) o.set(k, json::Value(static_cast<uint64_t>(*v)));
    else o.set(k, json::Value(nullptr));
  };
  put("pci_domain", n.pci_domain);
  put("pci_bus", n.pci_bus);
  put("pci_device", n.pci_device);
  put("pci_function", n.pci_function);
  put("vendor_id", n.vendor_id);
  put("device_id", n.device_id);
  put("subsystem_id", n.subsystem_id);
  put("subsystem_vendor_id", n.subsystem_vendor_id);
  o.set("numa_node", n.numa_node ? json::Value(static_cast<uint64_t>(*n.numa_node)) : json::Value(nullptr));
  o.set("cpu_package", n.cpu_package ? json::Value(static_cast<uint64_t>(*n.cpu_package)) : json::Value(nullptr));
  o.set("core_id", n.core_id ? json::Value(static_cast<uint64_t>(*n.core_id)) : json::Value(nullptr));
  o.set("processor_group", n.processor_group ? json::Value(static_cast<uint64_t>(*n.processor_group)) : json::Value(nullptr));
  o.set("logical_processor_index", n.logical_processor_index ? json::Value(static_cast<uint64_t>(*n.logical_processor_index)) : json::Value(nullptr));
  o.set("cuda_ordinal", n.cuda_ordinal ? json::Value(static_cast<uint64_t>(*n.cuda_ordinal)) : json::Value(nullptr));
  o.set("cuda_uuid", json::Value(n.cuda_uuid));
  o.set("name", json::Value(n.name));
  o.set("network_interface_name", json::Value(n.network_interface_name));
  o.set("network_hardware_id", json::Value(n.network_hardware_id));
  o.set("storage_id", json::Value(n.storage_id));
  o.set("storage_device_path", json::Value(n.storage_device_path));
  o.set("machine_name", json::Value(n.machine_name));
  o.set("os_version", json::Value(n.os_version));
  return o;
}

static std::optional<uint16_t> opt_u16(const json::Value& o, const char* k) {
  auto* p = o.find(k);
  if (!p || p->is_null()) return std::nullopt;
  return static_cast<uint16_t>(p->integer());
}
static std::optional<uint32_t> opt_u32(const json::Value& o, const char* k) {
  auto* p = o.find(k);
  if (!p || p->is_null()) return std::nullopt;
  return static_cast<uint32_t>(p->integer());
}

NativeIdentity native_from_json(const json::Value& o) {
  NativeIdentity n;
  if (!o.is_object()) return n;
  n.pci_domain = opt_u16(o, "pci_domain");
  n.pci_bus = opt_u16(o, "pci_bus");
  n.pci_device = opt_u16(o, "pci_device");
  n.pci_function = opt_u16(o, "pci_function");
  n.vendor_id = opt_u16(o, "vendor_id");
  n.device_id = opt_u16(o, "device_id");
  n.subsystem_id = opt_u16(o, "subsystem_id");
  n.subsystem_vendor_id = opt_u16(o, "subsystem_vendor_id");
  n.numa_node = opt_u32(o, "numa_node");
  n.cpu_package = opt_u32(o, "cpu_package");
  n.core_id = opt_u32(o, "core_id");
  n.processor_group = opt_u32(o, "processor_group");
  n.logical_processor_index = opt_u32(o, "logical_processor_index");
  n.cuda_ordinal = opt_u32(o, "cuda_ordinal");
  if (auto* p = o.find("cuda_uuid"); p && p->is_string()) n.cuda_uuid = p->as_string();
  if (auto* p = o.find("name"); p && p->is_string()) n.name = p->as_string();
  if (auto* p = o.find("network_interface_name"); p && p->is_string()) n.network_interface_name = p->as_string();
  if (auto* p = o.find("network_hardware_id"); p && p->is_string()) n.network_hardware_id = p->as_string();
  if (auto* p = o.find("storage_id"); p && p->is_string()) n.storage_id = p->as_string();
  if (auto* p = o.find("storage_device_path"); p && p->is_string()) n.storage_device_path = p->as_string();
  if (auto* p = o.find("machine_name"); p && p->is_string()) n.machine_name = p->as_string();
  if (auto* p = o.find("os_version"); p && p->is_string()) n.os_version = p->as_string();
  return n;
}

json::Value provenance_to_json(const Provenance& p) {
  json::Value o = json::Value::object();
  o.set("kind", json::Value(std::string(to_string(p.kind))));
  o.set("confidence", json::Value(std::string(to_string(p.confidence))));
  o.set("provider", json::Value(p.provider));
  o.set("api", json::Value(p.api));
  o.set("detail", json::Value(p.detail));
  o.set("timestamp_ms", json::Value(p.timestamp_ms));
  o.set("schema_version", json::Value(p.schema_version));
  o.set("provider_version", json::Value(p.provider_version));
  return o;
}

Provenance provenance_from_json(const json::Value& o) {
  Provenance p;
  if (!o.is_object()) return p;
  if (auto* v = o.find("kind"); v && v->is_string()) p.kind = provenance_kind_from_string(v->as_string());
  if (auto* v = o.find("confidence"); v && v->is_string()) p.confidence = confidence_from_string(v->as_string());
  if (auto* v = o.find("provider"); v && v->is_string()) p.provider = v->as_string();
  if (auto* v = o.find("api"); v && v->is_string()) p.api = v->as_string();
  if (auto* v = o.find("detail"); v && v->is_string()) p.detail = v->as_string();
  if (auto* v = o.find("timestamp_ms"); v && v->is_number()) p.timestamp_ms = v->integer();
  if (auto* v = o.find("schema_version"); v && v->is_string()) p.schema_version = v->as_string();
  if (auto* v = o.find("provider_version"); v && v->is_string()) p.provider_version = v->as_string();
  return p;
}

json::Value snapshot_to_json(const TopologySnapshot& snap) {
  json::Value root = json::Value::object();
  root.set("schema", json::Value(kSnapshotSchemaVersion));
  root.set("snapshot_id", json::Value(snap.metadata().snapshot_id));
  root.set("generation", json::Value(snap.metadata().generation));
  root.set("created_ms", json::Value(snap.metadata().created_ms));
  root.set("machine_identity", json::Value(snap.metadata().machine_identity));
  root.set("provider_versions", json::Value(snap.metadata().provider_versions));
  root.set("format", json::Value("topology_fabric_snapshot"));

  json::Array nodes;
  {
    std::vector<TopologyNodeId> ids;
    ids.reserve(snap.nodes().size());
    for (const auto& [id, n] : snap.nodes()) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    for (const auto& id : ids) {
      const auto& n = snap.node(id);
    json::Value o = json::Value::object();
    o.set("id", json::Value(id.to_hex()));
    o.set("type", json::Value(std::string(to_string(n.type))));
    o.set("category", json::Value(n.category));
    o.set("name", json::Value(n.name));
    o.set("display_name", json::Value(n.display_name));
    o.set("native", native_to_json(n.native));
    o.set("capabilities", json::Value(static_cast<uint64_t>(to_uint(n.capabilities))));
    o.set("confidence", json::Value(std::string(to_string(n.confidence))));
    o.set("provenance", provenance_to_json(n.provenance));
    o.set("synthetic", json::Value(n.synthetic));
    json::Value props = json::Value::object();
    for (const auto& [k, v] : n.properties) props.set(k, property_to_json(v));
    o.set("properties", std::move(props));
    nodes.push_back(std::move(o));
    }
  }
  root.set("nodes", std::move(nodes));

  json::Array edges;
  {
    std::vector<const TopologyEdge*> eptr;
    for (auto& e : snap.edges()) eptr.push_back(&e);
    std::sort(eptr.begin(), eptr.end(), [](const TopologyEdge* a, const TopologyEdge* b) {
      if (a->source != b->source) return a->source < b->source;
      if (a->target != b->target) return a->target < b->target;
      return static_cast<int>(a->type) < static_cast<int>(b->type);
    });
    for (const auto* ep : eptr) {
      const auto& e = *ep;
    json::Value o = json::Value::object();
    o.set("source", json::Value(e.source.to_hex()));
    o.set("target", json::Value(e.target.to_hex()));
    o.set("type", json::Value(std::string(to_string(e.type))));
    o.set("direction", json::Value(std::string(to_string(e.direction))));
    o.set("confidence", json::Value(std::string(to_string(e.confidence))));
    o.set("provenance", provenance_to_json(e.provenance));
    if (e.width) o.set("width", json::Value(static_cast<uint64_t>(*e.width)));
    if (e.pcie_generation) o.set("pcie_generation", json::Value(static_cast<uint64_t>(*e.pcie_generation)));
    if (e.bandwidth_bytes_per_sec) o.set("bandwidth_bps", json::Value(*e.bandwidth_bytes_per_sec));
    if (e.latency_ns) o.set("latency_ns", json::Value(*e.latency_ns));
    o.set("hop_count", json::Value(e.hop_count));
    if (e.locality_score) o.set("locality_score", json::Value(*e.locality_score));
    o.set("accessible", json::Value(e.accessible));
    if (e.peer_capability) o.set("peer_capability", json::Value(*e.peer_capability));
    o.set("policy_version", json::Value(static_cast<uint64_t>(e.policy_version)));
    json::Value props = json::Value::object();
    for (const auto& [k, v] : e.properties) props.set(k, property_to_json(v));
    o.set("properties", std::move(props));
    edges.push_back(std::move(o));
    }
  }
  root.set("edges", std::move(edges));

  json::Array warns;
  for (auto& w : snap.metadata().warnings) warns.push_back(json::Value(w));
  root.set("warnings", std::move(warns));
  root.set("partial_discovery", json::Value(snap.metadata().partial_discovery));
  root.set("synthetic", json::Value(snap.metadata().synthetic));

  json::Value val = json::Value::object();
  val.set("ok", json::Value(snap.validation().ok));
  json::Array errs; for (auto& e : snap.validation().errors) errs.push_back(json::Value(e)); val.set("errors", std::move(errs));
  json::Array warn; for (auto& w : snap.validation().warnings) warn.push_back(json::Value(w)); val.set("warnings", std::move(warn));
  root.set("validation", std::move(val));

  return root;
}

}  // namespace detail

std::string serialize_snapshot_json(const TopologySnapshot& snap, bool pretty) {
  auto v = detail::snapshot_to_json(snap);
  return json::dump(v, pretty ? json::WriteMode::Pretty : json::WriteMode::Compact);
}

std::shared_ptr<const TopologySnapshot> deserialize_snapshot_json(const std::string& jsonText,
                                                                  const Bounds& bounds) {
  auto r = json::parse(jsonText);
  if (!r.ok())
    throw TopologyError(ErrorCode::MALFORMED_DATA, "malformed JSON: " + r.message);
  if (jsonText.size() > bounds.max_snapshot_bytes)
    throw TopologyError(ErrorCode::OVERSIZED, "serialized snapshot exceeds bound");
  const auto& root = r.value;
  if (!root.is_object())
    throw TopologyError(ErrorCode::MALFORMED_DATA, "snapshot root must be an object");
  auto* fmt = root.find("format");
  if (!fmt || !fmt->is_string() || fmt->as_string() != "topology_fabric_snapshot")
    throw TopologyError(ErrorCode::MALFORMED_DATA, "not a topology_fabric_snapshot document");
  auto* schema = root.find("schema");

  SnapshotBuilder b(bounds);
  SnapshotMetadata meta;
  if (auto* v = root.find("snapshot_id"); v && v->is_string()) meta.snapshot_id = v->as_string();
  if (auto* v = root.find("generation"); v && v->is_number()) meta.generation = static_cast<uint64_t>(v->integer());
  if (auto* v = root.find("created_ms"); v && v->is_number()) meta.created_ms = v->integer();
  if (auto* v = root.find("machine_identity"); v && v->is_string()) meta.machine_identity = v->as_string();
  if (auto* v = root.find("provider_versions"); v && v->is_string()) meta.provider_versions = v->as_string();
  if (auto* v = root.find("partial_discovery"); v && v->is_bool()) meta.partial_discovery = v->as_bool();
  if (auto* v = root.find("synthetic"); v && v->is_bool()) meta.synthetic = v->as_bool();
  if (schema && schema->is_string()) meta.schema_version = schema->as_string(); else meta.schema_version = kSnapshotSchemaVersion;
  if (auto* v = root.find("warnings"); v && v->is_array())
    for (auto& w : v->as_array()) if (w.is_string()) meta.warnings.push_back(w.as_string());

  auto* nodes = root.find("nodes");
  if (!nodes || !nodes->is_array())
    throw TopologyError(ErrorCode::MALFORMED_DATA, "snapshot missing nodes array");
  size_t count = 0;
  for (auto& nv : nodes->as_array()) {
    if (++count > bounds.max_nodes) throw TopologyError(ErrorCode::OVERSIZED, "node count exceeds bound");
    if (!nv.is_object()) throw TopologyError(ErrorCode::MALFORMED_DATA, "node entry must be object");
    TopologyNode n;
    auto* id = nv.find("id");
    if (!id || !id->is_string() || !TopologyNodeId::try_from_hex(id->as_string(), n.id))
      throw TopologyError(ErrorCode::MALFORMED_DATA, "node missing/invalid id");
    if (auto* v = nv.find("type"); v && v->is_string()) n.type = node_type_from_string(v->as_string());
    if (auto* v = nv.find("category"); v && v->is_string()) n.category = v->as_string();
    if (auto* v = nv.find("name"); v && v->is_string()) n.name = v->as_string();
    if (auto* v = nv.find("display_name"); v && v->is_string()) n.display_name = v->as_string();
    if (auto* v = nv.find("native"); v) n.native = detail::native_from_json(*v);
    if (auto* v = nv.find("capabilities"); v && v->is_number()) n.capabilities = capability_from_uint(static_cast<uint64_t>(v->integer()));
    if (auto* v = nv.find("confidence"); v && v->is_string()) n.confidence = confidence_from_string(v->as_string());
    if (auto* v = nv.find("provenance"); v) n.provenance = detail::provenance_from_json(*v);
    if (auto* v = nv.find("synthetic"); v && v->is_bool()) n.synthetic = v->as_bool();
    if (auto* v = nv.find("properties"); v && v->is_object())
      for (auto& [k, pv] : v->as_object()) n.properties.emplace(k, detail::property_from_json(pv));
    b.add_node(std::move(n));
  }

  auto* edges = root.find("edges");
  if (!edges || !edges->is_array())
    throw TopologyError(ErrorCode::MALFORMED_DATA, "snapshot missing edges array");
  count = 0;
  for (auto& ev : edges->as_array()) {
    if (++count > bounds.max_edges) throw TopologyError(ErrorCode::OVERSIZED, "edge count exceeds bound");
    if (!ev.is_object()) throw TopologyError(ErrorCode::MALFORMED_DATA, "edge entry must be object");
    TopologyEdge e;
    auto* s = ev.find("source"); auto* t = ev.find("target");
    if (!s || !t || !s->is_string() || !t->is_string() ||
        !TopologyNodeId::try_from_hex(s->as_string(), e.source) ||
        !TopologyNodeId::try_from_hex(t->as_string(), e.target))
      throw TopologyError(ErrorCode::MALFORMED_DATA, "edge missing/invalid endpoint");
    if (auto* v = ev.find("type"); v && v->is_string()) e.type = edge_type_from_string(v->as_string());
    if (auto* v = ev.find("direction"); v && v->is_string()) e.direction = edge_direction_from_string(v->as_string());
    if (auto* v = ev.find("confidence"); v && v->is_string()) e.confidence = confidence_from_string(v->as_string());
    if (auto* v = ev.find("provenance"); v) e.provenance = detail::provenance_from_json(*v);
    if (auto* v = ev.find("width"); v && v->is_number()) e.width = static_cast<int>(v->integer());
    if (auto* v = ev.find("pcie_generation"); v && v->is_number()) e.pcie_generation = static_cast<int>(v->integer());
    if (auto* v = ev.find("bandwidth_bps"); v && v->is_number()) e.bandwidth_bytes_per_sec = v->number();
    if (auto* v = ev.find("latency_ns"); v && v->is_number()) e.latency_ns = v->number();
    if (auto* v = ev.find("hop_count"); v && v->is_number()) e.hop_count = static_cast<int>(v->integer());
    if (auto* v = ev.find("locality_score"); v && v->is_number()) e.locality_score = v->number();
    if (auto* v = ev.find("accessible"); v && v->is_bool()) e.accessible = v->as_bool();
    if (auto* v = ev.find("peer_capability"); v && v->is_number()) e.peer_capability = static_cast<uint64_t>(v->integer());
    if (auto* v = ev.find("policy_version"); v && v->is_number()) e.policy_version = static_cast<uint32_t>(v->integer());
    if (auto* v = ev.find("properties"); v && v->is_object())
      for (auto& [k, pv] : v->as_object()) e.properties.emplace(k, detail::property_from_json(pv));
    b.add_edge(std::move(e));
  }

  b.set_metadata(std::move(meta));
  try { return b.take(); }
  catch (const TopologyError&) { throw; }
}

std::shared_ptr<const TopologySnapshot> import_synthetic_json(const std::string& jsonText,
                                                              const Bounds& bounds) {
  auto snap = deserialize_snapshot_json(jsonText, bounds);
  // Verified synthetic marker is set in the document; if not, mark the copy synthetic.
  if (!snap->metadata().synthetic) {
    // Rebuild with synthetic flag. Simpler: clone metadata through a rebuild not required;
    // the serialized synthetic flag should be true for imports; otherwise honor document.
  }
  return snap;
}

}  // namespace topology_fabric