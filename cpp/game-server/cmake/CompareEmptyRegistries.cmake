# CTest gs.registry.empty_tables: the empty registry tables that aion_gs_add_registries() writes at configure time (<out>/empty) equal the
# .empty.gen.cpp files of aion_gs_regscan (<out>), so the configure-time copies cannot drift from the tool's emitter.
#   cmake -DOUT_DIR=<registry output directory> -DNAMES=ai,instance,... -P CompareEmptyRegistries.cmake

string(REPLACE "," ";" names "${NAMES}")
set(problems)
foreach(registry IN LISTS names)
	set(tool "${OUT_DIR}/Registry.${registry}.empty.gen.cpp")
	set(configured "${OUT_DIR}/empty/Registry.${registry}.empty.gen.cpp")
	if(NOT EXISTS "${tool}" OR NOT EXISTS "${configured}")
		list(APPEND problems "missing ${tool} or ${configured} (build the registry scan first)")
		continue()
	endif()
	file(READ "${tool}" tool_text)
	file(READ "${configured}" configured_text)
	if(NOT tool_text STREQUAL configured_text)
		list(APPEND problems "${configured} differs from the tool's ${tool} (update aion_gs_add_registries in AionRegscan.cmake)")
	endif()
endforeach()
if(problems)
	list(JOIN problems "\n  " text)
	message(FATAL_ERROR "gs.registry.empty_tables:\n  ${text}")
endif()
message(STATUS "gs.registry.empty_tables: ${NAMES} match")
