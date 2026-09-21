# CTest registrations of the M5a scenario harness (chunk P5-SC, owned through its test directory tests/scenario; m5a-plan.md §5.10). Included
# by game-server/CMakeLists.txt inside if(AION_BUILD_TESTS), after the wiring of aion_gs_scenario_tests (login-server/tests/support include
# directory, aion_loginserver_crypto, the dependencies on aion_game_server and aion_login_server, and the compile definitions
# AION_GAME_SERVER_EXECUTABLE, AION_LOGIN_SERVER_EXECUTABLE, AION_LOGINSERVER_JAVA_DIR, AION_GAMESERVER_JAVA_DIR, AION_SCENARIO_OUTPUT_DIR).
#
# The GoogleTest cases of aion_gs_scenario_tests are discovered like every chunk test. The planned gate (F-06, stage 2):
#   gs.scenario.m5a: LABELS "scenario;realdata", TIMEOUT 900, RESOURCE_LOCK "aion_game_server_log;aion_login_server_log",
#   SKIP_REGULAR_EXPRESSION "gs\\.scenario\\.m5a: skipped"; gs.scenario.m5a_stress (G-01): LABELS "scenario;stress;nightly".

if(TARGET aion_gs_scenario_tests)
	# the harness self-tests (F-04): the stub game server is StubGameServer.cmake run by this CMake
	target_compile_definitions(aion_gs_scenario_tests PRIVATE AION_SCENARIO_CMAKE_COMMAND="${CMAKE_COMMAND}"
		AION_SCENARIO_STUB_GAME_SERVER="${CMAKE_CURRENT_SOURCE_DIR}/tests/scenario/StubGameServer.cmake")

	# cases that read the Java module directories
	aion_set_discovered_test_properties(aion_gs_scenario_tests REGEX "^(ScenarioDatabaseTest|ScenarioServersTest|ScenarioWiringTest)\\."
		PROPERTIES LABELS "scenario;realdata")
	# the real login server writes login-server/log (like the login server's own server tests)
	aion_set_discovered_test_properties(aion_gs_scenario_tests REGEX "^LoginServerHarnessTest\\."
		PROPERTIES LABELS "scenario;realdata" RESOURCE_LOCK "aion_login_server_log" TIMEOUT 300)
endif()
