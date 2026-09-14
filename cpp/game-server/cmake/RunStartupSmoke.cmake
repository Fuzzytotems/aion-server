# CTest gs.smoke.startup (spine step S0a link proof): runs aion_game_server in the Java game-server directory and expects the startup to reach
# the unported DataManager: its AION_UNPORTED message, then an orderly shutdown and a non-zero exit code.
#   cmake -DEXECUTABLE=<aion_game_server> -DWORKING_DIRECTORY=<game-server> -P RunStartupSmoke.cmake
# Database (like the commons and login server database tests, the test is skipped without it): the environment variable
# AION_TEST_GS_DATABASE_URL (e.g. jdbc:mysql://127.0.0.1:3306/aion_cpp_test?characterEncoding=UTF-8; the database must exist) and optionally
# AION_TEST_GS_DATABASE_USER / AION_TEST_GS_DATABASE_PASSWORD (default: root without password) are passed as -Ddatabase.* overrides, so the test
# never uses the database of config/network/database.properties. Output "gs.smoke.startup: skipped" marks the test skipped
# (SKIP_REGULAR_EXPRESSION). The server writes its log files to <WORKING_DIRECTORY>/log (gitignored), like a normal start.

if(NOT DEFINED ENV{AION_TEST_GS_DATABASE_URL} OR "$ENV{AION_TEST_GS_DATABASE_URL}" STREQUAL "")
	message("gs.smoke.startup: skipped (set AION_TEST_GS_DATABASE_URL, e.g. jdbc:mysql://127.0.0.1:3306/aion_cpp_test?characterEncoding=UTF-8)")
	return()
endif()
if(NOT EXISTS "${EXECUTABLE}")
	message(FATAL_ERROR "gs.smoke.startup: ${EXECUTABLE} does not exist (build aion_game_server first)")
endif()

set(arguments "-Ddatabase.url=$ENV{AION_TEST_GS_DATABASE_URL}")
if(DEFINED ENV{AION_TEST_GS_DATABASE_USER})
	list(APPEND arguments "-Ddatabase.user=$ENV{AION_TEST_GS_DATABASE_USER}")
else()
	list(APPEND arguments "-Ddatabase.user=root")
endif()
list(APPEND arguments "-Ddatabase.password=$ENV{AION_TEST_GS_DATABASE_PASSWORD}")

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

if(log MATCHES "AION_UNPORTED reached: [^\n]*DataManager::getInstance" AND log MATCHES "startup stopped at an unported function: [^\n]*DataManager")
	if(result EQUAL 0 OR NOT result MATCHES "^[0-9]+$")
		message(FATAL_ERROR "gs.smoke.startup: reached the unported DataManager, but the exit code is '${result}' (expected a non-zero exit code)")
	endif()
	if(NOT log MATCHES "Runtime shut down:")
		message(FATAL_ERROR "gs.smoke.startup: reached the unported DataManager, but the runtime did not report its shutdown")
	endif()
	message(STATUS "gs.smoke.startup: passed (Config -> database -> runtime -> unported DataManager -> orderly shutdown)")
elseif(result MATCHES "timeout|Process terminated due to timeout")
	message(FATAL_ERROR "gs.smoke.startup: aion_game_server did not exit within 150 seconds")
elseif(log MATCHES "DatabaseFactory|[Cc]onnect|MariaDB|SQLException|database")
	message(FATAL_ERROR "gs.smoke.startup: the game server could not use the database $ENV{AION_TEST_GS_DATABASE_URL}. Start MariaDB, create "
		"the database or fix AION_TEST_GS_DATABASE_URL, and run the test again.")
else()
	message(FATAL_ERROR "gs.smoke.startup: the startup did not reach the unported DataManager (see the log above)")
endif()
