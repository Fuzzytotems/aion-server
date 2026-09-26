# CTest registration of the M5a stress nightly (m5a-plan.md G-01, §5.10 last row, §11; chunk P5-SC, owned through the test directory
# tests/scenario). Included by game-server/CMakeLists.txt inside if(AION_BUILD_TESTS), next to tests/scenario/ScenarioTests.cmake and after the
# wiring of aion_gs_scenario_tests, with:
#
#     if(EXISTS "${GS_TESTS}/scenario/StressTests.cmake")
#         include("${GS_TESTS}/scenario/StressTests.cmake")
#     endif()
#
# The sources are tests/scenario/stress/*, which the chunk manifest already owns (aion_add_tests globs tests/scenario recursively), so nothing
# else has to change.

if(TARGET aion_gs_scenario_tests)
	# the pure helpers (options, log patterns, names) run like any other unit test: no database, no server, milliseconds
	aion_set_discovered_test_properties(aion_gs_scenario_tests REGEX "^StressSupportTest\\." PROPERTIES LABELS "scenario")
	# the run itself is gs.scenario.m5a_stress below and must never run as a discovered case as well: it owns a pair of server processes
	aion_set_discovered_test_properties(aion_gs_scenario_tests REGEX "^M5aStress\\." PROPERTIES DISABLED TRUE
		LABELS "scenario;realdata;stress;nightly")

	# A THIRTY MINUTE TEST MUST NOT START BY ACCIDENT. The label "stress" is how the run is SELECTED (`ctest -L stress`), but a label does not
	# keep a test out of a plain `ctest` - CTest runs every registered test unless -L/-LE says otherwise - and this one starts a game server, a
	# login server and twenty clients for half an hour on whatever machine the suite happens to run on. It is therefore registered DISABLED
	# unless the tree was configured for it:
	#
	#     cmake -S . -B build/nightly -DAION_STRESS_NIGHTLY=ON
	#     ctest -C RelWithDebInfo -L stress
	#
	# A default tree still LISTS it (ctest -N -L stress shows it as disabled), so nobody has to guess that it exists; it just does not run.
	option(AION_STRESS_NIGHTLY "Enable gs.scenario.m5a_stress, the 30 minute 20 client stress run of m5a-plan.md G-01" OFF)

	set(stress_work_dir "${CMAKE_CURRENT_BINARY_DIR}/test_work/aion_gs_scenario_tests")
	file(MAKE_DIRECTORY "${stress_work_dir}")
	add_test(NAME gs.scenario.m5a_stress COMMAND "$<TARGET_FILE:aion_gs_scenario_tests>" --gtest_filter=M5aStress.Run
		WORKING_DIRECTORY "${stress_work_dir}")
	# TIMEOUT: 30 minutes of clients, up to 10 minutes of startup (a loaded machine, and the game server loads the static data first), up to 5
	# minutes for the shutdown with its final census, and slack - a client that is waiting out its packet timeout when the duration ends adds one
	# more minute. The run's own duration comes from the environment below, so raising it means raising this too.
	set_tests_properties(gs.scenario.m5a_stress PROPERTIES LABELS "scenario;realdata;stress;nightly" TIMEOUT 3600
		# BOTH gate slots (ScenarioTests.cmake, "the two gate slots"): nothing else starts a server beside the twenty clients, and the run's
		# schema prefix m5a is the m5a gates' (slot 1)
		RESOURCE_LOCK "${AION_GS_GATE_SLOT_1};${AION_GS_GATE_SLOT_2}"
		SKIP_REGULAR_EXPRESSION "gs\\.scenario\\.m5a_stress: skipped")
	if(NOT AION_STRESS_NIGHTLY)
		set_tests_properties(gs.scenario.m5a_stress PROPERTIES DISABLED TRUE)
	endif()
	# G-01's numbers. The test's own defaults are small (2 clients, 45 s) so that running the binary by hand is cheap; the registration is what
	# asks for the real thirty minutes with twenty clients.
	set_property(TEST gs.scenario.m5a_stress APPEND PROPERTY ENVIRONMENT_MODIFICATION
		"AION_STRESS_CLIENTS=set:20" "AION_STRESS_MINUTES=set:30")
	# the no-silent-skip policy wave A established (ScenarioTests.cmake): without the database URLs the run FAILS and names what to set, unless
	# the tree was configured with -DAION_GS_ALLOW_MILESTONE_SKIP=ON. A skipped nightly is not a passed nightly.
	if(AION_SCENARIO_REQUIRE OR NOT AION_GS_ALLOW_MILESTONE_SKIP)
		set_property(TEST gs.scenario.m5a_stress APPEND PROPERTY ENVIRONMENT_MODIFICATION "AION_SCENARIO_REQUIRE=set:1")
	endif()
endif()
