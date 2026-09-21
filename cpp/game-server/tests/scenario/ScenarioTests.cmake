# CTest registrations of the M5a scenario harness and of the M5a gate itself (chunk P5-SC, owned through its test directory tests/scenario;
# m5a-plan.md §5.10, F-06). Included by game-server/CMakeLists.txt inside if(AION_BUILD_TESTS), after the wiring of aion_gs_scenario_tests
# (login-server/tests/support include directory, aion_loginserver_crypto, the dependencies on aion_game_server and aion_login_server, and the
# compile definitions AION_GAME_SERVER_EXECUTABLE, AION_LOGIN_SERVER_EXECUTABLE, AION_LOGINSERVER_JAVA_DIR, AION_GAMESERVER_JAVA_DIR,
# AION_SCENARIO_OUTPUT_DIR).
#
# The GoogleTest cases of aion_gs_scenario_tests are discovered like every chunk test. M5aScenario.Run is the gate: it owns one pair of server
# processes, so it must not run twice, and the discovered case is therefore marked DISABLED while gs.scenario.m5a runs the same binary with a
# filter (LABELS "scenario;realdata", TIMEOUT 900, RESOURCE_LOCK "aion_game_server_log;aion_login_server_log",
# SKIP_REGULAR_EXPRESSION "gs\\.scenario\\.m5a: skipped"). gs.scenario.m5a_stress (G-01, stage 3): LABELS "scenario;stress;nightly".

if(TARGET aion_gs_scenario_tests)
	# the harness self-tests (F-04): the stub game server is StubGameServer.cmake run by this CMake
	target_compile_definitions(aion_gs_scenario_tests PRIVATE AION_SCENARIO_CMAKE_COMMAND="${CMAKE_COMMAND}"
		AION_SCENARIO_STUB_GAME_SERVER="${CMAKE_CURRENT_SOURCE_DIR}/tests/scenario/StubGameServer.cmake")

	# the gate's own inputs: tools/oracle/oracle.py (F-05, run with AION_TEST_PYTHON) and the AION_PARTIAL allow-list of D3
	target_compile_definitions(aion_gs_scenario_tests PRIVATE AION_SCENARIO_ORACLE_SCRIPT="${CMAKE_SOURCE_DIR}/tools/oracle/oracle.py"
		AION_SCENARIO_PARTIAL_ALLOWLIST="${CMAKE_CURRENT_SOURCE_DIR}/tests/scenario/m5a_partial_allowlist.txt")

	# the oracle answers are JSON (Oracle.cpp); commons finds the same package in its own directory scope
	find_package(nlohmann_json CONFIG REQUIRED)
	target_link_libraries(aion_gs_scenario_tests PRIVATE nlohmann_json::nlohmann_json)

	# cases that read the Java module directories
	aion_set_discovered_test_properties(aion_gs_scenario_tests REGEX "^(ScenarioDatabaseTest|ScenarioServersTest|ScenarioWiringTest)\\."
		PROPERTIES LABELS "scenario;realdata")
	# the real login server writes login-server/log (like the login server's own server tests)
	aion_set_discovered_test_properties(aion_gs_scenario_tests REGEX "^LoginServerHarnessTest\\."
		PROPERTIES LABELS "scenario;realdata" RESOURCE_LOCK "aion_login_server_log" TIMEOUT 300)
	# the gate runs as gs.scenario.m5a below, never as a discovered case
	aion_set_discovered_test_properties(aion_gs_scenario_tests REGEX "^M5aScenario\\." PROPERTIES DISABLED TRUE
		LABELS "scenario;realdata")

	# F-06: the M5a gate (§5.10). The oracle needs a Python interpreter; without it the test prints "gs.scenario.m5a: skipped".
	set(scenario_work_dir "${CMAKE_CURRENT_BINARY_DIR}/test_work/aion_gs_scenario_tests")
	file(MAKE_DIRECTORY "${scenario_work_dir}")
	add_test(NAME gs.scenario.m5a COMMAND "$<TARGET_FILE:aion_gs_scenario_tests>" --gtest_filter=M5aScenario.Run
		WORKING_DIRECTORY "${scenario_work_dir}")
	set_tests_properties(gs.scenario.m5a PROPERTIES LABELS "scenario;realdata" TIMEOUT 900
		RESOURCE_LOCK "aion_game_server_log;aion_login_server_log" SKIP_REGULAR_EXPRESSION "gs\\.scenario\\.m5a: skipped")
	if(Python3_Interpreter_FOUND)
		set_property(TEST gs.scenario.m5a APPEND PROPERTY ENVIRONMENT_MODIFICATION "AION_TEST_PYTHON=set:${Python3_EXECUTABLE}")
	endif()
	# A SKIPPED GATE IS NOT A PASSED GATE. The database URLs come from the environment at test time, not from this configure, so without them
	# the test skips itself, SKIP_REGULAR_EXPRESSION turns that into CTest's "***Skipped" and CTest counts it as passed - a full ctest without
	# AION_TEST_GS_DATABASE_URL / AION_TEST_LS_DATABASE_URL then reports 100% green with M5a never executed. Configure the milestone and CI
	# trees with -DAION_SCENARIO_REQUIRE=ON: the gate then FAILS instead of skipping when its inputs are missing (M5aScenarioTest.cpp).
	if(AION_SCENARIO_REQUIRE)
		set_property(TEST gs.scenario.m5a APPEND PROPERTY ENVIRONMENT_MODIFICATION "AION_SCENARIO_REQUIRE=set:1")
	endif()
endif()
