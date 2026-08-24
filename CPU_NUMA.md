# CPU and NUMA Discovery

This document describes how Topology Fabric discovers CPU and NUMA topology on Windows. It is
implemented in src/providers/cpu_numa.cpp and src/providers/host.cpp.

## Host facts (host provider)

The host provider creates the MACHINE node. It uses:

- GetComputerNameExA(ComputerNameDnsHostname) for the machine name, falling back to
  GetComputerNameA.
- RtlGetVersion (loaded from ntdll.dll) for an accurate OS version string, e.g.
  "windows 10.0.26100" (major.minor.build). If RtlGetVersion is unavailable the os string stays
  "windows".
- GetActiveProcessorGroupCount and GetActiveProcessorCount(ALL_PROCESSOR_GROUPS) for the
  group count and total logical processor count.
- GlobalMemoryStatusEx for total / available physical memory and the current memory load %.

The machine node is contributed with ref "machine:<name>", provenance
"windows.host" / getters at Confidence::HIGH, and capabilities CPU_ADDRESSABLE |
NUMA_LOCAL | SHARED_MEMORY. Properties recorded:

- os.version, machine.name
- processor.logical_count, processor.group_count
- memory.total_bytes, memory.available_bytes, memory.usage_percent (when memory stats are
  available)

## CPU / NUMA facts (cpu_numa provider)

The cpu_numa provider calls GetLogicalProcessorInformationEx(RelationAll) once to get the full
buffer, then walks the returned records, dispatching on p->Relationship:

- RelationProcessorCore: pushes the core's GROUP_AFFINITY masks into a cores list.
- RelationProcessorPackage: pushes the package's group masks into packages.
- RelationNumaNode: pushes the node's group masks and its NodeNumber into numa and numaNumbers.

If the API returns no bytes or fails, the provider records a warning (winutil::last_error) and
marks the contribution partial.

### Node construction

From these records the provider builds:

- One NUMA_NODE per RelationNumaNode, ref "numa:<NodeNumber>", named "NUMA <n>", with
  native.numa_node set, capabilities CPU_ADDRESSABLE | NUMA_LOCAL.
- One HOST_MEMORY_DOMAIN per NUMA node, ref "memdom:<NodeNumber>", with capabilities
  CPU_ADDRESSABLE | NUMA_LOCAL | SHARED_MEMORY. Its provenance is inferred
  ("windows.cpu_numa", "NUMA_NODE_RELATIONSHIP", Confidence::MEDIUM) since the host memory domain
  is a derived fact.
- One CPU_PACKAGE per package, ref "pkg:<index>", native.cpu_package set, capabilities
  CPU_ADDRESSABLE | NUMA_LOCAL.
- One CPU_CORE per core, ref "core:<index>", native.core_id set; the core's cpu_package is
  resolved by matching its group masks against the packages list.
- One CPU_THREAD per logical processor bit in each core's mask. The thread ref is
  "thread:g<group>:<bit>"; native.logical_processor_index is the running counter,
  native.processor_group is the group, native.core_id is the core index, native.cpu_package and
  native.numa_node are resolved by matching the single-bit mask against packages and numa.

### Containment edges

- machine contains each NUMA node ("machine:<name>" -> "numa:<n>"), CONTAINS, HIGH.
- each NUMA node contains its host memory domain, CONTAINS, HIGH.
- machine contains each CPU package, CONTAINS, HIGH.
- the package (or machine, when no package is resolved) contains each core, CONTAINS, HIGH.
- each core contains each of its threads, CONTAINS, HIGH.
- each thread that maps to a NUMA node gains a LOCAL_TO edge to "numa:<n>", UNDIRECTED, MEDIUM.

The CONTAINS edges use provenance "windows.cpu_numa"/"GetLogicalProcessorInformationEx" at
Confidence::HIGH; the node provenance is Confidence::AUTHORITATIVE (for package/core/thread)
because the relationship is directly observed.

## Group affinity representation

The provider uses GROUP_AFFINITY to map a core/package/nuuma to its container. A core's group
mask is matched bit-by-bit against the package and NUMA masks via package_of / numa_of, which
scan the packages and numa lists for a group + mask that contains the given group + bit. This
lets the provider attribute each logical processor to a package and a NUMA node on systems with
multiple processor groups.

## Physical vs logical

The provider records logical_processor_index per thread, core_id per core, cpu_package per
package, processor_group per thread, and numa_node where known. It does not try to infer cache
hierarchy or SMT siblings beyond the core/thread split that the CPU relationship already encodes.
The "CPU <n>" naming uses a monotonic counter, which is display only; identity is derived from the
canonical native key (see GRAPH_MODEL.md).

## Non-Windows behavior

On non-Windows builds both host and cpu_numa return a Contribution with
warnings.push_back("... only implemented on Windows"), partial=true, success=false. No platform
facts are fabricated.
