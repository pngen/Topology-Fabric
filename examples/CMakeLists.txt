
set(TF_EX_DEPS TopologyFabric::topology_fabric)
if(TARGET topology_fabric_cuda)
  list(APPEND TF_EX_DEPS topology_fabric_cuda)
endif()

# 14 runnable examples.
foreach(ex ex_discover ex_cpus ex_numa ex_pci ex_cuda ex_accel_locality ex_path
        ex_rank ex_snapshot ex_diff ex_multigpu ex_merge ex_validate ex_explain)
  add_executable(${ex} ${ex}.cpp)
  target_link_libraries(${ex} PRIVATE ${TF_EX_DEPS})
  target_compile_options(${ex} PRIVATE ${TOPOLOGY_FABRIC_WARNINGS})
  target_compile_options(${ex} PRIVATE ${TOPOLOGY_FABRIC_MSVC_DISABLE})
endforeach()
