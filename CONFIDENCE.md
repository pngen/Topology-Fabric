# Confidence

Confidence expresses how certain a topology fact is. It is a separate axis from provenance: a fact
can be discovered (observed) yet only medium confidence, or inferred yet still reliable. This
document describes the five levels and the never-silently-upgrade rule, as implemented in
types.hpp and confidence.hpp.

## The five levels

Confidence (types.hpp):

| Value | Enumerator | Meaning | Typical example |
|-------|-----------|---------|-----------------|
| 0 | AUTHORITATIVE | the source of truth, directly observed | a read from the CUDA driver API, an authoritative Windows API result |
| 1 | HIGH | strongly supported | a parsed PCI hierarchy from the Configuration Manager |
| 2 | MEDIUM | moderately supported / derived | an inferred host memory domain, a machine-attach edge |
| 3 | LOW | weakly supported | a heuristic or partially verified value |
| 4 | UNKNOWN | not yet classified | a placeholder before attribution |

## Ranking order

ConfidenceRanking (confidence.hpp) fixes a total order:

- rank(AUTHORITATIVE) = 5
- rank(HIGH)           = 4
- rank(MEDIUM)         = 3
- rank(LOW)            = 2
- rank(UNKNOWN)        = 1

It also provides maximum(a, b) and minimum(a, b). The header comment states the invariant
directly: "Confidence is only ever decreased on merge, never silently upgraded."

## Never silently upgrade

The merge (see PROVIDERS.md) sets a merged node's confidence to ConfidenceRanking::maximum over its
members, and selects the highest-confidence member's provenance. This means:

- An AUTHORITATIVE fact is never replaced by a HIGH/MEDIUM/LOW/UNKNOWN one on merge.
- A conflict between members is recorded in MergedGraph.conflicts rather than silently resolved.
- A less authoritative source cannot overwrite a more authoritative one at the value level either;
  the merge keeps the first (highest-confidence) value for each property key and records a
  conflict if a later (lower-confidence) member disagrees.

## Where confidence is used

- Node level: TopologyNode.confidence and TopologyNode.provenance.confidence both carry it.
- Edge level: TopologyEdge.confidence and TopologyEdge.provenance.confidence.
- Path level: TopologyPath.confidence defaults to MEDIUM but is refined by cost_breakdown.
- Cost model: edges whose confidence is LOW or UNKNOWN add an uncertainty_penalty
  (see COST_MODEL.md); a path with low-confidence edges is flagged "uncertain_path" in ranking
  reasons.

## Concrete in-practice assignments

- CPU/NUMA package/core/thread nodes: AUTHORITATIVE (directly observed via
  GetLogicalProcessorInformationEx).
- Host memory domains: MEDIUM (inferred "one host memory domain per NUMA node").
- PCI hierarchy nodes: HIGH.
- Container edges: CONTAINS HIGH, LOCAL_TO MEDIUM.
- CUDA device: AUTHORITATIVE.
- CUDA peer edges: AUTHORITATIVE.
- Machine-attach edges added by the runtime: MEDIUM (inferred).
- Synthetic/imported snapshots: the confidence is preserved from the source document; the
  synthetic flag is what marks them as non-discovered.

## JSON

Confidence serializes as its to_string(Confidence) text (e.g. "authoritative", "high") and parses
back via confidence_from_string. Unknown/empty values map to UNKNOWN and are never fatal during
deserialization.
