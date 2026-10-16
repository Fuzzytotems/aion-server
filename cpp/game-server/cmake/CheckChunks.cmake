# Runs the chunk ownership checks of cmake/AionChunks.cmake without configuring the project and writes chunks.json:
#   cmake [-DJSON=<file>] [-DJAVA_DIR=<game-server>] [-DSOURCE_DIR=<cpp/game-server>] [-DMANIFEST=<chunks.cmake>] -P CheckChunks.cmake
# Fails like the configure step (one FATAL_ERROR listing every problem: files with no or several owners, unexpected C++ extensions, test files
# outside the test directories, unclaimed Java files). tools/porting/tests/test_chunks.py uses it to check that chunks.py and the CMake
# implementation agree on fixtures and on the real tree.

cmake_minimum_required(VERSION 3.28)
get_filename_component(here "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
if(NOT SOURCE_DIR)
	get_filename_component(SOURCE_DIR "${here}/.." ABSOLUTE)
endif()
if(NOT JAVA_DIR)
	get_filename_component(JAVA_DIR "${SOURCE_DIR}/../../game-server" ABSOLUTE)
endif()
if(NOT MANIFEST)
	set(MANIFEST "${SOURCE_DIR}/chunks.cmake")
endif()
if(NOT JSON)
	set(JSON "${CMAKE_CURRENT_BINARY_DIR}/chunks.json")
endif()
include("${here}/AionChunks.cmake")
include("${MANIFEST}")
aion_gs_check_chunks(SOURCE_DIR "${SOURCE_DIR}" JAVA_DIR "${JAVA_DIR}" JSON "${JSON}")
