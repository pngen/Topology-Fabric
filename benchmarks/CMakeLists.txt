
set(TF_BENCH_DEPS TopologyFabric::topology_fabric)
if(TARGET topology_fabric_cuda)
  list(APPEND TF_BENCH_DEPS topology_fabric_cuda)
endif()

add_executable(tf_bench_graph bench_graph.cpp)
target_link_libraries(tf_bench_graph PRIVATE ${TF_BENCH_DEPS})
target_compile_options(tf_bench_graph PRIVATE ${TOPOLOGY_FABRIC_WARNINGS})
target_compile_options(tf_bench_graph PRIVATE ${TOPOLOGY_FABRIC_MSVC_DISABLE})

add_executable(tf_bench_discovery bench_discovery.cpp)
target_link_libraries(tf_bench_discovery PRIVATE ${TF_BENCH_DEPS})
target_compile_options(tf_bench_discovery PRIVATE ${TOPOLOGY_FABRIC_WARNINGS})
target_compile_options(tf_bench_discovery PRIVATE ${TOPOLOGY_FABRIC_MSVC_DISABLE})
