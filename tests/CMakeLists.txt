
set(TF_TEST_DEPS TopologyFabric::topology_fabric)
if(TARGET topology_fabric_cuda)
  list(APPEND TF_TEST_DEPS topology_fabric_cuda)
endif()

add_executable(tf_tests
  test_main.cpp
  test_smoke.cpp
  test_graph.cpp
  test_identity.cpp
  test_merge.cpp
  test_path.cpp
  test_ranking.cpp
  test_cost.cpp
  test_serialization.cpp
  test_snapshot.cpp
  test_diff.cpp
  test_concurrency.cpp
  test_property.cpp
  test_adversarial.cpp
  test_import.cpp
  test_validation.cpp
)
target_link_libraries(tf_tests PRIVATE ${TF_TEST_DEPS})
target_compile_options(tf_tests PRIVATE ${TOPOLOGY_FABRIC_WARNINGS})
target_compile_options(tf_tests PRIVATE ${TOPOLOGY_FABRIC_MSVC_DISABLE})
add_test(NAME tf_tests COMMAND tf_tests)
