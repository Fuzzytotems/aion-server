# CTest gs.m4.check_static_data: the M4 gate (docs/design/handlers-and-porting-plan.md §2.7), "all static data loads".
#   cmake -DEXECUTABLE=<aion_game_server> -DDATABASE_TOOL=<aion_gs_m4_database> -DWORKING_DIRECTORY=<game-server> -DPYTHON=<python 3.12>
#         -DORACLE_DIR=<cpp/tools/oracle> -DOUTPUT_DIR=<dir> -P RunM4Check.cmake
# Steps (each failure names the item):
#  1. aion_gs_m4_database create aion_gs_test_m4_<hash>: a fresh schema from game-server/sql/aion_gs.sql.
#  2. aion_game_server --check-id-factory on the empty schema: Config.load, DatabaseFactory, setAllPlayersOffline, runtime start; IDFactory
#     locks only id 0 ("IDFactory: 1 IDs used.").
#  3. aion_gs_m4_database fixture: 2 online players and 9 more rows with object ids (11 distinct ids).
#  4. aion_game_server --check-static-data (with the 200 getZ probes of python -m geo m4-probes): the whole M4 path (Config, database,
#     setAllPlayersOffline, runtime, IDFactory "12 IDs used.", DataManager strict load with hooks and post-processing, ZoneService, GeoService,
#     World) and an orderly shutdown with exit code 0, no ERROR line, no unknown config property, 0 AION_UNPORTED hits (unported_trace.txt
#     lists no site), LeakCensus 0 objects still tracked (trivially true for M4, which never removes an object from the world; the check is a
#     guard for M5a).
#  5. aion_gs_m4_database online: setAllPlayersOffline left no online player.
#  6. python oracle.py compare-counts --log static_data_counts.txt (the 90 "Loaded N ..." lines vs the independent XML count oracle).
#  7. python -m geo m4-compare: geo counts, material zone names, getZ probes (geo oracle) and the zone names of every map instance of World.
# Python: PYTHON is the interpreter for the oracles; PYTHON=NOTFOUND (CMake found no Python 3) reports the test as skipped with that reason
# instead of leaving it unregistered.
# Database: AION_TEST_GS_DATABASE_URL names the server (its database must exist; the schema is created next to it), optional
# AION_TEST_GS_DATABASE_USER / AION_TEST_GS_DATABASE_PASSWORD (default root without password). Without the URL the test is skipped
# ("gs.m4.check_static_data: skipped", SKIP_REGULAR_EXPRESSION). The schema is aion_gs_test_m4_<first 12 hex digits of the MD5 of OUTPUT_DIR>,
# so M4 runs of different build directories or configurations on one MariaDB never share (and drop) each other's schema; RESOURCE_LOCK only
# serializes the tests of one ctest invocation. It is dropped at the end of the run, after a failure too (the output of every database step
# and both server logs stay in OUTPUT_DIR). The server logs are OUTPUT_DIR/id_factory.log and OUTPUT_DIR/check.log, the report files
# OUTPUT_DIR/empty and OUTPUT_DIR/full. Each server run gets its own log directory (--log-folder=OUTPUT_DIR/<name>_log) instead of the shared
# <WORKING_DIRECTORY>/log, so a game server of another build directory cannot make the log archiving of this run fail.

if(NOT PYTHON OR NOT EXISTS "${PYTHON}")
	message("gs.m4.check_static_data: skipped (no Python 3 interpreter for the oracles: PYTHON='${PYTHON}'; install Python 3.12 and reconfigure)")
	return()
endif()
if(NOT DEFINED ENV{AION_TEST_GS_DATABASE_URL} OR "$ENV{AION_TEST_GS_DATABASE_URL}" STREQUAL "")
	message("gs.m4.check_static_data: skipped (set AION_TEST_GS_DATABASE_URL, e.g. jdbc:mysql://127.0.0.1:3306/aion_cpp_test?characterEncoding=UTF-8)")
	return()
endif()
foreach(file IN ITEMS "${EXECUTABLE}" "${DATABASE_TOOL}")
	if(NOT EXISTS "${file}")
		message(FATAL_ERROR "gs.m4.check_static_data: ${file} does not exist (build it first)")
	endif()
endforeach()

string(MD5 output_hash "${OUTPUT_DIR}")
string(SUBSTRING "${output_hash}" 0 12 output_hash)
set(database "aion_gs_test_m4_${output_hash}")
set(url "$ENV{AION_TEST_GS_DATABASE_URL}")
if(NOT url MATCHES "^(jdbc:[A-Za-z]+://[^/?]+)(/[^?]*)?(\\?.*)?$")
	message(FATAL_ERROR "gs.m4.check_static_data: cannot parse AION_TEST_GS_DATABASE_URL '${url}'")
endif()
set(schema_url "${CMAKE_MATCH_1}/${database}${CMAKE_MATCH_3}")
set(user "root")
if(DEFINED ENV{AION_TEST_GS_DATABASE_USER})
	set(user "$ENV{AION_TEST_GS_DATABASE_USER}")
endif()
set(database_arguments "-Ddatabase.url=${schema_url}" "-Ddatabase.user=${user}" "-Ddatabase.password=$ENV{AION_TEST_GS_DATABASE_PASSWORD}")

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

function(m4_fail item text)
	get_property(created GLOBAL PROPERTY m4_database_created)
	if(created)
		# best effort, without checks (m4_database fails through this function)
		execute_process(COMMAND "${DATABASE_TOOL}" drop ${database} OUTPUT_VARIABLE output ERROR_VARIABLE errors RESULT_VARIABLE result TIMEOUT 300)
		message("aion_gs_m4_database drop after the failure (${result}): ${output}${errors}")
	endif()
	message(FATAL_ERROR "gs.m4.check_static_data: ${item}: ${text}")
endfunction()

function(m4_database command expected_output)
	if(command STREQUAL "create")
		set_property(GLOBAL PROPERTY m4_database_created TRUE) # a failed create may have left a partial schema
	endif()
	execute_process(COMMAND "${DATABASE_TOOL}" ${command} ${database} OUTPUT_VARIABLE output ERROR_VARIABLE errors RESULT_VARIABLE result TIMEOUT 300)
	message("aion_gs_m4_database ${command}: ${output}${errors}")
	if(NOT result STREQUAL "0")
		m4_fail("database" "aion_gs_m4_database ${command} failed (${result}): ${errors}. Start MariaDB or fix AION_TEST_GS_DATABASE_URL.")
	endif()
	if(NOT output MATCHES "${expected_output}")
		m4_fail("database" "aion_gs_m4_database ${command}: expected '${expected_output}'")
	endif()
endfunction()

# the server run: exit code 0, the log in OUTPUT_DIR/<name>.log, common checks; the log text is returned in <out_log>
function(m4_server name out_log)
	execute_process(
		# --log-folder: the run's own log directory, never the shared <WORKING_DIRECTORY>/log (Logging::init archives and deletes the *.log
		# files it finds there, so two server processes in one log directory fail each other's archiving; RunStartupSmoke.cmake does the same)
		COMMAND "${EXECUTABLE}" ${ARGN} ${database_arguments} "--log-folder=${OUTPUT_DIR}/${name}_log"
		WORKING_DIRECTORY "${WORKING_DIRECTORY}"
		OUTPUT_FILE "${OUTPUT_DIR}/${name}.log"
		ERROR_FILE "${OUTPUT_DIR}/${name}.stderr.log"
		RESULT_VARIABLE result
		TIMEOUT 1200
	)
	file(READ "${OUTPUT_DIR}/${name}.log" log)
	file(READ "${OUTPUT_DIR}/${name}.stderr.log" errors)
	string(APPEND log "${errors}")
	string(LENGTH "${log}" length)
	if(length GREATER 6000)
		math(EXPR start "${length} - 6000")
		string(SUBSTRING "${log}" ${start} 6000 tail)
	else()
		set(tail "${log}")
	endif()
	message("aion_game_server ${ARGN}: exit code ${result}, log ${OUTPUT_DIR}/${name}.log (last 6000 characters):\n${tail}")
	if(log MATCHES "startup stopped at an unported function: ([^\n]*)")
		m4_fail("item 7" "the startup stopped at an unported function: ${CMAKE_MATCH_1}")
	endif()
	if(NOT result STREQUAL "0")
		m4_fail("${name}" "aion_game_server exit code '${result}'")
	endif()
	if(log MATCHES "is unknown and therefore ignored")
		m4_fail("item 1" "Config.load warned about unknown properties (the predicted set is empty)")
	endif()
	# a newline in front, so an ERROR on the very first log line is found as well
	set(scan "\n${log}")
	if(scan MATCHES "\n[0-9:]+ ERROR ([^\n]*)")
		m4_fail("${name}" "the log has an ERROR line: ${CMAKE_MATCH_1}")
	endif()
	if(NOT log MATCHES "Runtime shut down: 0 tasks left, [0-9]+ cleaner ids drained, reclaimer backlog 0, 0 objects still tracked")
		m4_fail("shutdown" "no orderly runtime shutdown with an empty LeakCensus")
	endif()
	set(${out_log} "${log}" PARENT_SCOPE)
endfunction()

function(m4_python item)
	execute_process(COMMAND "${PYTHON}" ${ARGN} WORKING_DIRECTORY "${ORACLE_DIR}" OUTPUT_VARIABLE output ERROR_VARIABLE errors RESULT_VARIABLE result
		TIMEOUT 600)
	message("python ${ARGN}:\n${output}${errors}")
	if(NOT result STREQUAL "0")
		m4_fail("${item}" "python ${ARGN} failed (${result})")
	endif()
endfunction()

function(m4_summary dir key out_value)
	file(STRINGS "${dir}/m4_summary.txt" lines REGEX "^${key} ")
	if(NOT lines MATCHES "^${key} (.+)$")
		m4_fail("summary" "${dir}/m4_summary.txt has no ${key}")
	endif()
	set(${out_value} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

# 1-2: IDFactory from the empty schema
m4_database(create "created ${database}")
m4_server(id_factory log --check-id-factory "--check-output=${OUTPUT_DIR}/empty")
if(NOT log MATCHES "IDFactory: 1 IDs used\\.")
	m4_fail("item 2" "IDFactory on the empty schema did not log 'IDFactory: 1 IDs used.'")
endif()
m4_summary("${OUTPUT_DIR}/empty" idsUsed ids)
if(NOT ids STREQUAL "1")
	m4_fail("item 2" "IDFactory on the empty schema: ${ids} IDs used, expected 1")
endif()

# 3-5: the whole path on the fixture schema
m4_database(fixture "fixture ids 11")
m4_python("item 5" -m geo m4-probes --out "${OUTPUT_DIR}/geo_probes.txt")
m4_server(check log --check-static-data "--check-output=${OUTPUT_DIR}/full" "--check-geo-probes=${OUTPUT_DIR}/geo_probes.txt")
set(full "${OUTPUT_DIR}/full")
if(NOT log MATCHES "IDFactory: 12 IDs used\\.")
	m4_fail("item 2" "IDFactory on the fixture schema did not log 'IDFactory: 12 IDs used.' (id 0 and the 11 fixture ids)")
endif()
if(NOT log MATCHES "##### \\[Static Data loaded in [0-9.]+ seconds\\] #####")
	m4_fail("item 3" "DataManager did not finish")
endif()
if(NOT log MATCHES "World: 161 world maps created\\.")
	m4_fail("item 6" "World did not create 161 world maps")
endif()
if(NOT log MATCHES "M4 startup path \\(Config, database, runtime, static data, zones, geo, world\\) completed in ([0-9]+) ms, peak working set ([0-9]+) MB")
	m4_fail("load time" "the load time and peak working set line is missing")
endif()
set(load_time_ms "${CMAKE_MATCH_1}")
set(peak_mb "${CMAKE_MATCH_2}")
m4_summary("${full}" unportedHits hits)
file(STRINGS "${full}/unported_trace.txt" trace)
list(LENGTH trace trace_lines)
if(NOT hits STREQUAL "0" OR NOT trace_lines EQUAL 1)
	m4_fail("item 7" "${hits} AION_UNPORTED hits, see ${full}/unported_trace.txt")
endif()
m4_database(online "online players 0")

# 6-7: the oracles
m4_python("item 4" oracle.py compare-counts --log "${full}/static_data_counts.txt")
m4_python("items 5 and 6" -m geo m4-compare --dir "${full}")

set_property(GLOBAL PROPERTY m4_database_created FALSE)
m4_database(drop "dropped ${database}")
message(STATUS "gs.m4.check_static_data: passed (load time ${load_time_ms} ms, peak working set ${peak_mb} MB, 0 AION_UNPORTED hits)")
