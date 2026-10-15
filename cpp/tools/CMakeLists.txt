# Python tooling (generators, lints, oracles) under cpp/tools/<name>. Each tool with a tests/ directory gets a CTest entry that runs its
# stdlib unittest suite: python -m unittest discover -s tests (from the tool directory).
find_package(Python3 3.12 COMPONENTS Interpreter)
if(NOT Python3_Interpreter_FOUND)
	message(STATUS "Python 3.12 not found: tool tests are not registered")
	return()
endif()
# The compile checks of tools/gen include the vcpkg headers of this build, wherever VCPKG_INSTALLED_DIR points (a git worktree configured
# against the main tree's vcpkg_installed has none of its own); run outside CTest they fall back to cpp/vcpkg_installed/x64-windows.
set(tool_env "")
if(VCPKG_INSTALLED_DIR AND VCPKG_TARGET_TRIPLET)
	set(tool_env "AION_VCPKG_INCLUDE=${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include")
endif()
file(GLOB tool_dirs LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/*")
foreach(dir IN LISTS tool_dirs)
	if(IS_DIRECTORY "${dir}/tests")
		get_filename_component(tool "${dir}" NAME)
		add_test(NAME tools.${tool} COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s tests -t . WORKING_DIRECTORY "${dir}")
		if(tool_env)
			set_tests_properties(tools.${tool} PROPERTIES ENVIRONMENT "${tool_env}")
		endif()
	endif()
endforeach()
