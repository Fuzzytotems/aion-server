# Server process tests of the game server application (chunk P5-14: OTHER_FILES in chunks.cmake), included by CMakeLists.txt inside
# if(AION_BUILD_TESTS) with its variables (GS_TESTS, GS_JAVA_DIR). Moved out of CMakeLists.txt in the wave 5a pre-stage (m5a-plan.md I-01), so the
# app lane can change the registrations of its tests (F-02: expected lines, TIMEOUT, gs.smoke.startup_progress) without the integrator.

# Startup smoke tests (cmake/RunStartupSmoke.cmake): aion_game_server with the M5a profile and a stop file against its own schema, the geo data
# switched off. Like the other database tests they are skipped unless the environment names the database: AION_TEST_GS_DATABASE_URL, e.g.
# jdbc:mysql://127.0.0.1:3306/aion_cpp_test?characterEncoding=UTF-8 (the database must exist; user root without password unless
# AION_TEST_GS_DATABASE_USER / AION_TEST_GS_DATABASE_PASSWORD are set).
# - gs.smoke.startup: the whole startup, the stop file and an orderly shutdown with exit code 0, and (m5a-plan.md F-01b) a startup that reached
#   "Game server started" without a single AION_UNPORTED hit. GS_SMOKE_REQUIRE_STARTED was OFF in stage 1 of wave 5a (a startup that passed the
#   M4 path and then stopped at an unported function was reported as skipped); stage 2 (F-01b) turns it ON, so the startup path is now a gate.
# - gs.smoke.startup_progress (m5a-plan.md F-01a): the "startup step N: name" log lines are complete and the run ends in order, whether the startup
#   reached "Game server started" or stopped at a step whose owner has not merged yet; it prints the last step reached.
set(GS_SMOKE_REQUIRE_STARTED ON)
foreach(gs_smoke_mode IN ITEMS smoke progress)
	if(gs_smoke_mode STREQUAL "smoke")
		set(gs_smoke_name gs.smoke.startup)
	else()
		set(gs_smoke_name gs.smoke.startup_progress)
	endif()
	add_test(NAME ${gs_smoke_name}
		COMMAND "${CMAKE_COMMAND}" "-DEXECUTABLE=$<TARGET_FILE:aion_game_server>" "-DDATABASE_TOOL=$<TARGET_FILE:aion_gs_m4_database>"
			"-DWORKING_DIRECTORY=${GS_JAVA_DIR}" "-DMODE=${gs_smoke_mode}" "-DREQUIRE_STARTED=${GS_SMOKE_REQUIRE_STARTED}"
			"-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/${gs_smoke_name}/$<CONFIG>" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunStartupSmoke.cmake")
	# Each run has its own log directory and schema now (RunStartupSmoke.cmake), so a run of another build directory no longer breaks this one.
	# The lock stays: two game servers loading the whole static data at once make both runs much slower and can exhaust the machine's memory.
	set_tests_properties(${gs_smoke_name} PROPERTIES LABELS "smoke;realdata" TIMEOUT 600
		SKIP_REGULAR_EXPRESSION "${gs_smoke_name}: skipped" RESOURCE_LOCK aion_game_server_log)
endforeach()

# M4 gate (handlers-and-porting-plan.md §2.7, cmake/RunM4Check.cmake): aion_game_server --check-static-data against a fresh aion_gs_test_m4_<hash>
# schema (IDFactory from the empty schema, then from fixture rows), then tools/oracle compare-counts and the geo/world comparison. Skipped
# without AION_TEST_GS_DATABASE_URL like gs.smoke.startup, and without Python (needed by the oracles).
add_executable(aion_gs_m4_database "${GS_TESTS}/m4/M4Database.cpp")
target_include_directories(aion_gs_m4_database PRIVATE "${GS_TESTS}/dao")
target_link_libraries(aion_gs_m4_database PRIVATE aion_commons_database aion_gs_build_options aion::compiler_options)
set_target_properties(aion_gs_m4_database PROPERTIES FOLDER "tests" CXX_SCAN_FOR_MODULES OFF)
# registered without Python too: RunM4Check.cmake then reports "skipped (no Python 3 ...)" instead of the test silently missing
if(Python3_Interpreter_FOUND)
	set(gs_m4_python "${Python3_EXECUTABLE}")
else()
	set(gs_m4_python NOTFOUND)
endif()
add_test(NAME gs.m4.check_static_data
	COMMAND "${CMAKE_COMMAND}" "-DEXECUTABLE=$<TARGET_FILE:aion_game_server>" "-DDATABASE_TOOL=$<TARGET_FILE:aion_gs_m4_database>"
		"-DWORKING_DIRECTORY=${GS_JAVA_DIR}" "-DPYTHON=${gs_m4_python}" "-DORACLE_DIR=${CMAKE_SOURCE_DIR}/tools/oracle"
		"-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/m4/$<CONFIG>" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunM4Check.cmake")
set_tests_properties(gs.m4.check_static_data PROPERTIES LABELS "m4;oracle;realdata" TIMEOUT 1500
	SKIP_REGULAR_EXPRESSION "gs\\.m4\\.check_static_data: skipped" RESOURCE_LOCK aion_game_server_log)
