# Game server benchmarks (design §19 P4: movement / known-list microbenchmark). Prototype code, not the real game model.
# Owned by the bench stage. Run manually (RelWithDebInfo = checked, Release = unchecked); not registered with ctest.

file(GLOB_RECURSE bench_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
# .inl files are included by the model sources; listed as headers so they appear in the IDE project
file(GLOB_RECURSE bench_inlines CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.inl")
set_source_files_properties(${bench_inlines} PROPERTIES HEADER_FILE_ONLY ON)
add_executable(aion_gs_bench ${bench_sources} ${bench_inlines})
target_link_libraries(aion_gs_bench PRIVATE aion::gs_runtime aion::compiler_options)
target_include_directories(aion_gs_bench PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
aion_copy_asan_runtime(aion_gs_bench)
