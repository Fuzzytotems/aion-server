# Server process tests of the game server application (chunk P5-14: OTHER_FILES in chunks.cmake), included by CMakeLists.txt inside
# if(AION_BUILD_TESTS) with its variables (GS_TESTS, GS_JAVA_DIR, AION_GS_ALLOW_MILESTONE_SKIP). Moved out of CMakeLists.txt in the wave 5a
# pre-stage (m5a-plan.md I-01), so the app lane can change the registrations of its tests (F-02: expected lines, TIMEOUT,
# gs.smoke.startup_progress) without the integrator.

# Startup smoke tests (cmake/RunStartupSmoke.cmake): aion_game_server with the M5a profile and a stop file against its own schema and its own log
# directory. They need the database in the environment: AION_TEST_GS_DATABASE_URL, e.g.
# jdbc:mysql://127.0.0.1:3306/aion_cpp_test?characterEncoding=UTF-8 (the database must exist; user root without password unless
# AION_TEST_GS_DATABASE_USER / AION_TEST_GS_DATABASE_PASSWORD are set). Without it they FAIL instead of skipping themselves - see
# AION_GS_ALLOW_MILESTONE_SKIP in CMakeLists.txt.
# - gs.smoke.startup: the whole startup, the stop file and an orderly shutdown with exit code 0, and (m5a-plan.md F-01b) a startup that reached
#   "Game server started" without a single AION_UNPORTED hit. GS_SMOKE_REQUIRE_STARTED was OFF in stage 1 of wave 5a (a startup that passed the
#   M4 path and then stopped at an unported function was reported as skipped); stage 2 (F-01b) turns it ON, so the startup path is now a gate.
# - gs.smoke.startup_progress (m5a-plan.md F-01a): the "startup step N: name" log lines are complete and the run ends in order, whether the startup
#   reached "Game server started" or stopped at a step whose owner has not merged yet; it prints the last step reached.
# - gs.smoke.startup_geo (stage 3 of wave 5a): the same startup with the geo data ENABLED. Zone handlers run only with geo on, so before this test
#   no automated run had ever executed one - and the user's first real client lost 13 npc spawns to an unported SiegeShield::onEnterZone
#   (docs/design/m5a-client-session.md F-1), exactly as m5a-plan.md §5.1 "Geodata" predicted. It requires "Game server started", the geo data and
#   the npc spawns really loaded, 0 spawn failures, 0 AION_UNPORTED hits and no ERROR in the run's own log folder except the one the harness
#   causes itself (the login link points at a closed port, see RunStartupSmoke.cmake "MODE geo").
#   It is slow and large: the reference run of the integrator's machine takes 144 s and about 5 GB in a Debug build and loads 83,872 npc spawns,
#   so it gets TIMEOUT 2400 (the script's own process timeout is 2100 s) and the label geo next to smoke;realdata, to select it (ctest -L geo) or
#   leave it out (ctest -LE geo) on its own.
set(GS_SMOKE_REQUIRE_STARTED ON)
foreach(gs_smoke_mode IN ITEMS smoke progress geo)
	if(gs_smoke_mode STREQUAL "smoke")
		set(gs_smoke_name gs.smoke.startup)
		set(gs_smoke_labels "smoke;realdata")
		set(gs_smoke_timeout 600)
	elseif(gs_smoke_mode STREQUAL "progress")
		set(gs_smoke_name gs.smoke.startup_progress)
		set(gs_smoke_labels "smoke;realdata")
		set(gs_smoke_timeout 600)
	else()
		set(gs_smoke_name gs.smoke.startup_geo)
		set(gs_smoke_labels "smoke;realdata;geo")
		set(gs_smoke_timeout 2400)
	endif()
	add_test(NAME ${gs_smoke_name}
		COMMAND "${CMAKE_COMMAND}" "-DEXECUTABLE=$<TARGET_FILE:aion_game_server>" "-DDATABASE_TOOL=$<TARGET_FILE:aion_gs_m4_database>"
			"-DWORKING_DIRECTORY=${GS_JAVA_DIR}" "-DMODE=${gs_smoke_mode}" "-DREQUIRE_STARTED=${GS_SMOKE_REQUIRE_STARTED}"
			"-DALLOW_SKIP=${AION_GS_ALLOW_MILESTONE_SKIP}"
			"-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/${gs_smoke_name}/$<CONFIG>" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunStartupSmoke.cmake")
	# Each run has its own log directory and schema now (RunStartupSmoke.cmake), so a run of another build directory no longer breaks this one.
	# The lock stays: two game servers loading the whole static data at once make both runs much slower and can exhaust the machine's memory
	# (the geo run alone needs about 5 GB).
	set_tests_properties(${gs_smoke_name} PROPERTIES LABELS "${gs_smoke_labels}" TIMEOUT ${gs_smoke_timeout}
		SKIP_REGULAR_EXPRESSION "${gs_smoke_name}: skipped" RESOURCE_LOCK aion_game_server_log)
endforeach()

# M4 gate (handlers-and-porting-plan.md §2.7, cmake/RunM4Check.cmake): aion_game_server --check-static-data against a fresh aion_gs_test_m4_<hash>
# schema (IDFactory from the empty schema, then from fixture rows), then tools/oracle compare-counts and the geo/world comparison. Like
# gs.smoke.startup it needs AION_TEST_GS_DATABASE_URL, and additionally a Python 3 interpreter for the oracles; without either it fails instead of
# skipping itself (AION_GS_ALLOW_MILESTONE_SKIP opts out).
add_executable(aion_gs_m4_database "${GS_TESTS}/m4/M4Database.cpp")
target_include_directories(aion_gs_m4_database PRIVATE "${GS_TESTS}/dao")
target_link_libraries(aion_gs_m4_database PRIVATE aion_commons_database aion_gs_build_options aion::compiler_options)
set_target_properties(aion_gs_m4_database PROPERTIES FOLDER "tests" CXX_SCAN_FOR_MODULES OFF)
# registered without Python too: RunM4Check.cmake then names the missing interpreter instead of the test silently missing
if(Python3_Interpreter_FOUND)
	set(gs_m4_python "${Python3_EXECUTABLE}")
else()
	set(gs_m4_python NOTFOUND)
endif()
add_test(NAME gs.m4.check_static_data
	COMMAND "${CMAKE_COMMAND}" "-DEXECUTABLE=$<TARGET_FILE:aion_game_server>" "-DDATABASE_TOOL=$<TARGET_FILE:aion_gs_m4_database>"
		"-DWORKING_DIRECTORY=${GS_JAVA_DIR}" "-DPYTHON=${gs_m4_python}" "-DORACLE_DIR=${CMAKE_SOURCE_DIR}/tools/oracle"
		"-DALLOW_SKIP=${AION_GS_ALLOW_MILESTONE_SKIP}"
		"-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/m4/$<CONFIG>" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunM4Check.cmake")
set_tests_properties(gs.m4.check_static_data PROPERTIES LABELS "m4;oracle;realdata" TIMEOUT 1500
	SKIP_REGULAR_EXPRESSION "gs\\.m4\\.check_static_data: skipped" RESOURCE_LOCK aion_game_server_log)
