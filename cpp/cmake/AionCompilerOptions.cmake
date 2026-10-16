# Common compiler settings, applied to every project target via aion_configure_target().

add_library(aion_compiler_options INTERFACE)
add_library(aion::compiler_options ALIAS aion_compiler_options)

if(MSVC)
	target_compile_options(aion_compiler_options INTERFACE
		/W4 /permissive- /utf-8 /Zc:__cplusplus /Zc:preprocessor /EHsc /MP
		/wd4100 # unreferenced formal parameter (common in virtual hook overrides)
	)
	target_compile_definitions(aion_compiler_options INTERFACE
		_WIN32_WINNT=0x0A00 # Windows 10+, required by Asio
		WIN32_LEAN_AND_MEAN
		NOGDI # keeps wingdi.h macros such as ERROR, ABSOLUTE, RELATIVE, TRANSPARENT out entirely (see utils/WindowsMacroGuard.h for the rest)
		NOMINMAX
		_CRT_SECURE_NO_WARNINGS
	)
else()
	target_compile_options(aion_compiler_options INTERFACE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

target_compile_definitions(aion_compiler_options INTERFACE
	ASIO_STANDALONE
	ASIO_NO_DEPRECATED
	SPDLOG_FMT_EXTERNAL
)

# AddressSanitizer build (preset msvc-asan). Every project target is instrumented; vcpkg dependencies are not (MSVC supports mixing).
# The ASan runtime DLLs are copied next to each test executable (aion_copy_asan_runtime), so tests run from any shell without PATH changes.
option(AION_ASAN "Build with AddressSanitizer (MSVC: /fsanitize=address)" OFF)
if(AION_ASAN)
	if(MSVC)
		# /RTC (runtime checks) and /ZI (edit and continue) are incompatible with /fsanitize=address
		foreach(flags_var CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS)
			string(REGEX REPLACE "/RTC[1csu]+" "" ${flags_var} "${${flags_var}}")
			string(REPLACE "/ZI" "/Zi" ${flags_var} "${${flags_var}}")
			set(${flags_var} "${${flags_var}}") # included from the top-level CMakeLists, so this is the top directory scope
		endforeach()
		target_compile_options(aion_compiler_options INTERFACE /fsanitize=address)
		target_link_options(aion_compiler_options INTERFACE /INCREMENTAL:NO)
		target_compile_definitions(aion_compiler_options INTERFACE AION_ASAN=1)
		get_filename_component(AION_ASAN_RUNTIME_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
		set(AION_ASAN_RUNTIME_DIR "${AION_ASAN_RUNTIME_DIR}" CACHE INTERNAL "directory of clang_rt.asan_*dynamic-x86_64.dll")
	else()
		target_compile_options(aion_compiler_options INTERFACE -fsanitize=address -fno-omit-frame-pointer)
		target_link_options(aion_compiler_options INTERFACE -fsanitize=address)
		target_compile_definitions(aion_compiler_options INTERFACE AION_ASAN=1)
	endif()
endif()

# Copies the MSVC ASan runtime DLLs next to an executable (no-op unless AION_ASAN).
function(aion_copy_asan_runtime target)
	if(AION_ASAN AND MSVC)
		foreach(dll clang_rt.asan_dynamic-x86_64.dll clang_rt.asan_dbg_dynamic-x86_64.dll)
			if(EXISTS "${AION_ASAN_RUNTIME_DIR}/${dll}")
				add_custom_command(TARGET ${target} POST_BUILD
					COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${AION_ASAN_RUNTIME_DIR}/${dll}" "$<TARGET_FILE_DIR:${target}>"
					VERBATIM)
			endif()
		endforeach()
	endif()
endfunction()

# Adds a static library whose sources are all .cpp/.h files below src/<subdir>. Headers are included as "aion/<module>/...".
function(aion_add_library target)
	cmake_parse_arguments(ARG "" "SOURCE_ROOT;EXCLUDE_REGEX" "SOURCE_DIRS;DEPENDS" ${ARGN})
	set(sources)
	foreach(dir IN LISTS ARG_SOURCE_DIRS)
		file(GLOB_RECURSE dir_sources CONFIGURE_DEPENDS "${ARG_SOURCE_ROOT}/${dir}/*.cpp" "${ARG_SOURCE_ROOT}/${dir}/*.h")
		list(APPEND sources ${dir_sources})
	endforeach()
	if(ARG_EXCLUDE_REGEX AND sources)
		list(FILTER sources EXCLUDE REGEX "${ARG_EXCLUDE_REGEX}")
	endif()
	if(sources)
		source_group(TREE "${ARG_SOURCE_ROOT}" FILES ${sources})
	else()
		# subsystem not ported yet: keep the target valid so dependents still configure
		set(placeholder "${CMAKE_CURRENT_BINARY_DIR}/${target}_placeholder.cpp")
		file(WRITE "${placeholder}" "// placeholder for ${target}, which has no sources yet\n")
		list(APPEND sources "${placeholder}")
	endif()
	add_library(${target} STATIC ${sources})
	target_include_directories(${target} PUBLIC "${ARG_SOURCE_ROOT}")
	# PUBLIC so that consumers see the same preprocessor definitions (e.g. ASIO_STANDALONE, _WIN32_WINNT) as the library
	target_link_libraries(${target} PUBLIC ${ARG_DEPENDS} aion::compiler_options)
endfunction()

# Adds a GoogleTest executable from all .cpp files in the given directory (and the EXTRA_DIRS; each directory is an include directory).
#   aion_add_tests(<target> <test_dir> [EXTRA_DIRS <dir>...] [DEPENDS <target>...])
# The tests run (and are discovered) in <build dir>/test_work/<target>: gtest_discover_tests writes its cmake_test_discovery_*.json into the
# working directory, and files a test creates relative to it stay out of the source tree.
function(aion_add_tests target test_dir)
	cmake_parse_arguments(ARG "" "" "DEPENDS;EXTRA_DIRS" ${ARGN})
	if(NOT AION_BUILD_TESTS)
		return()
	endif()
	set(test_dirs "${test_dir}" ${ARG_EXTRA_DIRS})
	set(test_sources)
	foreach(dir IN LISTS test_dirs)
		file(GLOB_RECURSE dir_sources CONFIGURE_DEPENDS "${dir}/*.cpp" "${dir}/*.h")
		list(APPEND test_sources ${dir_sources})
	endforeach()
	if(NOT test_sources)
		return()
	endif()
	add_executable(${target} ${test_sources})
	target_link_libraries(${target} PRIVATE ${ARG_DEPENDS} GTest::gtest GTest::gtest_main aion::compiler_options)
	target_include_directories(${target} PRIVATE ${test_dirs})
	aion_copy_asan_runtime(${target})
	set(work_dir "${CMAKE_CURRENT_BINARY_DIR}/test_work/${target}")
	file(MAKE_DIRECTORY "${work_dir}")
	gtest_discover_tests(${target} DISCOVERY_MODE PRE_TEST WORKING_DIRECTORY "${work_dir}")
endfunction()

# Sets CTest properties on the tests that gtest_discover_tests found in <target> (created by aion_add_tests in the current directory) whose
# name ("Suite.Test") matches REGEX, e.g. labels for the tests that read the Java data tree:
#   aion_set_discovered_test_properties(aion_gs_xml_tests REGEX "^StaticDataRealFilesTest\\." PROPERTIES LABELS realdata)
# Discovery runs at ctest time (PRE_TEST), so this appends a script to the directory's TEST_INCLUDE_FILES that runs after the discovered list.
# Every argument after PROPERTIES is one property name or one value, and a value may be a list: LABELS "scenario;realdata" gives the tests
# both labels. The arguments are parsed with PARSE_ARGV, which keeps such a value one element (`${ARGN}` would flatten it into two, and every
# name and value after it would pair up wrongly), and each one is written into the script as one bracket argument.
function(aion_set_discovered_test_properties target)
	cmake_parse_arguments(PARSE_ARGV 1 ARG "" "REGEX;DIRECTORY" "PROPERTIES")
	if(NOT AION_BUILD_TESTS OR NOT TARGET ${target})
		return()
	endif()
	if(NOT ARG_DIRECTORY)
		set(ARG_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
	endif()
	set(properties)
	foreach(value IN LISTS ARG_PROPERTIES)
		string(APPEND properties " [==[${value}]==]")
	endforeach()
	string(MD5 hash "${ARG_REGEX}${properties}")
	string(SUBSTRING "${hash}" 0 8 hash)
	get_property(binary_dir DIRECTORY "${ARG_DIRECTORY}" PROPERTY BINARY_DIR)
	set(script "${binary_dir}/${target}_properties_${hash}.cmake")
	file(CONFIGURE OUTPUT "${script}" CONTENT "# generated by aion_set_discovered_test_properties
foreach(aion_test IN LISTS ${target}_TESTS)
	if(aion_test MATCHES [==[${ARG_REGEX}]==])
		set_tests_properties(\"\${aion_test}\" PROPERTIES${properties})
	endif()
endforeach()
" @ONLY)
	set_property(DIRECTORY "${ARG_DIRECTORY}" APPEND PROPERTY TEST_INCLUDE_FILES "${script}")
endfunction()
