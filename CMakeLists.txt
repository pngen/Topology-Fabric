
cmake_minimum_required(VERSION 3.24)

# Topology Fabric 1.0.0 - vendor-neutral hardware/interconnect topology runtime.
project(TopologyFabric
  VERSION 1.0.0
  DESCRIPTION "Discover, model, measure, validate, score, and expose hardware and interconnect topology beneath heterogeneous AI infrastructure."
  HOMEPAGE_URL "https://github.com/summon-software-labs/topology-fabric"
  LANGUAGES CXX)

# Options
option(TOPOLOGY_FABRIC_BUILD_TESTS        "Build Topology Fabric tests"        ON)
option(TOPOLOGY_FABRIC_BUILD_EXAMPLES     "Build Topology Fabric examples"     ON)
option(TOPOLOGY_FABRIC_BUILD_BENCHMARKS   "Build Topology Fabric benchmarks"   ON)
option(TOPOLOGY_FABRIC_ENABLE_CUDA        "Enable the CUDA provider"           ON)
option(TOPOLOGY_FABRIC_WARNINGS_AS_ERRORS "Treat compiler warnings as errors"  ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Shared C++ settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows
if(WIN32)
  add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
endif()

# Warning policy.
set(TOPOLOGY_FABRIC_WARNINGS "")
if(MSVC)
  list(APPEND TOPOLOGY_FABRIC_WARNINGS /W4)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  list(APPEND TOPOLOGY_FABRIC_WARNINGS -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

if(TOPOLOGY_FABRIC_WARNINGS_AS_ERRORS)
  if(MSVC)
    list(APPEND TOPOLOGY_FABRIC_WARNINGS /WX)
  else()
    list(APPEND TOPOLOGY_FABRIC_WARNINGS -Werror)
  endif()
endif()

# Warning suppression list (MSVC) for known-benign system/CUDA-header codes.
list(APPEND TOPOLOGY_FABRIC_MSVC_DISABLE
  /wd4100
  /wd4127
  /wd4324
  /wd4201
  /wd4458
  /wd4456
  /wd4505
  /wd4251
  /wd4239
  /wd4702
  /wd4706
  /wd4389
  /wd4018
  /wd4244
  /wd4267
  /wd4365
)

# Topology Fabric core library
add_library(topology_fabric
  src/core/node.cpp
  src/core/types.cpp
  src/core/node_id.cpp
  src/core/provenance.cpp
  src/core/edge.cpp
  src/core/snapshot.cpp
  src/core/graph.cpp
  src/core/value.cpp
  src/core/identity.cpp
  src/core/serialization.cpp
  src/core/merge.cpp
  src/core/validation.cpp
  src/core/diff.cpp
  src/core/path.cpp
  src/core/distance.cpp
  src/core/cost.cpp
  src/core/locality.cpp
  src/core/ranking.cpp
  src/core/telemetry.cpp
  src/core/registry.cpp
  src/core/runtime.cpp
  src/providers/host.cpp
  src/providers/cpu_numa.cpp
  src/providers/pci.cpp
  src/providers/storage.cpp
  src/providers/network.cpp
  src/providers/cuda.cpp
)

add_library(TopologyFabric::topology_fabric ALIAS topology_fabric)

target_include_directories(topology_fabric
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_options(topology_fabric PRIVATE ${TOPOLOGY_FABRIC_WARNINGS})
target_compile_options(topology_fabric PRIVATE ${TOPOLOGY_FABRIC_MSVC_DISABLE})

if(TOPOLOGY_FABRIC_ENABLE_CUDA)
  target_compile_definitions(topology_fabric PRIVATE TOPOLOGY_FABRIC_HAS_CUDA)
endif()

find_package(Threads REQUIRED)
target_link_libraries(topology_fabric PUBLIC Threads::Threads)

if(WIN32)
  target_link_libraries(topology_fabric PRIVATE cfgmgr32 iphlpapi ws2_32)
endif()

set_target_properties(topology_fabric PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
  POSITION_INDEPENDENT_CODE ON
)

# CUDA provider (optional). Uses runtime dynamic loading of the NVIDIA
# CUDA driver API (nvcuda.dll) so no build-time CUDA toolchain is required.
# TOPOLOGY_FABRIC_ENABLE_CUDA=OFF excludes it entirely.
if(TOPOLOGY_FABRIC_ENABLE_CUDA)
  set(TOPOLOGY_FABRIC_CUDA_PROVIDER_SRC src/providers/cuda.cpp)
else()
  set(TOPOLOGY_FABRIC_CUDA_PROVIDER_SRC "")
endif()
# The provider is part of the core library; see src/providers/cuda.cpp.


# CLI executable
add_executable(topology-fabric
  src/cli/main.cpp
)
target_link_libraries(topology-fabric PRIVATE topology_fabric)
target_compile_options(topology-fabric PRIVATE ${TOPOLOGY_FABRIC_WARNINGS})
target_compile_options(topology-fabric PRIVATE ${TOPOLOGY_FABRIC_MSVC_DISABLE})

# Tests / examples / benchmarks
if(TOPOLOGY_FABRIC_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
if(TOPOLOGY_FABRIC_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
if(TOPOLOGY_FABRIC_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()

# Install + package config
install(TARGETS topology_fabric
  EXPORT TopologyFabricTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(TARGETS topology-fabric
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})


install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

set(CONFIG_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/TopologyFabric)
configure_package_config_file(
  cmake/TopologyFabricConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/TopologyFabricConfig.cmake"
  INSTALL_DESTINATION ${CONFIG_INSTALL_DIR})
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/TopologyFabricConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/TopologyFabricConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/TopologyFabricConfigVersion.cmake"
  DESTINATION ${CONFIG_INSTALL_DIR})
install(EXPORT TopologyFabricTargets
  FILE TopologyFabricTargets.cmake
  NAMESPACE TopologyFabric::
  DESTINATION ${CONFIG_INSTALL_DIR})

# Compile definitions for consumers
target_compile_definitions(topology_fabric INTERFACE
  TOPOLOGY_FABRIC_VERSION_MAJOR=@PROJECT_VERSION_MAJOR@
  TOPOLOGY_FABRIC_VERSION_MINOR=@PROJECT_VERSION_MINOR@
  TOPOLOGY_FABRIC_VERSION_PATCH=@PROJECT_VERSION_PATCH@)