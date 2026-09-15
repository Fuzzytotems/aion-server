# CTest gs.smoke.startup (spine step S0a link proof, milestone M4 startup path): runs aion_game_server in the Java game-server directory and expects
# the ported startup (Config -> database -> setAllPlayersOffline -> runtime with IDFactory -> DataManager, all static data loaded -> ZoneService ->
# GeoService -> World) to complete: "##### [Static Data loaded in N seconds] #####", "World: 161 world maps created.", "M4 startup sequence
# complete", an orderly runtime shutdown and exit code 0. A startup that stops at an unported function fails and names it. The geo data is
# switched off (-Dgameserver.geodata.enable=false, GeoService logs "Geo data is disabled"): loading data/geo takes minutes in a Debug build and is
# covered by gs.m4.check_static_data.
#   cmake -DEXECUTABLE=<aion_game_server> -DDATABASE_TOOL=<aion_gs_m4_database> -DWORKING_DIRECTORY=<game-server> -P RunStartupSmoke.cmake
# Database (like the commons and login server database tests, the test is skipped without it): the environment variable
# AION_TEST_GS_DATABASE_URL names the MariaDB server (e.g. jdbc:mysql://127.0.0.1:3306/aion_cpp_test?characterEncoding=UTF-8; that database must
# exist), optionally AION_TEST_GS_DATABASE_USER / AION_TEST_GS_DATABASE_PASSWORD (default: root without password). Since the startup runs
# PlayerDAO.setAllPlayersOffline and the IDFactory queries, the test creates its own game server schema aion_gs_test_smoke from
# game-server/sql/aion_gs.sql with aion_gs_m4_database, passes it as -Ddatabase.* overrides (never the database of
# config/network/database.properties) and drops it after a successful run. Output "gs.smoke.startup: skipped" marks the test skipped
# (SKIP_REGULAR_EXPRESSION). The server writes its log files to <WORKING_DIRECTORY>/log (gitignored), like a normal start.

if(NOT DEFINED ENV{AION_TEST_GS_DATABASE_URL} OR "$ENV{AION_TEST_GS_DATABASE_URL}" STREQUAL "")
	message("gs.smoke.startup: skipped (set AION_TEST_GS_DATABASE_URL, e.g. jdbc:mysql://127.0.0.1:3306/aion_cpp_test?characterEncoding=UTF-8)")
	return()
endif()
foreach(file IN ITEMS "${EXECUTABLE}" "${DATABASE_TOOL}")
	if(NOT EXISTS "${file}")
		message(FATAL_ERROR "gs.smoke.startup: ${file} does not exist (build aion_game_server and aion_gs_m4_database first)")
	endif()
endforeach()

set(database aion_gs_test_smoke)
set(url "$ENV{AION_TEST_GS_DATABASE_URL}")
if(NOT url MATCHES "^(jdbc:[A-Za-z]+://[^/?]+)(/[^?]*)?(\\?.*)?$")
	message(FATAL_ERROR "gs.smoke.startup: cannot parse AION_TEST_GS_DATABASE_URL '${url}'")
endif()
set(schema_url "${CMAKE_MATCH_1}/${database}${CMAKE_MATCH_3}")
execute_process(COMMAND "${DATABASE_TOOL}" create ${database} OUTPUT_VARIABLE created ERROR_VARIABLE create_errors RESULT_VARIABLE create_result
	TIMEOUT 300)
if(NOT create_result STREQUAL "0")
	message(FATAL_ERROR "gs.smoke.startup: could not create ${database} (${create_errors}). Start MariaDB or fix AION_TEST_GS_DATABASE_URL, and run "
		"the test again.")
endif()
message("${created}")

set(arguments "-Ddatabase.url=${schema_url}")
if(DEFINED ENV{AION_TEST_GS_DATABASE_USER})
	list(APPEND arguments "-Ddatabase.user=$ENV{AION_TEST_GS_DATABASE_USER}")
else()
	list(APPEND arguments "-Ddatabase.user=root")
endif()
list(APPEND arguments "-Ddatabase.password=$ENV{AION_TEST_GS_DATABASE_PASSWORD}")
list(APPEND arguments "-Dgameserver.geodata.enable=false")

execute_process(
	COMMAND "${EXECUTABLE}" ${arguments}
	WORKING_DIRECTORY "${WORKING_DIRECTORY}"
	OUTPUT_VARIABLE output
	ERROR_VARIABLE errors
	RESULT_VARIABLE result
	TIMEOUT 150
)
set(log "${output}${errors}")
message("${log}")
message("aion_game_server exit code: ${result}")

if(result STREQUAL "0" AND log MATCHES "Static Data loaded in [0-9.,]+ seconds" AND log MATCHES "World: 161 world maps created\\."
	AND log MATCHES "M4 startup sequence complete")
	if(NOT log MATCHES "Runtime shut down:")
		message(FATAL_ERROR "gs.smoke.startup: the startup completed, but the runtime did not report its shutdown")
	endif()
	execute_process(COMMAND "${DATABASE_TOOL}" drop ${database} OUTPUT_QUIET ERROR_QUIET TIMEOUT 120)
	message(STATUS "gs.smoke.startup: passed (Config -> database -> runtime -> static data -> zones -> world -> orderly shutdown)")
elseif(result MATCHES "timeout|Process terminated due to timeout")
	message(FATAL_ERROR "gs.smoke.startup: aion_game_server did not exit within 150 seconds")
elseif(log MATCHES "startup stopped at an unported function: ([^\n]*)")
	message(FATAL_ERROR "gs.smoke.startup: the startup stopped at an unported function: ${CMAKE_MATCH_1}")
elseif(log MATCHES "Failed to initialize pool|DatabaseFactory is not initialized|SQLException")
	message(FATAL_ERROR "gs.smoke.startup: the game server could not use the database ${schema_url}. Start MariaDB or fix "
		"AION_TEST_GS_DATABASE_URL, and run the test again.")
else()
	message(FATAL_ERROR "gs.smoke.startup: the startup did not complete (exit code '${result}', see the log above)")
endif()
