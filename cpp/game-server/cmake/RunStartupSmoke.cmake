# CTests gs.smoke.startup, gs.smoke.startup_progress and gs.smoke.startup_geo (spine step S0a link proof, milestone M4 startup path,
# m5a-plan.md F-01a/F-02, stage 3 geo gate): runs aion_game_server in the Java game-server directory with the M5a profile (D1) and a stop file,
# and checks the startup in Java order (Config -> database -> runtime with IDFactory -> DataManager -> engines -> World -> services -> spawns ->
# ... -> "Game server started" -> LoginServer connect) and the shutdown. MODE smoke and progress switch the geo data off
# (-Dgameserver.geodata.enable=false), because loading data/geo takes minutes and about 5 GB in a Debug build; MODE geo is the run that switches
# it on. The client socket binds an ephemeral port (127.0.0.1:0) and the login server link points to a closed port (127.0.0.1:1), so a game or
# login server running on this machine is never touched.
#
# The stop file (--stop-file) is written before the server starts: the server checks it after "Game server started" and then shuts down like on
# Ctrl+C (with nobody online the ShutdownHook takes its fast exit). The plan (F-02) writes it after "Game server started"; writing it up front is
# equivalent because the run loop is only entered once the whole startup finished, and the run is proved to have consumed it by the log line
# "Stop file ... found, shutting down" (docs/deviations/P5-14.md, row "Run mode").
#
# Every mode checks the --check-output reports of the run (m5a-plan.md F-02, F-07, D8): the seven report files exist, m5a_summary.txt agrees with
# whether the server started, a started run really ran the final census, and its summary has 0 AION_UNPORTED hits (smoke and geo, F-01b). This is
# the only exercise of that path in a real server process. The W-07 row `knownListNotifyFailures` is only required to be present: a startup
# without a client builds no known list, so its value cannot be anything but 0 here (see the comment at that check).
#
#   cmake -DEXECUTABLE=<aion_game_server> -DDATABASE_TOOL=<aion_gs_m4_database> -DWORKING_DIRECTORY=<game-server> -DMODE=smoke|progress|geo
#         [-DREQUIRE_STARTED=ON|OFF] [-DALLOW_SKIP=ON|OFF] [-DOUTPUT_DIR=<dir>] -P RunStartupSmoke.cmake
#
# MODE smoke (gs.smoke.startup): passes when the whole startup ran ("Game server started") with zero AION_UNPORTED hits (m5a-plan.md F-01b; the
# AION_PARTIAL sites it reached are printed), the stop file shut the server down in order ("Runtime shut down: ...") and the exit code is 0. With
# REQUIRE_STARTED OFF (stage 1 of wave 5a, before F-01b), a startup that loaded all static data and created the world but then stopped at an
# unported function was reported as skipped (it names the function); stage 2 sets REQUIRE_STARTED ON (cmake/AppTests.cmake).
# MODE progress (gs.smoke.startup_progress): prints the last startup step reached ("startup step N: name", m5a-plan.md F-01a) and passes when
# the steps are numbered without gaps from 1 and the run ended in order: either started and stopped by the stop file with exit code 0, or
# stopped at a step (an unported function or an exception) with exit code 1 and an orderly runtime shutdown. A crash, a hang, a missing step
# log or a missing runtime shutdown fail.
# MODE geo (gs.smoke.startup_geo, stage 3 of wave 5a): the same startup with the geo data ENABLED, which is the only configuration in which a
# zone handler runs at all. It must reach "Game server started", load the geo data and the npc spawns, and end with 0 spawn failures,
# 0 AION_UNPORTED hits and no ERROR in its own log folder except the one the harness causes itself. See the MODE geo section below for what each
# check is worth.
#
# Database (the environment names it): AION_TEST_GS_DATABASE_URL names the MariaDB server (e.g.
# jdbc:mysql://127.0.0.1:3306/aion_cpp_test?characterEncoding=UTF-8; that database must exist), optionally AION_TEST_GS_DATABASE_USER /
# AION_TEST_GS_DATABASE_PASSWORD (default: root without password). The test creates its own game server schema (aion_gs_test_smoke_<hash>,
# aion_gs_test_smoke_progress_<hash>, aion_gs_test_smoke_geo_<hash>, the first 12 hex digits of the MD5 of OUTPUT_DIR like RunM4Check.cmake)
# from game-server/sql/aion_gs.sql with aion_gs_m4_database, passes it as -Ddatabase.* overrides (never the database of
# config/network/database.properties) and drops it again when the script ends.
#
# A FAILED RUN DROPS ITS SCHEMA TOO (stage 3 wave B, low finding of the wave A review): only the three success paths used to call `drop`, and a
# CMake script has no finally, so every `message(FATAL_ERROR ...)` after the create left a 61-table schema behind - one per failing run and per
# build tree, forever, because the name carries the hash of OUTPUT_DIR and nothing ever sweeps it. All those exits now go through `fail()`, which
# drops first. What a post mortem reads (the server log, the stderr log, the --log-folder files and the --check-output reports) is in OUTPUT_DIR
# and is kept either way; the schema itself is worth keeping only when the failure is about the database, so that is the opt-in:
# AION_GS_KEEP_TEST_SCHEMA=1 in the environment keeps it and the failure message names it (the same idea as AION_SCENARIO_KEEP_SCHEMAS in
# tests/scenario/ScenarioServers.cpp).
#
# A MISSING PREREQUISITE FAILS (stage 3, "no silent green"): without the database URL, without the Java checkout, and for MODE geo without
# data/geo, these tests used to report themselves skipped - and CTest counts a skip as a pass, so a plain `ctest` declared the milestone green
# without running it. They now fail and name the variable to set. The opt-out is explicit (cpp/README.md): configure with
# -DAION_GS_ALLOW_MILESTONE_SKIP=ON or run ctest with AION_GS_ALLOW_MILESTONE_SKIP=1 in the environment; the output is then
# "gs.smoke.startup: skipped" / "gs.smoke.startup_progress: skipped" / "gs.smoke.startup_geo: skipped" again (SKIP_REGULAR_EXPRESSION).
#
# The server writes its log files to <OUTPUT_DIR>/log (--log-folder), not to the shared <WORKING_DIRECTORY>/log of a normal start: Logging::init
# archives and deletes the *.log files of the previous run, so a second server process in the same directory - another build directory's test
# run, or a game server the user started - makes both runs fail with "Error gathering and archiving old logs". Together with the hashed schema
# name this makes the test independent of what else runs on the machine (CTest's RESOURCE_LOCK only serializes one ctest run).

if(NOT DEFINED MODE)
	set(MODE smoke)
endif()
if(MODE STREQUAL "smoke")
	set(test_name gs.smoke.startup)
	set(database_prefix aion_gs_test_smoke)
	set(geodata_enable false)
	set(server_timeout 540)
elseif(MODE STREQUAL "progress")
	set(test_name gs.smoke.startup_progress)
	set(database_prefix aion_gs_test_smoke_progress)
	set(geodata_enable false)
	set(server_timeout 540)
elseif(MODE STREQUAL "geo")
	set(test_name gs.smoke.startup_geo)
	set(database_prefix aion_gs_test_smoke_geo)
	set(geodata_enable true)
	# The reference geo startup of the integrator's machine takes 144 seconds and about 5 GB in a Debug build (m5a-client-session.md "Setup"):
	# the headroom is for a slower or loaded machine, and the CTest TIMEOUT in cmake/AppTests.cmake is larger again.
	set(server_timeout 2100)
	set(REQUIRE_STARTED ON) # a geo startup that does not reach "Game server started" is a failure, never a skip
else()
	message(FATAL_ERROR "RunStartupSmoke.cmake: unknown MODE '${MODE}' (smoke, progress or geo)")
endif()
if(NOT DEFINED REQUIRE_STARTED)
	set(REQUIRE_STARTED ON)
endif()

# NO SILENT GREEN (m5a-client-session.md; m5a-plan.md §5.10 "A skipped gate is not a passed gate"). A milestone test whose prerequisite is
# missing FAILS: CTest counts a skipped test as passed, so a default `ctest` without AION_TEST_GS_DATABASE_URL used to report the milestone
# green while it was never executed. The opt-out is explicit and documented in cpp/README.md: the cache option
# AION_GS_ALLOW_MILESTONE_SKIP=ON (cmake/AppTests.cmake passes its value as -DALLOW_SKIP) or the environment variable
# AION_GS_ALLOW_MILESTONE_SKIP=1 of a single ctest run (no reconfigure needed).
if(NOT DEFINED ALLOW_SKIP)
	set(ALLOW_SKIP OFF)
endif()
set(allow_skip ${ALLOW_SKIP})
if(DEFINED ENV{AION_GS_ALLOW_MILESTONE_SKIP} AND NOT "$ENV{AION_GS_ALLOW_MILESTONE_SKIP}" STREQUAL ""
		AND NOT "$ENV{AION_GS_ALLOW_MILESTONE_SKIP}" STREQUAL "0")
	set(allow_skip ON)
endif()

# A missing prerequisite: skipped with the opt-out (SKIP_REGULAR_EXPRESSION matches "<test>: skipped"), a failure without it. Both messages name
# the variable and the value to set. This is a macro, not a function, so that its return() ends the script.
macro(milestone_prerequisite missing fix)
	if(allow_skip)
		message("${test_name}: skipped (${missing}; ${fix})")
		return()
	endif()
	message(FATAL_ERROR "${test_name}: ${missing}.\n"
		"  Fix: ${fix}\n"
		"  Or opt out of the milestone tests explicitly: configure with -DAION_GS_ALLOW_MILESTONE_SKIP=ON, or run ctest with "
		"AION_GS_ALLOW_MILESTONE_SKIP=1 in the environment. Missing prerequisites fail by default because CTest reports a skipped test as "
		"passed, so a run without them would declare the milestone green without executing it.")
endmacro()

if(NOT DEFINED OUTPUT_DIR)
	set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/${test_name}")
endif()
# The schema name carries the hash of OUTPUT_DIR (like RunM4Check.cmake) and the log directory is OUTPUT_DIR/log, so two build directories
# running this test at the same time neither drop each other's schema nor archive each other's log files (Logging::init deletes what it archives).
string(MD5 output_hash "${OUTPUT_DIR}")
string(SUBSTRING "${output_hash}" 0 12 output_hash)
set(database "${database_prefix}_${output_hash}")

if(NOT DEFINED ENV{AION_TEST_GS_DATABASE_URL} OR "$ENV{AION_TEST_GS_DATABASE_URL}" STREQUAL "")
	string(CONCAT database_fix "set AION_TEST_GS_DATABASE_URL=jdbc:mysql://127.0.0.1:3306/aion_cpp_test?characterEncoding=UTF-8 (that database "
		"must exist and MariaDB must be running), optionally AION_TEST_GS_DATABASE_USER / AION_TEST_GS_DATABASE_PASSWORD (default: root without "
		"password)")
	milestone_prerequisite("the environment variable AION_TEST_GS_DATABASE_URL is not set, so the server has no test schema to run against"
		"${database_fix}")
endif()
# The Java checkout is the server's working directory: its config/ and data/ are read at startup, and MODE geo needs data/geo. A missing tree is
# a prerequisite like the database, not a reason to pass.
foreach(java_file IN ITEMS config/logback.xml config/main data/static_data)
	if(NOT EXISTS "${WORKING_DIRECTORY}/${java_file}")
		milestone_prerequisite("the Java game server checkout has no ${java_file} (working directory '${WORKING_DIRECTORY}')"
			"check out the Java server tree (the cpp/ directory lives inside it) so that game-server/${java_file} exists")
	endif()
endforeach()
if(MODE STREQUAL "geo")
	file(GLOB geo_files "${WORKING_DIRECTORY}/data/geo/*.geo")
	if(NOT geo_files)
		string(CONCAT geo_missing "the Java game server checkout has no data/geo/*.geo files, which this test exists to load (working directory "
			"'${WORKING_DIRECTORY}')")
		milestone_prerequisite("${geo_missing}" "check out or unpack game-server/data/geo (151 .geo files in the 4.8 tree)")
	endif()
	list(LENGTH geo_files geo_file_count)
	message("${test_name}: ${geo_file_count} files in ${WORKING_DIRECTORY}/data/geo")
endif()
foreach(file IN ITEMS "${EXECUTABLE}" "${DATABASE_TOOL}")
	if(NOT EXISTS "${file}")
		message(FATAL_ERROR "${test_name}: ${file} does not exist (build aion_game_server and aion_gs_m4_database first)")
	endif()
endforeach()

set(url "$ENV{AION_TEST_GS_DATABASE_URL}")
if(NOT url MATCHES "^(jdbc:[A-Za-z]+://[^/?]+)(/[^?]*)?(\\?.*)?$")
	message(FATAL_ERROR "${test_name}: cannot parse AION_TEST_GS_DATABASE_URL '${url}'")
endif()
set(schema_url "${CMAKE_MATCH_1}/${database}${CMAKE_MATCH_3}")

# The schema this run owns: created below, dropped by drop_schema() on every exit after that, success or failure (see the header comment).
# AION_GS_KEEP_TEST_SCHEMA keeps it for a post mortem; drop_schema() is a no-op before the create and after a drop, so it can be called twice.
set(keep_schema OFF)
if(DEFINED ENV{AION_GS_KEEP_TEST_SCHEMA} AND NOT "$ENV{AION_GS_KEEP_TEST_SCHEMA}" STREQUAL "" AND NOT "$ENV{AION_GS_KEEP_TEST_SCHEMA}" STREQUAL "0")
	set(keep_schema ON)
endif()
set(database_created FALSE)
macro(drop_schema)
	if(database_created)
		if(keep_schema)
			message("${test_name}: keeping the schema ${database} (AION_GS_KEEP_TEST_SCHEMA); drop it with "
				"\"${DATABASE_TOOL} drop ${database}\"")
		else()
			execute_process(COMMAND "${DATABASE_TOOL}" drop ${database} OUTPUT_QUIET ERROR_QUIET TIMEOUT 120)
		endif()
		set(database_created FALSE)
	endif()
endmacro()
# Every failure exit after the create goes through this instead of message(FATAL_ERROR ...), so that the schema never outlives the run.
# message(FATAL_ERROR) ends the whole script, inside a function as well, and a function can join its arguments the way message() itself does.
function(fail)
	drop_schema()
	set(text "")
	math(EXPR last "${ARGC} - 1")
	foreach(index RANGE 0 ${last})
		string(APPEND text "${ARGV${index}}")
	endforeach()
	message(FATAL_ERROR "${text}")
endfunction()

execute_process(COMMAND "${DATABASE_TOOL}" create ${database} OUTPUT_VARIABLE created ERROR_VARIABLE create_errors RESULT_VARIABLE create_result
	TIMEOUT 300)
if(NOT create_result STREQUAL "0")
	# the create drops the database before it creates it, so a failed create may still have left one behind
	set(database_created TRUE)
	fail("${test_name}: could not create ${database} (${create_errors}). Start MariaDB or fix AION_TEST_GS_DATABASE_URL, and run the test again.")
endif()
set(database_created TRUE)
message("${created}")

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(stop_file "${OUTPUT_DIR}/stop")
file(WRITE "${stop_file}" "stop\n")

set(arguments "-Ddatabase.url=${schema_url}")
if(DEFINED ENV{AION_TEST_GS_DATABASE_USER})
	list(APPEND arguments "-Ddatabase.user=$ENV{AION_TEST_GS_DATABASE_USER}")
else()
	list(APPEND arguments "-Ddatabase.user=root")
endif()
list(APPEND arguments "-Ddatabase.password=$ENV{AION_TEST_GS_DATABASE_PASSWORD}")
# the M5a profile (m5a-plan.md D1; game-server/config/m5a.properties.example) and the scenario keys
list(APPEND arguments "-Dgameserver.dev.missing_ai_handlers=warn" "-Dgameserver.siege.enable=false" "-Dgameserver.autogroup.enable=false"
	"-Dgameserver.rift.enable=false" "-Dgameserver.vortex.enable=false" "-Dgameserver.worldraid.enable=false" "-Dgameserver.cp.enable=false"
	"-Dgameserver.limits.enable=false" "-Dgameserver.event.service.disabled_events=*" "-Dgameserver.geodata.enable=${geodata_enable}"
	"-Dgameserver.shutdown.delay=2")
# the client socket takes an ephemeral port and the login link a closed one, so no server running on this machine is touched (MODE geo names
# login_address again: LoginServer retries this address for the whole run, and the retry that lands during the shutdown logs an ERROR)
set(login_address "127.0.0.1:1")
list(APPEND arguments "-Dgameserver.network.client.socket_address=127.0.0.1:0" "-Dgameserver.network.login.address=${login_address}")
list(APPEND arguments "--stop-file=${stop_file}" "--check-output=${OUTPUT_DIR}/check" "--log-folder=${OUTPUT_DIR}/log")

execute_process(
	COMMAND "${EXECUTABLE}" ${arguments}
	WORKING_DIRECTORY "${WORKING_DIRECTORY}"
	OUTPUT_FILE "${OUTPUT_DIR}/server.log"
	ERROR_FILE "${OUTPUT_DIR}/server.stderr.log"
	RESULT_VARIABLE result
	TIMEOUT ${server_timeout}
)
file(READ "${OUTPUT_DIR}/server.log" log)
file(READ "${OUTPUT_DIR}/server.stderr.log" errors)
string(APPEND log "${errors}")
string(LENGTH "${log}" length)
if(length GREATER 12000)
	math(EXPR start "${length} - 12000")
	string(SUBSTRING "${log}" ${start} 12000 tail)
else()
	set(tail "${log}")
endif()
message("aion_game_server: exit code ${result}, log ${OUTPUT_DIR}/server.log (last 12000 characters):\n${tail}")

if(result MATCHES "timeout|Process terminated due to timeout")
	fail("${test_name}: aion_game_server did not exit within ${server_timeout} seconds")
endif()
if(log MATCHES "Failed to initialize pool|DatabaseFactory is not initialized|SQLException")
	if(NOT log MATCHES "startup step [0-9]+: DataManager")
		fail("${test_name}: the game server could not use the database ${schema_url}. Start MariaDB or fix AION_TEST_GS_DATABASE_URL, and run "
			"the test again.")
	endif()
endif()

# the startup steps: numbered from 1 without gaps
string(REGEX MATCHALL "startup step [0-9]+: [^\r\n]*" step_lines "${log}")
list(LENGTH step_lines step_count)
set(expected 1)
set(last_step "none")
foreach(line IN LISTS step_lines)
	string(REGEX MATCH "^startup step ([0-9]+): (.*)$" matched "${line}")
	if(NOT CMAKE_MATCH_1 STREQUAL "${expected}")
		fail("${test_name}: startup step ${CMAKE_MATCH_1} logged where step ${expected} was expected (${line})")
	endif()
	set(last_step "${CMAKE_MATCH_1}: ${CMAKE_MATCH_2}")
	math(EXPR expected "${expected} + 1")
endforeach()
if(step_count EQUAL 0)
	fail("${test_name}: no startup step was logged (exit code '${result}')")
endif()
# --log-folder: the run's log files are its own, so no other server process archives or deletes them (and this one archives none of theirs)
if(NOT EXISTS "${OUTPUT_DIR}/log/server_console.log")
	fail("${test_name}: the server wrote no ${OUTPUT_DIR}/log/server_console.log (--log-folder was not honoured)")
endif()

set(started FALSE)
if(log MATCHES "Game server started in [0-9-]+ seconds\\.")
	set(started TRUE)
endif()
set(stopped_at "")
if(log MATCHES "startup stopped at an unported function: ([^\r\n]*)")
	set(stopped_at "unported function ${CMAKE_MATCH_1}")
elseif(log MATCHES "Critical Error - Thread \\[main\\] terminated abnormally:([^\r\n]*)")
	set(stopped_at "exception${CMAKE_MATCH_1}")
endif()
set(orderly FALSE)
if(log MATCHES "Runtime shut down:")
	set(orderly TRUE)
endif()
message("${test_name}: ${step_count} startup steps, last step ${last_step}; started ${started}; stopped at: ${stopped_at}")

# The --check-output reports (m5a-plan.md F-02, F-07, D8). This is the only place where they are produced by a real server process; without it
# the writers are exercised only by CheckOutputTest's fakes, and the plan's §5.7 Q8 would first read them in the stage-2 gate.
set(check_dir "${OUTPUT_DIR}/check")
set(report_files unported_trace.txt partial_trace.txt census.txt live_counts.txt lockdep.txt watchdog.txt m5a_summary.txt)
if(started)
	list(APPEND report_files live_counts_baseline.txt) # written right after "Game server started"
endif()
foreach(report IN LISTS report_files)
	if(NOT EXISTS "${check_dir}/${report}")
		fail("${test_name}: --check-output wrote no ${report} in ${check_dir} (started ${started}; m5a-plan.md F-02/F-07)")
	endif()
endforeach()
file(READ "${check_dir}/m5a_summary.txt" summary)
if(started)
	if(NOT summary MATCHES "started true")
		fail("${test_name}: m5a_summary.txt does not say \"started true\":\n${summary}")
	endif()
	# F-07: the final census ran in a real process (the file always carries its header line, and the hits are counted)
	file(READ "${check_dir}/census.txt" census)
	if(NOT census MATCHES "# final census v1")
		fail("${test_name}: census.txt has no final census header:\n${census}")
	endif()
	if(NOT log MATCHES "Final census: [0-9]+ leaks written to")
		fail("${test_name}: the final census did not run before the runtime shutdown (F-07)")
	endif()
	file(READ "${check_dir}/live_counts.txt" live_counts)
	string(LENGTH "${live_counts}" live_counts_length)
	if(live_counts_length EQUAL 0)
		fail("${test_name}: live_counts.txt is empty")
	endif()
	# F-01b: the whole startup path must reach no AION_UNPORTED body at all. The counter covers every thread (the spawn and zone paths swallow
	# the UnportedException, so a body reached there would otherwise only show up as an ERROR line).
	if(NOT summary MATCHES "unportedHits ([0-9]+)")
		fail("${test_name}: m5a_summary.txt has no \"unportedHits\" line:\n${summary}")
	endif()
	set(unported_hits "${CMAKE_MATCH_1}")
	if(NOT unported_hits STREQUAL "0")
		file(READ "${check_dir}/unported_trace.txt" unported_trace)
		# MODE geo requires this as much as MODE smoke does - its whole point is the code that only a geo-built world reaches, and an
		# AION_UNPORTED body there is swallowed by the spawn and zone paths (wave A only failed MODE smoke here, although its report and the
		# section comment below both say the geo test fails on an unported hit; stage 3 wave B).
		if(NOT MODE STREQUAL "progress")
			fail("${test_name}: the startup reached ${unported_hits} AION_UNPORTED hits; m5a-plan.md F-01b requires none:\n" "${unported_trace}")
		endif()
		# progress mode stays the diagnostic of F-01a: it names what the startup still reaches instead of failing
		message("${test_name}: the startup reached ${unported_hits} AION_UNPORTED hits (gs.smoke.startup fails on them, F-01b):\n${unported_trace}")
	endif()
	# W-07, the notify-failure counter: NOT asserted here, and that is a measurement, not an oversight (stage 3 wave B). A startup without a
	# client builds no known list at all - `NpcKnownList::update` returns to `clear()` while `isMapRegionActive()` is false (NpcKnownList.cpp:15-20),
	# and a map region is only activated by a player entering it - so `live_counts.txt` of a startup run contains no
	# `world::knownlist::KnownObject` row whatsoever (measured for MODE smoke and MODE geo; the scenario gate, which does connect a client,
	# reports `0 330 KnownObject` in the same file). `knownListNotifyFailures` can therefore only ever read 0 in this test, and asserting it
	# would be one more check that cannot fail. Only `gs.scenario.m5a` / `gs.scenario.m5a_geo` exercise a notification at all (§5.7 Q8 asserts
	# the row there). The row is printed with the rest of the summary below.
	if(NOT summary MATCHES "knownListNotifyFailures ([0-9]+)")
		fail("${test_name}: m5a_summary.txt has no \"knownListNotifyFailures\" line (m5a-plan.md W-07):\n" "${summary}")
	endif()
	# The AION_PARTIAL sites of the startup are allowed (D3), but the gate (F-06) has to know them: print them with their hit counts.
	file(READ "${check_dir}/partial_trace.txt" partial_trace)
	message("${test_name}: AION_PARTIAL sites reached by the startup (m5a-plan.md D3):\n${partial_trace}")
elseif(NOT summary MATCHES "started false")
	fail("${test_name}: m5a_summary.txt does not say \"started false\" although the server did not start:\n${summary}")
endif()
message("${test_name}: check output in ${check_dir}:\n${summary}")

# MODE geo (stage 3 of wave 5a). Zone handlers run only with the geo data loaded, so until this test no automated run had ever executed one: that
# is how the user's first real client lost 13 npc spawns to an unported SiegeShield::onEnterZone (m5a-client-session.md F-1), which
# m5a-plan.md §5.1 "Geodata" had predicted in writing. The four requirements of the geo gate:
#   1. the startup reaches "Game server started" (REQUIRE_STARTED is forced ON for this mode),
#   2. the run's own log folder and console stream have no unexpected ERROR entry - scanned per file in the way that file can be scanned, and
#      failing when nothing in the folder can be scanned that way at all (see the section below),
#   3. m5a_summary.txt counts 0 AION_UNPORTED hits (checked above for smoke and geo - the counter, not the log, because the spawn and zone paths
#      swallow the UnportedException),
#   4. no spawn failed ("Error during spawn" / "did not leave world cleanly", the F-1 signature).
# It also proves that the geo data was really loaded, so that a geo gate can never pass with geo off.
if(MODE STREQUAL "geo" AND started)
	# prints the first ${limit} entries of a list, indented
	function(first_entries entries limit out)
		list(LENGTH entries total)
		set(text "")
		set(index 0)
		foreach(entry IN LISTS entries)
			if(index GREATER_EQUAL limit)
				string(APPEND text "  ... (${total} in total)\n")
				break()
			endif()
			string(APPEND text "  ${entry}\n")
			math(EXPR index "${index} + 1")
		endforeach()
		set(${out} "${text}" PARENT_SCOPE)
	endfunction()

	set(geo_loaded "")
	if(log MATCHES "Loaded ([0-9]+) entities on ([0-9]+) maps")
		set(geo_loaded "${CMAKE_MATCH_1} entities on ${CMAKE_MATCH_2} maps")
	endif()
	if(log MATCHES "Geo data is disabled" OR geo_loaded STREQUAL "")
		fail("${test_name}: the run did not load the geo data (-Dgameserver.geodata.enable=${geodata_enable}), so it would prove nothing about "
			"the zone handlers, canSee or the terrain z of the spawns")
	endif()
	message("${test_name}: geo data loaded: ${geo_loaded}")

	# 4. failed spawns. VisibleObjectSpawner logs "Error during spawn" and World "did not leave world cleanly" for each one (F-1: 13 pairs,
	# 7 in Verteron and 6 in Reshanta). Both are ERROR lines, so check 2 would catch them too - this check names them.
	file(STRINGS "${OUTPUT_DIR}/log/server_console.log" spawn_failures REGEX "Error during spawn|did not leave world cleanly")
	list(LENGTH spawn_failures count)
	if(NOT count EQUAL 0)
		first_entries("${spawn_failures}" 20 lines)
		fail("${test_name}: ${count} spawn failure lines in ${OUTPUT_DIR}/log/server_console.log (m5a-client-session.md F-1):\n" "${lines}")
	endif()
	if(NOT log MATCHES "Loaded ([0-9]+) npc spawns")
		fail("${test_name}: the startup logged no \"Loaded N npc spawns\" line, so no npc was spawned and no zone handler ran")
	endif()
	set(npc_spawns "${CMAKE_MATCH_1}")
	# The reference runs load 83,885 npc spawns under this profile (83,872 before the SiegeShield fix restored 13, m5a-client-session.md).
	# The floor sits just under that: 40 per cent lower would let a silent loss of thousands of spawns pass. A static data change that moves
	# the number legitimately moves this line too.
	if(npc_spawns LESS 83000)
		fail("${test_name}: only ${npc_spawns} npc spawns were loaded (the reference run of this profile loads 83,885), so this run covers next "
			"to no zone handler")
	endif()

	# 2. ERROR entries in the run's own --log-folder, and what each file of that folder can be scanned for at all. Every appender of
	# game-server/config/logback.xml has its own pattern, and ONLY app_console carries the level word ("${date} %-5level [%thread] %logger -
	# %message"). app_error and app_warn write "${date} %logger - %message", and so does every audit log of an additivity="false" logger - so
	# `REGEX " ERROR "`, which this loop used for every file except server_errors.log, CAN ONLY EVER RETURN 0 on them, whatever the run logged
	# (stage 3 wave B, low finding of the wave A review: a "zero errors, zero warnings" evidence line came out of exactly such a grep; the wave A
	# geo run's server_warnings.log has 91 warning entries and 0 lines containing " ERROR "). The files are therefore classified by what their
	# lines look like, not by their name:
	#   - a file that carries the level word (server_console.log) is scanned line by line for " ERROR ";
	#   - a level-less file is grouped into EVENTS - one error is a header line plus the exception and its stack, so a new event starts at a
	#     timestamped line - and judged by the appender that owns it: every event of server_errors.log is an ERROR record (its LevelFilter takes
	#     ERROR and nothing else) and every event of server_warnings.log is a WARN record (LevelFilter WARN), which is counted and printed but
	#     never fails a run - a 4.8 startup legitimately warns about missing trade lists and lenient enums;
	#   - any other level-less file (chat.log, item.log, ... of the additivity="false" loggers) carries no level at all, so this check cannot
	#     judge it: it is named in the output instead of being scanned vacuously.
	# The captured console stream is scanned too, because those additivity="false" loggers never reach it while it never reaches the log folder
	# (m5a-plan.md §5.7 Q8 reads both for the same reason).
	#
	# AND THE SCAN HAS TO BE ABLE TO FAIL: if nothing in the folder carried the level word, the " ERROR " scan found nothing because it could not
	# match anything, and printing "0 ERROR entries" would be the vacuous evidence this check exists to prevent. Both canaries below - the log
	# folder and the console stream - fail the run in that case, which is what a changed logback pattern looks like from here.
	#
	# One error event is allowed, and it is this harness's own doing: the login link points at a closed port (${login_address}), so LoginServer's
	# reconnect task can never succeed, and when a retry lands after the ShutdownHook stopped the NioServer, openSocket throws "NioServer is not
	# running" and ExecuteWrapper logs it with a stack. The real login link is covered by gs.scenario.m5a, which runs a real login server.
	string(REPLACE "." "\\." allowed_error_address "${login_address}")
	# The call site alone is too wide a key: "Exception in a Runnable execution: scheduled task LoginServer.cpp:58" reads the same whatever
	# the exception is, so a real bug in the reconnect task would be allowed through. An EVENT of server_errors.log carries the exception
	# too, and is only allowed when its body is the one known exception; the single-line logs keep the site key, because the exception text
	# is not on the same line there. (Wave A's loop then passed the events it had already classified through the site key a second time, which
	# threw away exactly the events the body check had kept: an event at that site with a different exception was reported by nothing.)
	set(allowed_error "scheduled task LoginServer\\.cpp|Cannot connect to ${allowed_error_address}")
	set(allowed_error_body "NioServer is not running")
	set(levelled_line "^[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]T[^ ]* (TRACE|DEBUG|INFO|WARN|ERROR) ")
	set(error_lines "")
	set(warning_events 0)
	set(levelled_logs "")
	set(event_logs "")
	set(unscanned_logs "")
	set(folder_log_names "")
	file(GLOB folder_logs "${OUTPUT_DIR}/log/*.log")
	foreach(file IN LISTS folder_logs)
		get_filename_component(name "${file}" NAME)
		list(APPEND folder_log_names "${name}")
		file(STRINGS "${file}" all_lines)
		set(carries_level FALSE)
		foreach(line IN LISTS all_lines)
			if(line MATCHES "${levelled_line}")
				set(carries_level TRUE)
				break()
			endif()
		endforeach()
		# The NAME decides first, not the content: server_errors.log and server_warnings.log are written with an appender pattern that carries
		# no level word, so scanning them for " ERROR " can never match. One embedded line that looks like a console line used to flip the whole
		# file into the line branch, and a real error event then passed silently - the vacuous grep this check exists to remove.
		if(name STREQUAL "server_errors.log" OR name STREQUAL "server_warnings.log")
			# group the file into events; ";" would split the diagnostics into list elements, so it is replaced
			list(APPEND event_logs "${name}")
			set(event "")
			set(events "")
			foreach(line IN LISTS all_lines)
				string(REPLACE ";" "," line "${line}")
				if(line MATCHES "^[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]T" AND NOT event STREQUAL "")
					list(APPEND events "${event}")
					set(event "")
				endif()
				string(APPEND event " ${line}")
			endforeach()
			if(NOT event STREQUAL "")
				list(APPEND events "${event}")
			endif()
			if(name STREQUAL "server_warnings.log")
				list(LENGTH events events_count)
				math(EXPR warning_events "${warning_events} + ${events_count}")
			else()
				# the whole event decides whether it is the allowed one; only the diagnostics are shortened
				foreach(entry IN LISTS events)
					if(NOT (entry MATCHES "${allowed_error}" AND entry MATCHES "${allowed_error_body}"))
						string(LENGTH "${entry}" entry_length)
						if(entry_length GREATER 400)
							string(SUBSTRING "${entry}" 0 400 entry)
							string(APPEND entry " ...")
						endif()
						list(APPEND error_lines "${name}: ${entry}")
					endif()
				endforeach()
			endif()
		elseif(carries_level)
			list(APPEND levelled_logs "${name}")
			file(STRINGS "${file}" lines REGEX " ERROR ")
			foreach(line IN LISTS lines)
				if(NOT line MATCHES "${allowed_error}")
					list(APPEND error_lines "${name}: ${line}")
				endif()
			endforeach()
		elseif(NOT all_lines STREQUAL "")
			list(APPEND unscanned_logs "${name}")
		endif()
	endforeach()
	if(NOT levelled_logs)
		string(REPLACE ";" ", " folder_log_names "${folder_log_names}")
		fail("${test_name}: no file in ${OUTPUT_DIR}/log carries the log level in its lines (${folder_log_names}), so this check's \" ERROR \" "
			"scan could not have matched anything and its result means nothing. Check the app_console pattern of "
			"${WORKING_DIRECTORY}/config/logback.xml.")
	endif()
	file(STRINGS "${OUTPUT_DIR}/server.log" console_levelled REGEX "^[0-9:]+ (TRACE|DEBUG|INFO|WARN|ERROR) ")
	if(NOT console_levelled)
		fail("${test_name}: no line of ${OUTPUT_DIR}/server.log carries the log level, so the ERROR scan of the captured console stream could "
			"not have matched anything. Check the out_console pattern of ${WORKING_DIRECTORY}/config/logback.xml.")
	endif()
	file(STRINGS "${OUTPUT_DIR}/server.log" console_errors REGEX "^[0-9:]+ ERROR ")
	foreach(line IN LISTS console_errors)
		if(NOT line MATCHES "${allowed_error}")
			list(APPEND error_lines "server.log: ${line}")
		endif()
	endforeach()
	list(LENGTH error_lines count)
	if(NOT count EQUAL 0)
		first_entries("${error_lines}" 20 lines)
		fail("${test_name}: ${count} ERROR entries (events of server_errors.log, lines of the level-carrying logs) in ${OUTPUT_DIR}/log and the "
			"console stream:\n" "${lines}")
	endif()
	string(REPLACE ";" ", " levelled_logs "${levelled_logs}")
	string(REPLACE ";" ", " event_logs "${event_logs}")
	set(unscanned_note "")
	if(unscanned_logs)
		string(REPLACE ";" ", " unscanned_logs "${unscanned_logs}")
		set(unscanned_note "; NOT scanned for errors (their appender pattern carries no level): ${unscanned_logs}")
	endif()
	message("${test_name}: geo startup: ${npc_spawns} npc spawns, 0 spawn failures, 0 unexpected ERROR entries in ${OUTPUT_DIR}/log and the "
		"console stream (scanned by level: ${levelled_logs}, server.log; as events: ${event_logs}; ${warning_events} warning events in "
		"server_warnings.log)${unscanned_note}")
endif()

if(started AND result STREQUAL "0")
	if(NOT log MATCHES "Stop file [^\r\n]* found, shutting down")
		fail("${test_name}: the server exited without reading the stop file")
	endif()
	if(NOT orderly)
		fail("${test_name}: the server started, but the runtime did not report its shutdown")
	endif()
	drop_schema()
	message(STATUS "${test_name}: passed (the whole startup ran; stop file -> ShutdownHook -> orderly shutdown; last step ${last_step})")
	return()
endif()

if(started)
	fail("${test_name}: the server started but exited with code '${result}' (see the log above)")
endif()
if(NOT result STREQUAL "1" OR stopped_at STREQUAL "")
	fail("${test_name}: the startup did not complete (exit code '${result}', last step ${last_step}; see the log above)")
endif()
if(NOT orderly AND log MATCHES "startup step [0-9]+: ThreadPoolManager, CronService, IDFactory")
	fail("${test_name}: the startup stopped at ${stopped_at}, but the runtime did not report its shutdown")
endif()

if(MODE STREQUAL "progress")
	drop_schema()
	message(STATUS "${test_name}: passed (the startup reached step ${last_step} and stopped at ${stopped_at}; exit code 1, orderly shutdown)")
	return()
endif()

# MODE smoke and geo
if(NOT log MATCHES "Static Data loaded in [0-9.,]+ seconds" OR NOT log MATCHES "World: 161 world maps created\\.")
	fail("${test_name}: the startup stopped at ${stopped_at} before the M4 path (static data, 161 world maps) completed")
endif()
if(REQUIRE_STARTED)
	fail("${test_name}: the startup stopped at step ${last_step}: ${stopped_at}")
endif()
drop_schema()
message("${test_name}: skipped (wave 5a stage 1: the M4 path passed, the startup stopped at step ${last_step}: ${stopped_at}; F-01b makes the "
	"whole startup required)")
