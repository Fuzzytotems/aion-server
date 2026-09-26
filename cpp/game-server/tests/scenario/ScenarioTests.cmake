# CTest registrations of the M5a scenario harness and of the M5a gate itself (chunk P5-SC, owned through its test directory tests/scenario;
# m5a-plan.md §5.10, F-06). Included by game-server/CMakeLists.txt inside if(AION_BUILD_TESTS), after the wiring of aion_gs_scenario_tests
# (login-server/tests/support include directory, aion_loginserver_crypto, the dependencies on aion_game_server and aion_login_server, and the
# compile definitions AION_GAME_SERVER_EXECUTABLE, AION_LOGIN_SERVER_EXECUTABLE, AION_LOGINSERVER_JAVA_DIR, AION_GAMESERVER_JAVA_DIR,
# AION_SCENARIO_OUTPUT_DIR).
#
# The GoogleTest cases of aion_gs_scenario_tests are discovered like every chunk test. M5aScenario.Run and M5aScenarioGeo.Run are the gates:
# each owns one pair of server processes, so neither must run twice, and the discovered cases are therefore marked DISABLED while
# gs.scenario.m5a and gs.scenario.m5a_geo run the same binary with a filter (LABELS "scenario;realdata", TIMEOUT 900 / 2700, one of the two
# gate slots below as RESOURCE_LOCK, SKIP_REGULAR_EXPRESSION "gs\\.scenario\\.m5a: skipped" / "...m5a_geo: skipped").
# gs.scenario.m5a_stress (G-01, stage 3): LABELS "scenario;stress;nightly", both gate slots (StressTests.cmake).

# ---- the two gate slots -----------------------------------------------------------------------------------------------------------------------
#
# TWO SERVER RUNS AT A TIME, NEVER THREE. Every test that starts a real game server holds exactly one of these two CTest resource locks, so
# under any `ctest -j` at most two of them run at once. `ctest -j 2 -L "scenario|smoke|geo|m4"` took 1286 s and 1227 s on 2026-09-24 and 1220 s
# and 1145 s on 2026-09-25, 46 of 46 passing (the machine building other trees meanwhile), where the old single lock
# "aion_game_server_log;aion_login_server_log" ran the same tests one
# after the other in about 2100 s; each run is 10-30 % slower beside another, which the slot sums below do not include. That lock dates from
# before the harness gave each server run its own files; what two concurrent runs share today, and why it is safe:
#   - logs, reports, stop file: the game server gets --log-folder, --check-output and --stop-file below the run's own output directory, the
#     login server a working directory of its own with a copy of its config (ScenarioServers.h); RunStartupSmoke.cmake and RunM4Check.cmake
#     give theirs --log-folder=<OUTPUT_DIR>/... as well;
#   - the HTML cache, which HTMLCache writes at startup below the working directory whenever it is missing (./cache/html.cache,
#     HTMLCache.java:112-120): -Dgameserver.html.cache.file=<output directory>/html.cache in ScenarioServers and RunStartupSmoke.cmake
#     (the M4 check never creates HTMLCache);
#   - ports are ephemeral (ScenarioServers::reservePorts; the smoke tests bind 127.0.0.1:0 and point their login link at the closed port 1),
#     and NioServer binds with SO_EXCLUSIVEADDRUSE, so a collision would fail a run loudly instead of mixing two runs' clients;
#   - schemas: each run has its own pair, named after the hash of its output directory (the smoke and M4 schemas likewise), and holds an in-use
#     marker on it for the whole run;
#   - the working directory game-server/ itself: what a game server still writes there is ./log/stats/MethodStats.log at every orderly
#     shutdown (RunnableStatsManager::dumpClassStats, whose ./log/stats is hard-coded as in Java and ignores --log-folder) - two shutdowns
#     at the same moment interleave that diagnostic file, which no test reads - and, only on a watchdog stall, a minidump in ./log/dumps
#     whose name carries the process id. Neither can fail a run; routing the first needs a production change (reported, not made here);
#   - memory: a Debug game server with the geo data takes about 3.2 GB of private bytes, so two geo runs at once take about 6.4 GB beside
#     whatever else the machine builds (measured peak of the two servers: 6,352 MB);
#   - MariaDB: two game servers with at most 5 pool connections each, two login servers and the harness connections.
#
# ONE CONSTRAINT BEYOND THE BALANCE: a gate and its geo variant share a slot. createSchemas() sweeps the abandoned schemas of its own
# milestone prefix (aion_gs_test_m5b_...), and ScenarioDatabase::dropAbandonedSchemas checks the in-use marker before the DROP, not under
# it: two runs of one prefix starting at the same moment could drop the pair the other one was just recreating, if a killed earlier run had
# left it behind. One slot per prefix rules that out inside a ctest run; LoginServerHarnessTest, whose pair also has the prefix m5a, holds the
# m5a slot for the same reason. The smoke and M4 schemas are never swept.
#
# The slots are balanced by the runtimes of a Debug tree, each run alone (seconds, 2026-09-24; gs.smoke.startup_progress ~ gs.smoke.startup):
#   slot 1  gs.smoke.startup 34, gs.smoke.startup_progress 34, gs.smoke.startup_geo 150, gs.m4.check_static_data 149,
#           gs.scenario.m5a 65, gs.scenario.m5a_geo 170, gs.scenario.m5b3 146, gs.scenario.m5b3_geo 314            = 1062
#   slot 2  gs.scenario.m5b 227, gs.scenario.m5b_geo 351, gs.scenario.m5b2 172, gs.scenario.m5b2_geo 301           = 1051
# The balance holds for the full set above. `ctest -L scenario` alone leaves the smoke and M4 tests out, so slot 1 is then m5a, m5a_geo, m5b3 and
# m5b3_geo (about 695 s plus LoginServerHarnessTest) against slot 2's 1051 s: correct, just a longer wall clock for that label.
# The slots count inside ONE ctest process. Two build trees running their gates at the same time can reach four servers (about 12.8 GB with
# geo), and the marker-before-DROP window of dropAbandonedSchemas is open between them again: run one tree's gate set at a time.
# Slot 1 keeps the historical name aion_game_server_log because cmake/AppTests.cmake (chunk P5-14) registers the three smoke tests and the M4
# check with exactly that lock: they are in slot 1 without an edit there. A new gate joins the slot with the smaller sum, together with its
# geo variant, and adds its runtime above. The stress run (StressTests.cmake) holds both slots, so nothing else starts a server beside its
# twenty clients.
set(AION_GS_GATE_SLOT_1 "aion_game_server_log")
set(AION_GS_GATE_SLOT_2 "aion_game_server_slot_2")

# The safety net for the rule "exactly one slot per server run", run at the end of this directory, when cmake/AppTests.cmake, this file and
# StressTests.cmake have registered everything: a gs.scenario.*, gs.smoke.* or gs.m4.* test that holds NEITHER slot - a new gate registered
# without one - would start a third server beside two others, so it is given both (it then runs alone) and named in a warning.
function(aion_gs_check_gate_slots)
	get_directory_property(registered TESTS)
	foreach(test IN LISTS registered)
		if(NOT test MATCHES "^gs\\.(scenario|smoke|m4)\\.")
			continue()
		endif()
		get_test_property(${test} RESOURCE_LOCK locks)
		if(NOT locks)
			set(locks "")
		endif()
		list(FIND locks "${AION_GS_GATE_SLOT_1}" slot_1)
		list(FIND locks "${AION_GS_GATE_SLOT_2}" slot_2)
		if(slot_1 EQUAL -1 AND slot_2 EQUAL -1)
			message(WARNING "${test} starts a server but holds neither gate slot (RESOURCE_LOCK \"${locks}\"), so it would run beside two other "
				"server runs: it is given both and runs alone. Give it one slot, AION_GS_GATE_SLOT_1 or AION_GS_GATE_SLOT_2, as "
				"game-server/tests/scenario/ScenarioTests.cmake explains (\"the two gate slots\").")
			set_property(TEST ${test} APPEND PROPERTY RESOURCE_LOCK "${AION_GS_GATE_SLOT_1}" "${AION_GS_GATE_SLOT_2}")
		endif()
	endforeach()
endfunction()
cmake_language(DEFER CALL aion_gs_check_gate_slots)

if(TARGET aion_gs_scenario_tests)
	# the harness self-tests (F-04): the stub game server is StubGameServer.cmake run by this CMake
	target_compile_definitions(aion_gs_scenario_tests PRIVATE AION_SCENARIO_CMAKE_COMMAND="${CMAKE_COMMAND}"
		AION_SCENARIO_STUB_GAME_SERVER="${CMAKE_CURRENT_SOURCE_DIR}/tests/scenario/StubGameServer.cmake")

	# the gate's own inputs: tools/oracle/oracle.py (F-05, run with AION_TEST_PYTHON) and the AION_PARTIAL allow-list of D3. The M5b gate reads a
	# SECOND list (m5b-plan.md D9): M5b-1 legitimately adds partials - D3 the post-spawn skills, D4 the npc skill attack, D5 the drop
	# registration, D14 the critical proc - that the M5a scripted path must still not reach, so the two lists have to be able to disagree.
	target_compile_definitions(aion_gs_scenario_tests PRIVATE AION_SCENARIO_ORACLE_SCRIPT="${CMAKE_SOURCE_DIR}/tools/oracle/oracle.py"
		AION_SCENARIO_PARTIAL_ALLOWLIST="${CMAKE_CURRENT_SOURCE_DIR}/tests/scenario/m5a_partial_allowlist.txt"
		AION_SCENARIO_M5B_PARTIAL_ALLOWLIST="${CMAKE_CURRENT_SOURCE_DIR}/tests/scenario/m5b_partial_allowlist.txt"
		AION_SCENARIO_M5B2_PARTIAL_ALLOWLIST="${CMAKE_CURRENT_SOURCE_DIR}/tests/scenario/m5b2_partial_allowlist.txt"
		AION_SCENARIO_M5B3_PARTIAL_ALLOWLIST="${CMAKE_CURRENT_SOURCE_DIR}/tests/scenario/m5b3_partial_allowlist.txt")

	# the oracle answers are JSON (Oracle.cpp); commons finds the same package in its own directory scope
	find_package(nlohmann_json CONFIG REQUIRED)
	target_link_libraries(aion_gs_scenario_tests PRIVATE nlohmann_json::nlohmann_json)

	# cases that read the Java module directories
	aion_set_discovered_test_properties(aion_gs_scenario_tests REGEX "^(ScenarioDatabaseTest|ScenarioServersTest|ScenarioWiringTest)\\."
		PROPERTIES LABELS "scenario;realdata")
	# starts the real login server; since stage 3 it writes its own log directory (ScenarioServers::loginLogFolder) and binds ephemeral ports.
	# It holds the slot of the two m5a gates because its schema pair carries their prefix m5a (see "the two gate slots" above).
	aion_set_discovered_test_properties(aion_gs_scenario_tests REGEX "^LoginServerHarnessTest\\."
		PROPERTIES LABELS "scenario;realdata" RESOURCE_LOCK "${AION_GS_GATE_SLOT_1}" TIMEOUT 300)
	# OracleRunTest runs tools/oracle/oracle.py m5b2-skills over the real static data through Oracle::skills (OracleTest.cpp), with the
	# interpreter CMake found handed over the way the gates below get theirs; without one the case skips itself.
	if(Python3_Interpreter_FOUND)
		aion_set_discovered_test_properties(aion_gs_scenario_tests REGEX "^OracleRunTest\\." PROPERTIES LABELS "scenario;realdata"
			ENVIRONMENT_MODIFICATION "AION_TEST_PYTHON=set:${Python3_EXECUTABLE}")
	else()
		aion_set_discovered_test_properties(aion_gs_scenario_tests REGEX "^OracleRunTest\\." PROPERTIES LABELS "scenario;realdata")
	endif()
	# the gates run as gs.scenario.m5a and gs.scenario.m5a_geo below, never as discovered cases
	aion_set_discovered_test_properties(aion_gs_scenario_tests REGEX "^M5aScenario(Geo)?\\." PROPERTIES DISABLED TRUE
		LABELS "scenario;realdata")
	# the same for the M5b-1 gates (m5b-plan.md §6.5): M5bScenario.Run and M5bScenarioGeo.Run each own one pair of server processes
	aion_set_discovered_test_properties(aion_gs_scenario_tests REGEX "^M5bScenario(Geo)?\\." PROPERTIES DISABLED TRUE
		LABELS "scenario;realdata")
	# and for the M5b-2 gates (m5b2-plan.md G-03/G-04)
	aion_set_discovered_test_properties(aion_gs_scenario_tests REGEX "^M5b2Scenario(Geo)?\\." PROPERTIES DISABLED TRUE
		LABELS "scenario;realdata")
	# and for the M5b-3 gates (m5b3-plan.md G-03/G-04)
	aion_set_discovered_test_properties(aion_gs_scenario_tests REGEX "^M5b3Scenario(Geo)?\\." PROPERTIES DISABLED TRUE
		LABELS "scenario;realdata")

	# F-06: the M5a gate (§5.10). The oracle needs a Python interpreter; without it the test prints "gs.scenario.m5a: skipped".
	set(scenario_work_dir "${CMAKE_CURRENT_BINARY_DIR}/test_work/aion_gs_scenario_tests")
	file(MAKE_DIRECTORY "${scenario_work_dir}")
	add_test(NAME gs.scenario.m5a COMMAND "$<TARGET_FILE:aion_gs_scenario_tests>" --gtest_filter=M5aScenario.Run
		WORKING_DIRECTORY "${scenario_work_dir}")
	set_tests_properties(gs.scenario.m5a PROPERTIES LABELS "scenario;realdata" TIMEOUT 900
		RESOURCE_LOCK "${AION_GS_GATE_SLOT_1}" SKIP_REGULAR_EXPRESSION "gs\\.scenario\\.m5a: skipped")
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
	# It holds the SAME gate slot as gs.scenario.m5a, so the two never run at the same time: they share the schema prefix m5a, whose sweep must
	# not run beside another run of the prefix (see "the two gate slots" above).
	add_test(NAME gs.scenario.m5a_geo COMMAND "$<TARGET_FILE:aion_gs_scenario_tests>" --gtest_filter=M5aScenarioGeo.Run
		WORKING_DIRECTORY "${scenario_work_dir}")
	# TIMEOUT: the measured run is 18-21 s in this tree, but a Debug build starts in 144 s (m5a-client-session.md) and the timeout has to hold for
	# a loaded machine as well; RunStartupSmoke.cmake gives its own geo mode 2100 s of server time for the same reason.
	set_tests_properties(gs.scenario.m5a_geo PROPERTIES LABELS "scenario;realdata;geo" TIMEOUT 2700
		RESOURCE_LOCK "${AION_GS_GATE_SLOT_1}" SKIP_REGULAR_EXPRESSION "gs\\.scenario\\.m5a_geo: skipped")
	if(Python3_Interpreter_FOUND)
		set_property(TEST gs.scenario.m5a_geo APPEND PROPERTY ENVIRONMENT_MODIFICATION "AION_TEST_PYTHON=set:${Python3_EXECUTABLE}")
	endif()
	if(AION_SCENARIO_REQUIRE OR NOT AION_GS_ALLOW_MILESTONE_SKIP)
		set_property(TEST gs.scenario.m5a_geo APPEND PROPERTY ENVIRONMENT_MODIFICATION "AION_SCENARIO_REQUIRE=set:1")
	endif()

	# ---- the M5b-1 gate (m5b-plan.md G-03/G-06, §6.5) ------------------------------------------------------------------------------------
	#
	# gs.scenario.m5b: the scripted fight of §6.2 - approach, target, an out-of-range shot, the kill, the reward, the respawn, the character's
	# own death and its bind revive - in the same binary, with its own output directory <bin>/scenario/m5b, its own schema pair
	# (aion_gs_test_m5b_<hash>, ScenarioServers::Config::schemaPrefix) and its own AION_PARTIAL allow-list. It holds gate slot 2, together with
	# its geo variant (one slot per schema prefix) and the two M5b-2 gates: the user works at this machine, and a third geo-capable server beside
	# two others is what the slots prevent (§8 risk 15). TIMEOUT 900 like gs.scenario.m5a; the run itself is budgeted at 60-90 s, of which K7's
	# respawn assertion alone costs the spawn's 20 s respawn time and K8's death costs the monster's attack tempo.
	add_test(NAME gs.scenario.m5b COMMAND "$<TARGET_FILE:aion_gs_scenario_tests>" --gtest_filter=M5bScenario.Run
		WORKING_DIRECTORY "${scenario_work_dir}")
	set_tests_properties(gs.scenario.m5b PROPERTIES LABELS "scenario;realdata" TIMEOUT 900
		RESOURCE_LOCK "${AION_GS_GATE_SLOT_2}" SKIP_REGULAR_EXPRESSION "gs\\.scenario\\.m5b: skipped")
	if(Python3_Interpreter_FOUND)
		set_property(TEST gs.scenario.m5b APPEND PROPERTY ENVIRONMENT_MODIFICATION "AION_TEST_PYTHON=set:${Python3_EXECUTABLE}")
	endif()
	# A SKIPPED GATE IS NOT A PASSED GATE - the same default and the same opt-out as the two gates above (§6.5)
	if(AION_SCENARIO_REQUIRE OR NOT AION_GS_ALLOW_MILESTONE_SKIP)
		set_property(TEST gs.scenario.m5b APPEND PROPERTY ENVIRONMENT_MODIFICATION "AION_SCENARIO_REQUIRE=set:1")
	endif()

	# gs.scenario.m5b_geo (G-06): the same scripted fight with -Dgameserver.geodata.enable=true. It is the only run in which
	# GeoService::canSee can answer false on the attack path, and the only one in which R4's respawn position is asserted against a world that
	# has its terrain - which is what a geo z-snap on the respawn path breaks (§6.6, the same argument wave B made for gs.scenario.m5a_geo).
	# TIMEOUT 2700 for the same reason as gs.scenario.m5a_geo: the geo startup is seconds in a checked RelWithDebInfo tree and minutes in Debug.
	add_test(NAME gs.scenario.m5b_geo COMMAND "$<TARGET_FILE:aion_gs_scenario_tests>" --gtest_filter=M5bScenarioGeo.Run
		WORKING_DIRECTORY "${scenario_work_dir}")
	set_tests_properties(gs.scenario.m5b_geo PROPERTIES LABELS "scenario;realdata;geo" TIMEOUT 2700
		RESOURCE_LOCK "${AION_GS_GATE_SLOT_2}" SKIP_REGULAR_EXPRESSION "gs\\.scenario\\.m5b_geo: skipped")
	if(Python3_Interpreter_FOUND)
		set_property(TEST gs.scenario.m5b_geo APPEND PROPERTY ENVIRONMENT_MODIFICATION "AION_TEST_PYTHON=set:${Python3_EXECUTABLE}")
	endif()
	if(AION_SCENARIO_REQUIRE OR NOT AION_GS_ALLOW_MILESTONE_SKIP)
		set_property(TEST gs.scenario.m5b_geo APPEND PROPERTY ENVIRONMENT_MODIFICATION "AION_SCENARIO_REQUIRE=set:1")
	endif()

	# ---- the M5b-2 gate (m5b2-plan.md G-03/G-04, §10) ------------------------------------------------------------------------------------
	#
	# gs.scenario.m5b2: the abilities of §10.2 - an Elyos Warrior and an Elyos Mage on one account, the instant chain skill and its cooldown, an
	# npc's skill cast at the Warrior (X9), the cast bar with its MP cost and its interruption, a 20-second debuff on a monster, a two-template
	# self-buff, a heal, the enter-world passives,
	# the revive debuff and the saved effect and cooldown of a quit - in the same binary, with its own output directory <bin>/scenario/m5b2, its
	# own schema pair (aion_gs_test_m5b2_<hash>) and its own AION_PARTIAL allow-list, in gate slot 2 with its geo variant and the M5b gates.
	# TIMEOUT 900 like gs.scenario.m5b: the run is budgeted at three to four minutes, of which the Root's lifetime and the cooldowns are a
	# minute of deliberate waiting.
	add_test(NAME gs.scenario.m5b2 COMMAND "$<TARGET_FILE:aion_gs_scenario_tests>" --gtest_filter=M5b2Scenario.Run
		WORKING_DIRECTORY "${scenario_work_dir}")
	set_tests_properties(gs.scenario.m5b2 PROPERTIES LABELS "scenario;realdata" TIMEOUT 900
		RESOURCE_LOCK "${AION_GS_GATE_SLOT_2}" SKIP_REGULAR_EXPRESSION "gs\\.scenario\\.m5b2: skipped")
	if(Python3_Interpreter_FOUND)
		set_property(TEST gs.scenario.m5b2 APPEND PROPERTY ENVIRONMENT_MODIFICATION "AION_TEST_PYTHON=set:${Python3_EXECUTABLE}")
	endif()
	if(AION_SCENARIO_REQUIRE OR NOT AION_GS_ALLOW_MILESTONE_SKIP)
		set_property(TEST gs.scenario.m5b2 APPEND PROPERTY ENVIRONMENT_MODIFICATION "AION_SCENARIO_REQUIRE=set:1")
	endif()

	# gs.scenario.m5b2_geo (G-04): the same script with -Dgameserver.geodata.enable=true. The one geo check on the cast path of these skills is
	# FirstTargetRangeProperty's GeoService.canSee at the cast start and end, which this run exercises for targets in the open (see the comment
	# above TEST(M5b2ScenarioGeo, Run) for what it cannot assert and why). TIMEOUT 2700 for the geo startup, as the other geo gates.
	add_test(NAME gs.scenario.m5b2_geo COMMAND "$<TARGET_FILE:aion_gs_scenario_tests>" --gtest_filter=M5b2ScenarioGeo.Run
		WORKING_DIRECTORY "${scenario_work_dir}")
	set_tests_properties(gs.scenario.m5b2_geo PROPERTIES LABELS "scenario;realdata;geo" TIMEOUT 2700
		RESOURCE_LOCK "${AION_GS_GATE_SLOT_2}" SKIP_REGULAR_EXPRESSION "gs\\.scenario\\.m5b2_geo: skipped")
	if(Python3_Interpreter_FOUND)
		set_property(TEST gs.scenario.m5b2_geo APPEND PROPERTY ENVIRONMENT_MODIFICATION "AION_TEST_PYTHON=set:${Python3_EXECUTABLE}")
	endif()
	if(AION_SCENARIO_REQUIRE OR NOT AION_GS_ALLOW_MILESTONE_SKIP)
		set_property(TEST gs.scenario.m5b2_geo APPEND PROPERTY ENVIRONMENT_MODIFICATION "AION_SCENARIO_REQUIRE=set:1")
	endif()

	# ---- the M5b-3 gate (m5b3-plan.md G-03/G-04, §10) ------------------------------------------------------------------------------------
	#
	# gs.scenario.m5b3: the loot and item cases of §10.2 at gameserver.rates.drop = 1000000 (m5b3.properties.example) - the Elyos Warrior loots
	# two 210663 corpses and a 210133 corpse empty, kinah included, unequips and re-equips its sword, sockets a seeded godstone and sees it proc,
	# drinks a potion, moves, splits, destroys and swaps items and reads its inventory back from the database after a quit - in the same binary,
	# with its own output directory <bin>/scenario/m5b3, its own schema pair (aion_gs_test_m5b3_<hash>) and its own AION_PARTIAL allow-list,
	# in gate slot 1 with its geo variant, the M5a gates and the smoke and M4 tests. TIMEOUT 900 like gs.scenario.m5b2: the run is budgeted at
	# four to six minutes, of which three fights, three relogs and the oracle's cube budgets are most.
	add_test(NAME gs.scenario.m5b3 COMMAND "$<TARGET_FILE:aion_gs_scenario_tests>" --gtest_filter=M5b3Scenario.Run
		WORKING_DIRECTORY "${scenario_work_dir}")
	set_tests_properties(gs.scenario.m5b3 PROPERTIES LABELS "scenario;realdata" TIMEOUT 900
		RESOURCE_LOCK "${AION_GS_GATE_SLOT_1}" SKIP_REGULAR_EXPRESSION "gs\\.scenario\\.m5b3: skipped")
	if(Python3_Interpreter_FOUND)
		set_property(TEST gs.scenario.m5b3 APPEND PROPERTY ENVIRONMENT_MODIFICATION "AION_TEST_PYTHON=set:${Python3_EXECUTABLE}")
	endif()
	if(AION_SCENARIO_REQUIRE OR NOT AION_GS_ALLOW_MILESTONE_SKIP)
		set_property(TEST gs.scenario.m5b3 APPEND PROPERTY ENVIRONMENT_MODIFICATION "AION_SCENARIO_REQUIRE=set:1")
	endif()

	# gs.scenario.m5b3_geo (G-04, §10.5): the same script with -Dgameserver.geodata.enable=true plus the camp fire (case L14, Y15), which only a
	# server with the geo meshes has - the material zones are created from them. TIMEOUT 2700 for the geo startup, as the other geo gates.
	add_test(NAME gs.scenario.m5b3_geo COMMAND "$<TARGET_FILE:aion_gs_scenario_tests>" --gtest_filter=M5b3ScenarioGeo.Run
		WORKING_DIRECTORY "${scenario_work_dir}")
	set_tests_properties(gs.scenario.m5b3_geo PROPERTIES LABELS "scenario;realdata;geo" TIMEOUT 2700
		RESOURCE_LOCK "${AION_GS_GATE_SLOT_1}" SKIP_REGULAR_EXPRESSION "gs\\.scenario\\.m5b3_geo: skipped")
	if(Python3_Interpreter_FOUND)
		set_property(TEST gs.scenario.m5b3_geo APPEND PROPERTY ENVIRONMENT_MODIFICATION "AION_TEST_PYTHON=set:${Python3_EXECUTABLE}")
	endif()
	if(AION_SCENARIO_REQUIRE OR NOT AION_GS_ALLOW_MILESTONE_SKIP)
		set_property(TEST gs.scenario.m5b3_geo APPEND PROPERTY ENVIRONMENT_MODIFICATION "AION_SCENARIO_REQUIRE=set:1")
	endif()
endif()
