
// TopologyFabric/export.hpp - symbol export macros for the Topology Fabric runtime.
#pragma once
#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef TOPOLOGY_FABRIC_EXPORTS
    #define TOPOLOGY_FABRIC_API __declspec(dllexport)
  #else
    #define TOPOLOGY_FABRIC_API __declspec(dllimport)
  #endif
#elif defined(__GNUC__) || defined(__clang__)
  #define TOPOLOGY_FABRIC_API __attribute__((visibility("default")))
#else
  #define TOPOLOGY_FABRIC_API
#endif

// The core library is built as a static library; mark exports as no-op builds.
#if defined(TOPOLOGY_FABRIC_STATIC) || !defined(TOPOLOGY_FABRIC_EXPORTS)
  #ifdef TOPOLOGY_FABRIC_API
    #undef TOPOLOGY_FABRIC_API
  #endif
  #define TOPOLOGY_FABRIC_API
#endif

#define TOPOLOGY_FABRIC_NAMESPACE topology_fabric
#define TOPOLOGY_FABRIC_FILE(name) <topology_fabric/name.hpp>
