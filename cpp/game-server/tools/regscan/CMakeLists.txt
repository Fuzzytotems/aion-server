# aion_gs_regscan: build tool that scans handler and client packet sources for registration markers and generates the registry tables of
# aion/gameserver/handlers/HandlerRegistry.h (docs/design/handlers-and-porting-plan.md §1.5). Dependency-free C++23. See README.md.
#
# Owner: P4-02b (handler registry). The CMake function aion_gs_add_registries() is defined in AionRegscan.cmake.

add_library(aion_gs_regscan_lib STATIC
	src/Emitter.cpp
	src/Emitter.h
	src/JavaScanner.cpp
	src/JavaScanner.h
	src/Lexer.cpp
	src/Lexer.h
	src/Registry.cpp
	src/Registry.h
	src/SourceScanner.cpp
	src/SourceScanner.h
	src/SpawnIds.cpp
	src/SpawnIds.h
)
target_include_directories(aion_gs_regscan_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(aion_gs_regscan_lib PUBLIC aion::compiler_options)

add_executable(aion_gs_regscan src/main.cpp)
target_link_libraries(aion_gs_regscan PRIVATE aion_gs_regscan_lib)

include("${CMAKE_CURRENT_SOURCE_DIR}/AionRegscan.cmake")

if(NOT AION_BUILD_TESTS)
	return()
endif()

set(REGSCAN_FIXTURES "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")

# Unit tests of the scanner library (in-memory sources, fixture trees, and the real Java handler tree when it exists)
aion_add_tests(aion_gs_regscan_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit" DEPENDS aion_gs_regscan_lib)
if(TARGET aion_gs_regscan_tests)
	target_compile_definitions(aion_gs_regscan_tests PRIVATE
		AION_REGSCAN_FIXTURES_DIR="${REGSCAN_FIXTURES}"
		AION_GAMESERVER_JAVA_DIR="${GS_JAVA_DIR}"
		AION_REGSCAN_TEMP_DIR="${CMAKE_CURRENT_BINARY_DIR}/unit_test_output"
	)
endif()

# Integration: a fixture handler tree compiled against stand-in core classes, registered through aion_gs_add_registries() exactly like the
# server will do it, and tests that call the generated tables and factories.
set(FIXTURE "${REGSCAN_FIXTURES}/valid")
file(GLOB_RECURSE fixture_sources CONFIGURE_DEPENDS
	"${FIXTURE}/handlers/*.cpp" "${FIXTURE}/handlers/*.h" "${FIXTURE}/src/*.cpp" "${FIXTURE}/src/*.h" "${FIXTURE}/core/*.h")
add_library(aion_gs_regscan_fixture_handlers STATIC ${fixture_sources})
target_include_directories(aion_gs_regscan_fixture_handlers PUBLIC "${FIXTURE}/handlers" "${FIXTURE}/src" "${FIXTURE}/core")
target_link_libraries(aion_gs_regscan_fixture_handlers PUBLIC aion_gs_handler_registry)
# Handler libraries contain equal file names in different directories (CalindiFlamelordAI.cpp, PadmarashkaAI.cpp): without this, MSBuild's
# C++ module dependency scan writes one module.json per file name and warns MSB8074.
set_target_properties(aion_gs_regscan_fixture_handlers PROPERTIES CXX_SCAN_FOR_MODULES OFF)

aion_gs_add_registries(aion_gs_regscan_fixture_registry
	HANDLERS_ROOT "${FIXTURE}/handlers"
	CLIENTPACKETS_ROOT "${FIXTURE}/src"
	JAVA_HANDLERS "${FIXTURE}/java/handlers"
	JAVA_CLIENT_PACKET_FACTORY "${FIXTURE}/java/AionClientPacketFactory.java"
	HANDLER_LIBRARIES aion_gs_regscan_fixture_handlers
)

aion_add_tests(aion_gs_regscan_integration_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration" DEPENDS aion_gs_regscan_fixture_registry)
if(TARGET aion_gs_regscan_integration_tests)
	target_compile_definitions(aion_gs_regscan_integration_tests PRIVATE AION_REGSCAN_FIXTURE_REPORT="${aion_gs_regscan_fixture_registry_REPORT}")
endif()
aion_add_tests(aion_gs_regscan_empty_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/empty" DEPENDS aion_gs_regscan_fixture_registry_empty)

# Command line: rule violations fail with MSBuild-style diagnostics; the real Java handler tree yields the M6 target counts.
add_test(NAME aion_gs_regscan.cli.violations
	COMMAND aion_gs_regscan --out "${CMAKE_CURRENT_BINARY_DIR}/cli_violations" --handlers-root "${REGSCAN_FIXTURES}/invalid/handlers")
set_tests_properties(aion_gs_regscan.cli.violations PROPERTIES
	WILL_FAIL FALSE
	PASS_REGULAR_EXPRESSION "WrongNamespaceAI\\.cpp\\([0-9]+,[0-9]+\\): error: AION_AI must be in namespace aion::gameserver::handlers::ai")
add_test(NAME aion_gs_regscan.cli.exit_code
	COMMAND aion_gs_regscan --out "${CMAKE_CURRENT_BINARY_DIR}/cli_violations" --handlers-root "${REGSCAN_FIXTURES}/invalid/handlers")
set_tests_properties(aion_gs_regscan.cli.exit_code PROPERTIES WILL_FAIL TRUE)
if(IS_DIRECTORY "${GS_JAVA_DIR}/data/handlers")
	add_test(NAME aion_gs_regscan.cli.java_tree
		COMMAND aion_gs_regscan --out "${CMAKE_CURRENT_BINARY_DIR}/cli_java_tree"
			--java-handlers "${GS_JAVA_DIR}/data/handlers"
			--java-client-packet-factory "${GS_JAVA_DIR}/src/com/aionemu/gameserver/network/aion/AionClientPacketFactory.java")
	set_tests_properties(aion_gs_regscan.cli.java_tree PROPERTIES PASS_REGULAR_EXPRESSION
		"ai 0/457, instance 0/73, zone names 0/5, quest 0/1035, admin commands 0/101, player commands 0/16, console commands 0/35, client packets 0/186, npc ids spawned by handlers 0/1093")
endif()
