# Generates aion/BuildInfo.h with git revision, branch and build date (the C++ equivalent of the Maven jar manifest entries
# POMVersion/Revision/Branch/Date read by com.aionemu.commons.utils.info.VersionInfo). Only refreshed when CMake reconfigures.

find_package(Git QUIET)
set(AION_GIT_REVISION "unknown")
set(AION_GIT_BRANCH "unknown")
if(GIT_FOUND)
	execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		OUTPUT_VARIABLE AION_GIT_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
	execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --abbrev-ref HEAD WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		OUTPUT_VARIABLE AION_GIT_BRANCH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
	execute_process(COMMAND "${GIT_EXECUTABLE}" status --porcelain WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		OUTPUT_VARIABLE _git_status OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
	if(_git_status)
		string(APPEND AION_GIT_REVISION "-DIRTY")
	endif()
endif()
string(TIMESTAMP AION_BUILD_DATE "%Y-%m-%dT%H:%M:%SZ" UTC)

set(AION_GENERATED_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated")
configure_file("${CMAKE_CURRENT_LIST_DIR}/BuildInfo.h.in" "${AION_GENERATED_INCLUDE_DIR}/aion/BuildInfo.h" @ONLY)
