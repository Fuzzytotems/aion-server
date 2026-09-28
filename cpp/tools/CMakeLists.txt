# Python tooling (generators, lints, oracles) under cpp/tools/<name>. Each tool with a tests/ directory gets a CTest entry that runs its
# stdlib unittest suite: python -m unittest discover -s tests (from the tool directory).
find_package(Python3 3.12 COMPONENTS Interpreter)
if(NOT Python3_Interpreter_FOUND)
	message(STATUS "Python 3.12 not found: tool tests are not registered")
	return()
endif()
file(GLOB tool_dirs LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/*")
foreach(dir IN LISTS tool_dirs)
	if(IS_DIRECTORY "${dir}/tests")
		get_filename_component(tool "${dir}" NAME)
		add_test(NAME tools.${tool} COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s tests -t . WORKING_DIRECTORY "${dir}")
	endif()
endforeach()
