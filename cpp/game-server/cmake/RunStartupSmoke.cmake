# CTests gs.smoke.startup and gs.smoke.startup_progress (spine step S0a link proof, milestone M4 startup path, m5a-plan.md F-01a/F-02): runs
# aion_game_server in the Java game-server directory with the M5a profile (D1) and a stop file, and checks the startup in Java order (Config ->
# database -> runtime with IDFactory -> DataManager -> engines -> World -> services -> spawns -> ... -> "Game server started" -> LoginServer
# connect) and the shutdown. The geo data is switched off (-Dgameserver.geodata.enable=false): loading data/geo takes minutes in a Debug build
# and is covered by gs.m4.check_static_data. The client socket binds an ephemeral port (127.0.0.1:0) and the login server link points to a
# closed port (127.0.0.1:1), so a game or login server running on this machine is never touched.
#
# The stop file (--stop-file) is written before the server starts: the server checks it after "Game server started" and then shuts down like on
# Ctrl+C (with nobody online the ShutdownHook takes its fast exit). The plan (F-02) writes it after "Game server started"; writing it up front is
# equivalent because the run loop is only entered once the whole startup finished, and the run is proved to have consumed it by the log line
# "Stop file ... found, shutting down" (docs/deviations/P5-14.md, row "Run mode").
#
# Both modes check the --check-output reports of the run (m5a-plan.md F-02, F-07, D8): the seven report files exist, m5a_summary.txt agrees with
# whether the server started, and a started run really ran the final census. This is the only exercise of that path in a real server process.
#
#   cmake -DEXECUTABLE=<aion_game_server> -DDATABASE_TOOL=<aion_gs_m4_database> -DWORKING_DIRECTORY=<game-server> -DMODE=smoke|progress
#         [-DREQUIRE_STARTED=ON|OFF] [-DOUTPUT_DIR=<dir>] -P RunStartupSmoke.cmake
#
# MODE smoke (gs.smoke.startup): passes when the whole startup ran ("Game server started") with zero AION_UNPORTED hits (m5a-plan.md F-01b; the
# AION_PARTIAL sites it reached are printed), the stop file shut the server down in order ("Runtime shut down: ...") and the exit code is 0. With
# REQUIRE_STARTED OFF (stage 1 of wave 5a, before F-01b), a startup that loaded all static data and created the world but then stopped at an
# unported function was reported as skipped (it names the function); stage 2 sets REQUIRE_STARTED ON (cmake/AppTests.cmake).
# MODE progress (gs.smoke.startup_progress): prints the last startup step reached ("startup step N: name", m5a-plan.md F-01a) and passes when
# the steps are numbered without gaps from 1 and the run ended in order: either started and stopped by the stop file with exit code 0, or
# stopped at a step (an unported function or an exception) with exit code 1 and an orderly runtime shutdown. A crash, a hang, a missing step
# log or a missing runtime shutdown fail.
#
# Database (like the commons and login server database tests, the tests are skipped without it): the environment variable
# AION_TEST_GS_DATABASE_URL names the MariaDB server (e.g. jdbc:mysql://127.0.0.1:3306/aion_cpp_test?characterEncoding=UTF-8; that database must
# exist), optionally AION_TEST_GS_DATABASE_USER / AION_TEST_GS_DATABASE_PASSWORD (default: root without password). The test creates its own game
# server schema (aion_gs_test_smoke_<hash>, aion_gs_test_smoke_progress_<hash>, the first 12 hex digits of the MD5 of OUTPUT_DIR like
# RunM4Check.cmake) from game-server/sql/aion_gs.sql with aion_gs_m4_database, passes it as -Ddatabase.* overrides (never the database of
# config/network/database.properties) and drops it after a successful run. Output "gs.smoke.startup: skipped" /
# "gs.smoke.startup_progress: skipped" marks a test skipped (SKIP_REGULAR_EXPRESSION).
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
elseif(MODE STREQUAL "progress")
	set(test_name gs.smoke.startup_progress)
	set(database_prefix aion_gs_test_smoke_progress)
else()
	message(FATAL_ERROR "RunStartupSmoke.cmake: unknown MODE '${MODE}' (smoke or progress)")
endif()
if(NOT DEFINED REQUIRE_STARTED)
	set(REQUIRE_STARTED ON)
endif()

if(NOT DEFINED OUTPUT_DIR)
	set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/${test_name}")
endif()
# The schema name carries the hash of OUTPUT_DIR (like RunM4Check.cmake) and the log directory is OUTPUT_DIR/log, so two build directories
# running this test at the same time neither drop each other's schema nor archive each other's log files (Logging::init deletes what it archives).
string(MD5 output_hash "${OUTPUT_DIR}")
string(SUBSTRING "${output_hash}" 0 12 output_hash)
set(database "${database_prefix}_${output_hash}")

if(NOT DEFINED ENV{AION_TEST_GS_DATABASE_URL} OR "$ENV{AION_TEST_GS_DATABASE_URL}" STREQUAL "")
	message("${test_name}: skipped (set AION_TEST_GS_DATABASE_URL, e.g. jdbc:mysql://127.0.0.1:3306/aion_cpp_test?characterEncoding=UTF-8)")
	return()
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
execute_process(COMMAND "${DATABASE_TOOL}" create ${database} OUTPUT_VARIABLE created ERROR_VARIABLE create_errors RESULT_VARIABLE create_result
	TIMEOUT 300)
if(NOT create_result STREQUAL "0")
	message(FATAL_ERROR "${test_name}: could not create ${database} (${create_errors}). Start MariaDB or fix AION_TEST_GS_DATABASE_URL, and run "
		"the test again.")
endif()
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
	"-Dgameserver.limits.enable=false" "-Dgameserver.event.service.disabled_events=*" "-Dgameserver.geodata.enable=false"
	"-Dgameserver.shutdown.delay=2")
list(APPEND arguments "-Dgameserver.network.client.socket_address=127.0.0.1:0" "-Dgameserver.network.login.address=127.0.0.1:1")
list(APPEND arguments "--stop-file=${stop_file}" "--check-output=${OUTPUT_DIR}/check" "--log-folder=${OUTPUT_DIR}/log")

execute_process(
	COMMAND "${EXECUTABLE}" ${arguments}
	WORKING_DIRECTORY "${WORKING_DIRECTORY}"
	OUTPUT_FILE "${OUTPUT_DIR}/server.log"
	ERROR_FILE "${OUTPUT_DIR}/server.stderr.log"
	RESULT_VARIABLE result
	TIMEOUT 540
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
	message(FATAL_ERROR "${test_name}: aion_game_server did not exit within 540 seconds")
endif()
if(log MATCHES "Failed to initialize pool|DatabaseFactory is not initialized|SQLException")
	if(NOT log MATCHES "startup step [0-9]+: DataManager")
		message(FATAL_ERROR "${test_name}: the game server could not use the database ${schema_url}. Start MariaDB or fix "
			"AION_TEST_GS_DATABASE_URL, and run the test again.")
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
		message(FATAL_ERROR "${test_name}: startup step ${CMAKE_MATCH_1} logged where step ${expected} was expected (${line})")
	endif()
	set(last_step "${CMAKE_MATCH_1}: ${CMAKE_MATCH_2}")
	math(EXPR expected "${expected} + 1")
endforeach()
if(step_count EQUAL 0)
	message(FATAL_ERROR "${test_name}: no startup step was logged (exit code '${result}')")
endif()
# --log-folder: the run's log files are its own, so no other server process archives or deletes them (and this one archives none of theirs)
if(NOT EXISTS "${OUTPUT_DIR}/log/server_console.log")
	message(FATAL_ERROR "${test_name}: the server wrote no ${OUTPUT_DIR}/log/server_console.log (--log-folder was not honoured)")
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
		message(FATAL_ERROR "${test_name}: --check-output wrote no ${report} in ${check_dir} (started ${started}; m5a-plan.md F-02/F-07)")
	endif()
endforeach()
file(READ "${check_dir}/m5a_summary.txt" summary)
if(started)
	if(NOT summary MATCHES "started true")
		message(FATAL_ERROR "${test_name}: m5a_summary.txt does not say \"started true\":\n${summary}")
	endif()
	# F-07: the final census ran in a real process (the file always carries its header line, and the hits are counted)
	file(READ "${check_dir}/census.txt" census)
	if(NOT census MATCHES "# final census v1")
		message(FATAL_ERROR "${test_name}: census.txt has no final census header:\n${census}")
	endif()
	if(NOT log MATCHES "Final census: [0-9]+ leaks written to")
		message(FATAL_ERROR "${test_name}: the final census did not run before the runtime shutdown (F-07)")
	endif()
	file(READ "${check_dir}/live_counts.txt" live_counts)
	string(LENGTH "${live_counts}" live_counts_length)
	if(live_counts_length EQUAL 0)
		message(FATAL_ERROR "${test_name}: live_counts.txt is empty")
	endif()
	# F-01b: the whole startup path must reach no AION_UNPORTED body at all. The counter covers every thread (the spawn and zone paths swallow
	# the UnportedException, so a body reached there would otherwise only show up as an ERROR line).
	if(NOT summary MATCHES "unportedHits ([0-9]+)")
		message(FATAL_ERROR "${test_name}: m5a_summary.txt has no \"unportedHits\" line:\n${summary}")
	endif()
	set(unported_hits "${CMAKE_MATCH_1}")
	if(NOT unported_hits STREQUAL "0")
		file(READ "${check_dir}/unported_trace.txt" unported_trace)
		if(MODE STREQUAL "smoke")
			message(FATAL_ERROR "${test_name}: the startup reached ${unported_hits} AION_UNPORTED hits; m5a-plan.md F-01b requires none:\n"
				"${unported_trace}")
		endif()
		# progress mode stays the diagnostic of F-01a: it names what the startup still reaches instead of failing
		message("${test_name}: the startup reached ${unported_hits} AION_UNPORTED hits (gs.smoke.startup fails on them, F-01b):\n${unported_trace}")
	endif()
	# The AION_PARTIAL sites of the startup are allowed (D3), but the gate (F-06) has to know them: print them with their hit counts.
	file(READ "${check_dir}/partial_trace.txt" partial_trace)
	message("${test_name}: AION_PARTIAL sites reached by the startup (m5a-plan.md D3):\n${partial_trace}")
elseif(NOT summary MATCHES "started false")
	message(FATAL_ERROR "${test_name}: m5a_summary.txt does not say \"started false\" although the server did not start:\n${summary}")
endif()
message("${test_name}: check output in ${check_dir}:\n${summary}")

if(started AND result STREQUAL "0")
	if(NOT log MATCHES "Stop file [^\r\n]* found, shutting down")
		message(FATAL_ERROR "${test_name}: the server exited without reading the stop file")
	endif()
	if(NOT orderly)
		message(FATAL_ERROR "${test_name}: the server started, but the runtime did not report its shutdown")
	endif()
	execute_process(COMMAND "${DATABASE_TOOL}" drop ${database} OUTPUT_QUIET ERROR_QUIET TIMEOUT 120)
	message(STATUS "${test_name}: passed (the whole startup ran; stop file -> ShutdownHook -> orderly shutdown; last step ${last_step})")
	return()
endif()

if(started)
	message(FATAL_ERROR "${test_name}: the server started but exited with code '${result}' (see the log above)")
endif()
if(NOT result STREQUAL "1" OR stopped_at STREQUAL "")
	message(FATAL_ERROR "${test_name}: the startup did not complete (exit code '${result}', last step ${last_step}; see the log above)")
endif()
if(NOT orderly AND log MATCHES "startup step [0-9]+: ThreadPoolManager, CronService, IDFactory")
	message(FATAL_ERROR "${test_name}: the startup stopped at ${stopped_at}, but the runtime did not report its shutdown")
endif()

if(MODE STREQUAL "progress")
	execute_process(COMMAND "${DATABASE_TOOL}" drop ${database} OUTPUT_QUIET ERROR_QUIET TIMEOUT 120)
	message(STATUS "${test_name}: passed (the startup reached step ${last_step} and stopped at ${stopped_at}; exit code 1, orderly shutdown)")
	return()
endif()

# MODE smoke
if(NOT log MATCHES "Static Data loaded in [0-9.,]+ seconds" OR NOT log MATCHES "World: 161 world maps created\\.")
	message(FATAL_ERROR "${test_name}: the startup stopped at ${stopped_at} before the M4 path (static data, 161 world maps) completed")
endif()
if(REQUIRE_STARTED)
	message(FATAL_ERROR "${test_name}: the startup stopped at step ${last_step}: ${stopped_at}")
endif()
execute_process(COMMAND "${DATABASE_TOOL}" drop ${database} OUTPUT_QUIET ERROR_QUIET TIMEOUT 120)
message("${test_name}: skipped (wave 5a stage 1: the M4 path passed, the startup stopped at step ${last_step}: ${stopped_at}; F-01b makes the "
	"whole startup required)")
