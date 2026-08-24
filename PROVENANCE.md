# Provenance

Every nontrivial topology fact carries provenance: where it came from, how it was established, and
with what certainty. This document describes the Provenance struct and the kinds, as implemented
in provenance.hpp and provenance.cpp.

## The Provenance struct

Provenance (provenance.hpp) has these fields:

| Field | Type | Meaning |
|-------|------|---------|
| kind | ProvenanceKind | how the fact was established |
| confidence | Confidence | certainty of the fact |
| provider | string | e.g. "windows.pci", "cuda", "windows.cpu_numa" |
| api | string | e.g. "GetLogicalProcessorInformationEx", "cuDeviceGetAttribute" |
| detail | string | free-form note |
| timestamp_ms | int64_t | epoch milliseconds (0 = unset) |
| schema_version | string | "1.0" by default |
| provider_version | string | e.g. "10.0", "12.9" |

## The kinds

ProvenanceKind (types.hpp):

| Value | Enumerator | Meaning |
|-------|-----------|---------|
| 0 | DISCOVERED | directly observed via an API/platform call |
| 1 | INFERRED | derived from other facts / reasoning |
| 2 | MEASURED | produced by a measurement |
| 3 | USER_SUPPLIED | supplied by a caller/config |
| 4 | UNKNOWN | not classified |

## Factory helpers

Provenance provides static constructors:

- Provenance::discovered(provider, api, confidence, provider_version, detail) — sets
  kind = DISCOVERED.
- Provenance::inferred(provider, api, confidence, detail) — sets kind = INFERRED.
- Provenance::measured(provider, api, confidence, provider_version, detail) — sets
  kind = MEASURED.
- Provenance::user_supplied(confidence, detail) — sets kind = USER_SUPPLIED.
- Provenance::unknown() — returns an empty Provenance{} (kind UNKNOWN, confidence UNKNOWN).

## Examples from the providers

- Machine node: DISCOVERED, provider "windows.host", api
  "GetComputerNameEx/RtlGetVersion/GetActiveProcessorCount/GlobalMemoryStatusEx",
  confidence HIGH, provider_version "10.0".
- CPU/NUMA nodes: DISCOVERED, provider "windows.cpu_numa", api
  "GetLogicalProcessorInformationEx", confidence AUTHORITATIVE, provider_version "10.0".
- Host memory domains: INFERRED, provider "windows.cpu_numa", api "NUMA_NODE_RELATIONSHIP",
  confidence MEDIUM.
- PCI nodes: DISCOVERED, provider "windows.pci", api
  "CM_Get_Device_ID_List/CM_Get_Parent", confidence HIGH.
- CUDA device: DISCOVERED, provider "cuda", api "cuDeviceGetCount/cuDeviceGetAttribute",
  confidence AUTHORITATIVE.
- CUDA VRAM domain: INFERRED, provider "cuda", api "cuDeviceGetAttribute", confidence MEDIUM.
- Machine-attach edges applied by the runtime: INFERRED, provider "merge", api
  "machine attach", confidence MEDIUM.

## Never-overwrite rule at merge

The merge (PROVIDERS.md) treats provenance as the group's representative. It pre-sorts members
best-first by confidence and selects the best-confidence member's provenance for the merged node.
It does not merge provenance strings from lower-confidence members; it keeps the highest-confidence
source. Property conflicts between members are recorded in MergedGraph.conflicts instead of being
silently resolved. Combined with the confidence rule (only minimum on merge, see CONFIDENCE.md),
this means:

- A dominant, authoritative source remains authoritative.
- A less authoritative fact never overwrites a more authoritative one.
- Any disagreement is surfaced, not hidden.

## Provenance vs confidence

These are deliberately independent axes. A fact can be DISCOVERED (observed via an API) but only
MEDIUM confidence (e.g. a PCI bridge role inferred from parent/child structure). It can be
INFERRED yet no less certain than a discovered fact. Provenance answers "how did we come to know
this?"; confidence answers "how sure are we?". See CONFIDENCE.md.

## Serialization

Provenance is serialized losslessly. provenance_to_json emits kind, confidence, provider, api,
detail, timestamp_ms, schema_version, provider_version. provenance_from_json reads them back,
using the string<->enum converters in types.hpp (unknown values map to a default and are never
fatal).
