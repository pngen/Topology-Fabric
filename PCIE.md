# PCIe Hierarchy Discovery

This document describes how Topology Fabric discovers the PCIe hierarchy on Windows, implemented
in src/providers/pci.cpp using the Configuration Manager (cfgmgr32) and DEVPKEY device properties.

## Device enumeration

The provider enumerates present PCI devices:

1. CM_Get_Device_ID_List_SizeW(nullptr, CM_GETIDLIST_FILTER_PRESENT) gives the buffer size for
   the present-device ID list.
2. CM_Get_Device_ID_ListW(nullptr, ids, size, CM_GETIDLIST_FILTER_PRESENT) fills the list.
3. The provider iterates the double-null-terminated list, keeping only device instance IDs that
   begin with "PCI\".
4. For each such ID it calls CM_Locate_DevNodeW (CM_LOCATE_DEVNODE_NORMAL) to get the DEVINST,
   parses vendor/device/subsystem ids (VEN_/DEV_/SUBSYS_ hex in the instance ID), and calls
   CM_Get_Parent to find the parent DEVINST. The parent's instance ID (via CM_Get_Device_IDW)
   is recorded as parentOf[id].

If either list call fails, the provider records a warning (winutil::last_error) and marks the
contribution partial.

## Classification into root / bridge / endpoint

For each PCI device the provider decides its role:

- PCI_ROOT: a PCI entity that itself has PCI children but whose parent is not a PCI device
  (isParentNode && !hasPciParent).
- PCI_BRIDGE: a PCI entity that is under a PCI device AND has PCI children.
- PCI_DEVICE: a leaf PCI function with no PCI children.

Each node is contributed as ref "pci:<instance-id>", named "PCI <instance-id>", with
native.vendor_id / device_id / subsystem_id set where parsed, native.pci_domain = 0, and
capabilities PCI_CONNECTED | DMA_CAPABLE. Provenance is "windows.pci" /
"CM_Get_Device_ID_List/CM_Get_Parent" at Confidence::HIGH.

## BDF and LocationInfo

The provider attempts to read the device's location to populate the BDF (bus/device/function).

- get_location(dev) calls CM_Get_DevNode_PropertyW(dev, &DEVPKEY_Device_LocationInfo, ...),
  then a second call to fetch the string, returning std::nullopt on failure.
- parse_location parses the "bus", "device", and "function" integers out of the LocationInfo /
  LocationPaths string.
- If all three parse, the provider sets native.pci_bus / pci_device / pci_function; otherwise the
  BDF fields remain unset (std::nullopt).

Important honesty note: on the validated host, DEVPKEY_Device_LocationInfo and LocationPaths
returned CR_NOT_FOUND, so the BDF was not surfaced for the GPU. NativeIdentity::pci_bdf_string()
only reports a BDF when all three components are known and otherwise returns "unknown", and
canonical_key() falls through to the next-best identity. The result is that the CUDA GPU node and
its OS PCI node may remain separate nodes rather than being unified by BDF. This is a partial
discovery outcome, not a bug: see LIMITATIONS.md.

## Edges

For each PCI device the provider adds a CONTAINS edge:

- if the parent is also a PCI device in the list, parent -> child ("pci:<parent>" ->
  "pci:<child>");
- otherwise, machine -> device ("machine:<name>" -> "pci:<device>").

The machine name is obtained via GetComputerNameExA(ComputerNameDnsHostname). The same
provenance/confidence as the nodes is used. The provider sets partial=true even on success because
BDF (and other physical attributes) may not be resolvable on all hosts.

## What is not yet discovered

The PCI provider does not read PCIe link width/generation from the Configuration Manager; it only
classifies the hierarchy and the vendor/device/subsystem IDs. Consequently the cost model relies
on conservative class-based defaults for link bandwidth/latency (see COST_MODEL.md); the link
facts ("width", "pcie_generation") on edges are left unset unless a later provider supplies them.
This is documented as a limitation: nominal link speed/width are default estimates, not measured.

## Non-Windows behavior

On non-Windows builds the provider returns partial=true, success=false with the warning
"pci provider only implemented on Windows".
