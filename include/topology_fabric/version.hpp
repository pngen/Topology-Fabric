
// TopologyFabric/version.hpp - Topology Fabric version identification.
#pragma once
#ifndef TOPOLOGY_FABRIC_VERSION_MAJOR
#define TOPOLOGY_FABRIC_VERSION_MAJOR 1
#endif
#ifndef TOPOLOGY_FABRIC_VERSION_MINOR
#define TOPOLOGY_FABRIC_VERSION_MINOR 0
#endif
#ifndef TOPOLOGY_FABRIC_VERSION_PATCH
#define TOPOLOGY_FABRIC_VERSION_PATCH 0
#endif
#define TOPOLOGY_FABRIC_VERSION_STRING "1.0.0"
#define TOPOLOGY_FABRIC_VERSION "1.0.0"
namespace topology_fabric {
inline constexpr int kVersionMajor = 1;
inline constexpr int kVersionMinor = 0;
inline constexpr int kVersionPatch = 0;
inline constexpr const char* kVersionString = "1.0.0";
inline constexpr const char* kProductName = "Topology Fabric";
}