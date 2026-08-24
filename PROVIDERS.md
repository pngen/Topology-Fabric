# Providers

Providers are the pluggable discovery backends. Each provider observes platform facts and
contributes them independently to a deterministic merge. This document describes the provider
interface, the registry, and the merge precedence rules as actually implemented.

## Provider interface

A provider is a subclass of TopologyProvider (provider.hpp):

- virtual std::string name() const — the registry key (e.g. "host", "cpu_numa", "pci",
  "cuda", "storage", "network").
- virtual std::string version() const — provider version.
- virtual Contribution discover(const DiscoveryContext& ctx) — run discovery and return a
  Contribution.
- bool available() const / mark_available(bool) — a provider may mark itself unavailable (e.g.
  the CUDA provider when the driver is absent); available() is exposed for callers.

### DiscoveryContext

Handed to discover():

- Bounds bounds — resource limits.
- vector<string> warnings — provider-collected warnings.
- int64_t started_ms — wall-clock epoch ms.
- bool allow_measurement — set false by default; measurement providers honor this.

### Contribution

See PROVIDERS.md's contribution table in DISCOVERY.md. Each provider returns nodes + edges
addressed by provider-local refs plus warnings, a partial flag, and a success flag.

### ContributedNode / ContributedEdge

A ContributedNode carries ref (provider-local stable ref, ideally a canonical key), type,
category, name, native, capabilities, properties, provenance, synthetic. A ContributedEdge is
addressed by from_ref / to_ref and carries type, direction, provenance, confidence, and optional
link facts.

## Provider registry

ProviderRegistry (registry.hpp) is a thread-safe, ordered map of providers keyed by name.

- register_provider(p) is last-writer-wins by name: a later registration with the same name
  replaces the earlier one. This is used to override a stub with a real backend. The first
  registration of a name fixes its position in the iteration order; re-registration keeps that
  position.
- unregister(name), find(name), all() (in registration order), size(), clear().

Because registration is last-writer-wins, the runtime can replace a default provider with a
platform-specific one without touching the merge logic.

## Merge of contributions

merge_contributions(contributions, bounds) (merge.cpp) combines all contributions into one
MergedGraph (nodes, edges, warnings, conflicts, merged_count). The process is:

1. Group every contributed node by its canonical merge key. The key is
   NativeIdentity::canonical_key() when non-empty; otherwise a fallback
   "type:<type>:<name>".
2. Within each group, pre-sort members best-first by confidence rank (ties broken by
   contribution index, then node index).
3. Resolve the group's type to the best type by a fixed type rank (ACCELERATOR_MEMORY_DOMAIN >
   ACCELERATOR > NETWORK_INTERFACE > STORAGE/SHARED_MEMORY > PCI_DEVICE > PCI_BRIDGE > PCI_ROOT >
   CPU_* > NUMA/HOST_MEMORY/MACHINE > UNKNOWN/EXTENSION).
4. Assign each merged node a TopologyNodeId = derive_node_id("touchstone","node", key).
5. Merge NativeIdentity fields first-non-null over best-first members.
6. OR all member capabilities.
7. Set node confidence to the maximum over members (never silently lowered).
8. Keep the best-confidence member's provenance.
9. Keep the first value per property key, recording a conflict string when a later member
   disagrees, and add tf.merged_providers / tf.merged_members synthetic properties.
10. Resolve edges: build a global ref -> node-id map (canonical group keys preferred, then refs),
    resolve each contributed edge's endpoints, and drop any edge with an unresolvable endpoint
    (recorded as a warning).
11. Deduplicate edges by canonical TopologyEdge::Key (source/target canonicalized for undirected
    edges).
12. Enforce max_nodes / max_edges directly on the merged output.

## Never-overwrite authoritative facts

The merge only ever picks the highest-confidence member as the group's representative and its
provenance. It does not lower an authoritative fact to a less authoritative one. When members
disagree on a property value, the conflict is recorded in MergedGraph.conflicts rather than
silently resolved. This is the core of the "never silently upgrade (or downgrade) authority" rule
(see PROVENANCE.md and CONFIDENCE.md).

## Built-in providers

register_builtin_providers() registers, in order: host, cpu_numa, pci, cuda (when
TOPOLOGY_FABRIC_HAS_CUDA is defined), storage, network. Only Windows backends are implemented; on
non-Windows builds each returns partial/success=false with a clear warning. See:
CPU_NUMA.md, PCIE.md, CUDA.md, and the storage/network sections of DISCOVERY.md.

## Adding a custom provider

To add a provider, define a TopologyProvider subclass, implement name/version/discover, and
register it via runtime.providers().register_provider(std::make_shared<MyProvider>()) before
calling discover(). Register it last to override a built-in, or first to contribute facts the
built-ins do not produce. The provider must be deterministic and must not mutate shared state; it
only returns a Contribution.

## Machine-attach pass

After the merge, the runtime adds an inferred CONTAINS edge from the machine node to any rootless
ACCELERATOR, STORAGE_DEVICE, NETWORK_INTERFACE, PCI_ROOT, or PCI_DEVICE node, at
Confidence::MEDIUM. This is a merge-level provenance ("merge", "machine attach") and is not
attributed to any single provider.
