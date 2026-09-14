# aion_gs_add_registries(): generates the handler registry tables of HandlerRegistry.h with aion_gs_regscan on every build that changed a
# scanned source, and wraps them in static libraries. Include this file (or add game-server/tools/regscan first) before calling it.
#
#   aion_gs_add_registries(<name>
#       [HANDLERS_ROOT <dir>]                    include root that contains aion/gameserver/handlers (e.g. game-server/handlers)
#       [CLIENTPACKETS_ROOT <dir>]               include root that contains aion/gameserver/network/aion/clientpackets (e.g. game-server/src)
#       [JAVA_HANDLERS <dir>]                    ../game-server/data/handlers: Java cross-checks and the missing-key report
#       [JAVA_CLIENT_PACKET_FACTORY <file>]      AionClientPacketFactory.java: AION_CLIENT_PACKET classes must be in its table
#       [HANDLER_LIBRARIES <target>...]          libraries that define the factories (linked PUBLIC by the registry libraries)
#       [OUT_DIR <dir>]                          default: ${CMAKE_CURRENT_BINARY_DIR}/<name>
#   )
#
# Creates, for each registry r in ai instance zone quest commands clientpackets npcids:
#   <name>_<r>           STATIC library with the generated table (links HANDLER_LIBRARIES and aion_gs_handler_registry)
#   <name>_<r>_empty     STATIC library with the same accessor and an empty table (for tests that need no handlers)
# plus the INTERFACE libraries <name> (all tables) and <name>_empty (all empty tables), and the custom target <name>_scan that runs the tool.
# The report is written to <OUT_DIR>/registry_report.txt; the variable <name>_REPORT is set in the caller's scope.
#
# Behaviour: the scan runs when aion_gs_regscan or any scanned file changed (the file lists are globbed with CONFIGURE_DEPENDS, so added
# and removed files are noticed). Generated files are only rewritten when their content changes, so editing a handler body does not
# recompile the tables. Violations of the marker and file rules fail the <name>_scan target with "file(line,column): error: ..." lines, and
# the previous tables stay untouched. The tables reference every factory, so linking <name> pulls all handler objects out of their static
# libraries without /WHOLEARCHIVE.
#
# Example (spine step S0a):
#   aion_gs_add_registries(aion_gs_registry
#       HANDLERS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/handlers"
#       CLIENTPACKETS_ROOT "${GS_SRC}"
#       JAVA_HANDLERS "${GS_JAVA_DIR}/data/handlers"
#       JAVA_CLIENT_PACKET_FACTORY "${GS_JAVA_DIR}/src/com/aionemu/gameserver/network/aion/AionClientPacketFactory.java"
#       HANDLER_LIBRARIES aion_gs_handlers_ai_core aion_gs_network)
#   target_link_libraries(aion_game_server PRIVATE aion_gs_registry)
#   target_link_libraries(aion_gs_world_tests PRIVATE aion_gs_registry_empty)

set(AION_GS_REGISTRY_NAMES ai instance zone quest commands clientpackets npcids)

function(aion_gs_add_registries name)
	cmake_parse_arguments(ARG "" "HANDLERS_ROOT;CLIENTPACKETS_ROOT;JAVA_HANDLERS;JAVA_CLIENT_PACKET_FACTORY;OUT_DIR" "HANDLER_LIBRARIES" ${ARGN})
	if(ARG_UNPARSED_ARGUMENTS)
		message(FATAL_ERROR "aion_gs_add_registries(${name}): unknown arguments ${ARG_UNPARSED_ARGUMENTS}")
	endif()
	if(NOT ARG_OUT_DIR)
		set(ARG_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/${name}")
	endif()

	set(tool_args --out "${ARG_OUT_DIR}" --stamp "${ARG_OUT_DIR}/regscan.stamp" --quiet)
	set(scanned)
	if(ARG_HANDLERS_ROOT)
		if(NOT IS_DIRECTORY "${ARG_HANDLERS_ROOT}/aion/gameserver/handlers")
			message(FATAL_ERROR "aion_gs_add_registries(${name}): ${ARG_HANDLERS_ROOT}/aion/gameserver/handlers does not exist")
		endif()
		list(APPEND tool_args --handlers-root "${ARG_HANDLERS_ROOT}")
		file(GLOB_RECURSE files CONFIGURE_DEPENDS "${ARG_HANDLERS_ROOT}/aion/gameserver/handlers/*")
		list(APPEND scanned ${files})
	endif()
	if(ARG_CLIENTPACKETS_ROOT)
		if(NOT IS_DIRECTORY "${ARG_CLIENTPACKETS_ROOT}/aion/gameserver/network/aion/clientpackets")
			message(FATAL_ERROR "aion_gs_add_registries(${name}): ${ARG_CLIENTPACKETS_ROOT}/aion/gameserver/network/aion/clientpackets does not exist")
		endif()
		list(APPEND tool_args --clientpackets-root "${ARG_CLIENTPACKETS_ROOT}")
		file(GLOB_RECURSE files CONFIGURE_DEPENDS "${ARG_CLIENTPACKETS_ROOT}/aion/gameserver/network/aion/clientpackets/*")
		list(APPEND scanned ${files})
	endif()
	if(ARG_JAVA_HANDLERS)
		list(APPEND tool_args --java-handlers "${ARG_JAVA_HANDLERS}")
		file(GLOB_RECURSE files CONFIGURE_DEPENDS "${ARG_JAVA_HANDLERS}/*.java")
		list(APPEND scanned ${files})
	endif()
	if(ARG_JAVA_CLIENT_PACKET_FACTORY)
		list(APPEND tool_args --java-client-packet-factory "${ARG_JAVA_CLIENT_PACKET_FACTORY}")
		list(APPEND scanned "${ARG_JAVA_CLIENT_PACKET_FACTORY}")
	endif()

	set(real_sources)
	set(empty_sources)
	foreach(registry IN LISTS AION_GS_REGISTRY_NAMES)
		list(APPEND real_sources "${ARG_OUT_DIR}/Registry.${registry}.gen.cpp")
		list(APPEND empty_sources "${ARG_OUT_DIR}/Registry.${registry}.empty.gen.cpp")
	endforeach()

	# The stamp is the only OUTPUT, so the rule belongs to <name>_scan alone (listing the tables as outputs would attach the rule to all 14
	# libraries, which the Visual Studio generator runs concurrently). The tables are BYPRODUCTS for Ninja's restat and marked GENERATED.
	add_custom_command(
		OUTPUT "${ARG_OUT_DIR}/regscan.stamp"
		BYPRODUCTS ${real_sources} ${empty_sources} "${ARG_OUT_DIR}/registry_report.txt"
		COMMAND aion_gs_regscan ${tool_args}
		DEPENDS aion_gs_regscan ${scanned}
		COMMENT "aion_gs_regscan: ${name}"
		VERBATIM
	)
	add_custom_target(${name}_scan DEPENDS "${ARG_OUT_DIR}/regscan.stamp")
	set_source_files_properties(${real_sources} ${empty_sources} PROPERTIES GENERATED TRUE)

	add_library(${name} INTERFACE)
	add_library(${name}_empty INTERFACE)
	foreach(registry IN LISTS AION_GS_REGISTRY_NAMES)
		add_library(${name}_${registry} STATIC "${ARG_OUT_DIR}/Registry.${registry}.gen.cpp")
		add_library(${name}_${registry}_empty STATIC "${ARG_OUT_DIR}/Registry.${registry}.empty.gen.cpp")
		foreach(target IN ITEMS ${name}_${registry} ${name}_${registry}_empty)
			add_dependencies(${target} ${name}_scan)
			target_link_libraries(${target} PUBLIC aion_gs_handler_registry)
		endforeach()
		target_link_libraries(${name}_${registry} PUBLIC ${ARG_HANDLER_LIBRARIES})
		target_link_libraries(${name} INTERFACE ${name}_${registry})
		target_link_libraries(${name}_empty INTERFACE ${name}_${registry}_empty)
	endforeach()
	set(${name}_REPORT "${ARG_OUT_DIR}/registry_report.txt" PARENT_SCOPE)
endfunction()
