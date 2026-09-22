# CTest registrations of the M5a scenario harness and of the M5a gate itself (chunk P5-SC, owned through its test directory tests/scenario;
# m5a-plan.md §5.10, F-06). Included by game-server/CMakeLists.txt inside if(AION_BUILD_TESTS), after the wiring of aion_gs_scenario_tests
# (login-server/tests/support include directory, aion_loginserver_crypto, the dependencies on aion_game_server and aion_login_server, and the
# compile definitions AION_GAME_SERVER_EXECUTABLE, AION_LOGIN_SERVER_EXECUTABLE, AION_LOGINSERVER_JAVA_DIR, AION_GAMESERVER_JAVA_DIR,
# AION_SCENARIO_OUTPUT_DIR).
#
# The GoogleTest cases of aion_gs_scenario_tests are discovered like every chunk test. M5aScenario.Run and M5aScenarioGeo.Run are the gates:
# each owns one pair of server processes, so neither must run twice, and the discovered cases are therefore marked DISABLED while
# gs.scenario.m5a and gs.scenario.m5a_geo run the same binary with a filter (LABELS "scenario;realdata", TIMEOUT 900 / 2700, RESOURCE_LOCK
# "aion_game_server_log;aion_login_server_log", SKIP_REGULAR_EXPRESSION "gs\\.scenario\\.m5a: skipped" / "...m5a_geo: skipped").
# gs.scenario.m5a_stress (G-01, stage 3): LABELS "scenario;stress;nightly".

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
	# starts the real login server; since stage 3 it writes its own log directory (ScenarioServers::loginLogFolder), and the lock is kept only
	# so that two real login servers never start at the same time as the gate's
	aion_set_discovered_test_properties(aion_gs_scenario_tests REGEX "^LoginServerHarnessTest\\."
		PROPERTIES LABELS "scenario;realdata" RESOURCE_LOCK "aion_login_server_log" TIMEOUT 300)
	# the gates run as gs.scenario.m5a and gs.scenario.m5a_geo below, never as discovered cases
	aion_set_discovered_test_properties(aion_gs_scenario_tests REGEX "^M5aScenario(Geo)?\\." PROPERTIES DISABLED TRUE
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
	# A SKIPPED GATE IS NOT A PASSED GATE. The database URLs come from the environment at test time, not from this configure, so without them the
	# test would skip itself, SKIP_REGULAR_EXPRESSION would turn that into CTest's "***Skipped" and CTest counts a skip as passed - a full ctest
	# without AION_TEST_GS_DATABASE_URL / AION_TEST_LS_DATABASE_URL then reported 100% green with M5a never executed.
	# Since stage 3 the requirement is the DEFAULT (m5a-plan.md §5.10 asked for it as an option; the first real client session showed what an
	# unrun gate is worth): AION_SCENARIO_REQUIRE=1 is passed to the gate unless the user opts out with -DAION_GS_ALLOW_MILESTONE_SKIP=ON, and
	# M5aScenarioTest.cpp then turns every skip reason - missing URLs, missing Python interpreter - into a failure that names the variable.
	# Unlike the cmake -P driven milestone tests (RunStartupSmoke.cmake, RunM4Check.cmake), this one cannot honour the environment variable
	# AION_GS_ALLOW_MILESTONE_SKIP: ENVIRONMENT_MODIFICATION is fixed at configure time, and the gate is a GoogleTest binary, not a script. The
	# cache option is the opt-out here. -DAION_SCENARIO_REQUIRE=ON still forces the requirement, even with the opt-out on.
	if(AION_SCENARIO_REQUIRE OR NOT AION_GS_ALLOW_MILESTONE_SKIP)
		set_property(TEST gs.scenario.m5a APPEND PROPERTY ENVIRONMENT_MODIFICATION "AION_SCENARIO_REQUIRE=set:1")
	endif()

	# gs.scenario.m5a_geo (stage 3 wave B, m5a-plan.md §5.1 "Geodata" and §11): the SAME binary and the same scripted path, with
	# -Dgameserver.geodata.enable=true. It is a second CTest and not a flag on the one above, because the two configurations have different
	# costs - loading the 151 .geo files takes a checked RelWithDebInfo startup from 4 s to 6-9 s and a Debug one to 144 s and 3.2 GB
	# (m5a-client-session.md) - and because a tree that cannot afford the geo run must still be able to run the milestone gate:
	# `ctest -R 'gs\.scenario\.m5a$'`.
	#
	# It carries the SAME RESOURCE_LOCK as gs.scenario.m5a, which is what keeps the two from ever running at the same time: two game servers
	# with the geo data loaded on a machine somebody is working on is the one configuration the resource lock exists to prevent.
	add_test(NAME gs.scenario.m5a_geo COMMAND "$<TARGET_FILE:aion_gs_scenario_tests>" --gtest_filter=M5aScenarioGeo.Run
		WORKING_DIRECTORY "${scenario_work_dir}")
	# TIMEOUT: the measured run is 18-21 s in this tree, but a Debug build starts in 144 s (m5a-client-session.md) and the timeout has to hold for
	# a loaded machine as well; RunStartupSmoke.cmake gives its own geo mode 2100 s of server time for the same reason.
	set_tests_properties(gs.scenario.m5a_geo PROPERTIES LABELS "scenario;realdata;geo" TIMEOUT 2700
		RESOURCE_LOCK "aion_game_server_log;aion_login_server_log" SKIP_REGULAR_EXPRESSION "gs\\.scenario\\.m5a_geo: skipped")
	if(Python3_Interpreter_FOUND)
		set_property(TEST gs.scenario.m5a_geo APPEND PROPERTY ENVIRONMENT_MODIFICATION "AION_TEST_PYTHON=set:${Python3_EXECUTABLE}")
	endif()
	if(AION_SCENARIO_REQUIRE OR NOT AION_GS_ALLOW_MILESTONE_SKIP)
		set_property(TEST gs.scenario.m5a_geo APPEND PROPERTY ENVIRONMENT_MODIFICATION "AION_SCENARIO_REQUIRE=set:1")
	endif()
endif()
