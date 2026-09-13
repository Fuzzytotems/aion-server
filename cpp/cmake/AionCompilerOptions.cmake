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

# Adds a GoogleTest executable from all .cpp files in the given directory.
function(aion_add_tests target test_dir)
	cmake_parse_arguments(ARG "" "" "DEPENDS" ${ARGN})
	if(NOT AION_BUILD_TESTS)
		return()
	endif()
	file(GLOB_RECURSE test_sources CONFIGURE_DEPENDS "${test_dir}/*.cpp" "${test_dir}/*.h")
	if(NOT test_sources)
		return()
	endif()
	add_executable(${target} ${test_sources})
	target_link_libraries(${target} PRIVATE ${ARG_DEPENDS} GTest::gtest GTest::gtest_main aion::compiler_options)
	target_include_directories(${target} PRIVATE "${test_dir}")
	gtest_discover_tests(${target} DISCOVERY_MODE PRE_TEST WORKING_DIRECTORY "${test_dir}")
endfunction()
