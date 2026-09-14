# Port of ../game-server (com.aionemu.gameserver). For now: the runtime kernel (docs/design/runtime-architecture.md) only.
#
# Layout
#   src/aion/gameserver/runtime/<area>/...   kernel code without a Java counterpart, namespace aion::gameserver::runtime (flat, for every area)
#   src/aion/gameserver/<java package>/...   ported Java classes at their Java package paths (utils/ThreadPoolManager.h, utils/idfactory/...)
#   tests/runtime/<area>/                     one GoogleTest executable per kernel library
#   bench/                                    benchmarks (P4), built only with AION_BUILD_BENCH
#
# Kernel libraries and their dependency direction (arrows point at the dependency; no cycles):
#
#   aion_gs_runtime_base         checked-build macros (AION_CHECKED), Java exception types, TaskInfo, PCT yield points, per-thread context
#        ^                       registry. Depends on commons core only.
#   aion_gs_runtime_sync         Monitor, SYNCHRONIZED/monitorOf, RankedMutex + LockRank, LockClass, lock-order validator, BlockingRegion,
#        ^                       StampedLock, Semaphore, Watchdog core. Knows nothing about Ref/epochs: RefCounted contains a Monitor, so
#        |                       locks sit BELOW the lifetime core. The watchdog reads per-thread state from base's ThreadContext and gets
#        |                       reclamation metrics through probes registered by the Reclaimer (no upward dependency).
#   aion_gs_runtime_lifetime     RefCounted/Immortal, Ref/Ptr/makeRef/cast/as, TaskScope/QuiescentScope/quiescentPoint, Reclaimer, parts
#        ^                       (OwnedPart, PartSlot, PartMap, PartList, SelfOrRef), PCT scheduler.
#        +------------------+-----------------------+
#   aion_gs_runtime_fields   aion_gs_runtime_collections   aion_gs_runtime_sched
#   Field/Final/Array/       Java collection shims,         Future, Pin, TaskArg/bindTask, PinnedCallback, ExecutorBackend, clocks,
#   Atomic* shims (need      ConcurrentHashMap,             DeterministicExecutor, SerialExecutor, ForkJoinPool, utils/ThreadPoolManager
#   Ref + read barrier,      CopyOnWriteArrayList ...
#   so above lifetime)       (the three are siblings: they only depend on lifetime and below, so they build independently)
#        ^                        ^                          ^
#        +------------------------+--------------------------+
#   aion_gs_runtime_services     CleanerQueue/CleanerDrain, LeakCensus, ZombieBreaker, introspection, services/cron (CronExpression,
#                                CronService), utils/cron, utils/idfactory (IDFactory)
#
# Ownership during the parallel implementation stage (each area owns its source directories AND its test directory):
#   lifetime agent:    runtime/base, runtime/lifetime, tests/runtime/base, tests/runtime/lifetime
#   sync agent:        runtime/sync, runtime/fields, tests/runtime/sync, tests/runtime/fields
#   collections agent: runtime/collections, tests/runtime/collections
#   sched agent:       runtime/sched, utils/ThreadPoolManager.*, tests/runtime/sched
#   services (later):  runtime/services, services/cron, utils/cron, utils/idfactory, tests/runtime/services
#   stress (later):    tests/runtime/stress;  bench (later): bench/
# This file and the presets belong to the integrator (headers stage / fixers).

set(GS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(GS_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
set(GS_JAVA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../game-server")

# ---------------------------------------------------------------------------------------------------------------------------------------
# Build options shared by all game server targets
# ---------------------------------------------------------------------------------------------------------------------------------------
# AION_CHECKED (design §12.5): checked-build runtime checks C1-C16. AUTO = on for Debug and RelWithDebInfo, off for Release/MinSizeRel.
set(AION_CHECKED_MODE "AUTO" CACHE STRING "Game server checked build: AUTO (Debug and RelWithDebInfo), ON or OFF")
set_property(CACHE AION_CHECKED_MODE PROPERTY STRINGS AUTO ON OFF)

add_library(aion_gs_build_options INTERFACE)
if(AION_CHECKED_MODE STREQUAL "ON")
	target_compile_definitions(aion_gs_build_options INTERFACE AION_CHECKED=1)
elseif(AION_CHECKED_MODE STREQUAL "OFF")
	target_compile_definitions(aion_gs_build_options INTERFACE AION_CHECKED=0)
else()
	target_compile_definitions(aion_gs_build_options INTERFACE $<IF:$<CONFIG:Debug,RelWithDebInfo>,AION_CHECKED=1,AION_CHECKED=0>)
endif()
if(MSVC)
	# /bigobj: header-heavy template code (collection shims, schedulers) exceeds the default section limit in Debug
	target_compile_options(aion_gs_build_options INTERFACE /bigobj)
endif()
target_compile_definitions(aion_gs_build_options INTERFACE AION_GAMESERVER_JAVA_DIR="${GS_JAVA_DIR}")

# Adds a kernel static library from source directories (recursive) and single files (globs), relative to src/.
function(aion_gs_add_library target)
	cmake_parse_arguments(ARG "" "" "DIRS;FILES;DEPENDS" ${ARGN})
	set(sources)
	foreach(dir IN LISTS ARG_DIRS)
		file(GLOB_RECURSE dir_sources CONFIGURE_DEPENDS "${GS_SRC}/${dir}/*.cpp" "${GS_SRC}/${dir}/*.h")
		list(APPEND sources ${dir_sources})
	endforeach()
	foreach(pattern IN LISTS ARG_FILES)
		file(GLOB file_sources CONFIGURE_DEPENDS "${GS_SRC}/${pattern}")
		list(APPEND sources ${file_sources})
	endforeach()
	if(sources)
		source_group(TREE "${GS_SRC}" FILES ${sources})
	else()
		set(placeholder "${CMAKE_CURRENT_BINARY_DIR}/${target}_placeholder.cpp")
		file(WRITE "${placeholder}" "// placeholder for ${target}, which has no sources yet\n")
		list(APPEND sources "${placeholder}")
	endif()
	add_library(${target} STATIC ${sources})
	target_include_directories(${target} PUBLIC "${GS_SRC}")
	target_link_libraries(${target} PUBLIC ${ARG_DEPENDS} aion_gs_build_options aion::compiler_options)
endfunction()

# ---------------------------------------------------------------------------------------------------------------------------------------
# Runtime kernel
# ---------------------------------------------------------------------------------------------------------------------------------------
aion_gs_add_library(aion_gs_runtime_base DIRS aion/gameserver/runtime/base DEPENDS aion_commons_core)
aion_gs_add_library(aion_gs_runtime_sync DIRS aion/gameserver/runtime/sync DEPENDS aion_gs_runtime_base)
aion_gs_add_library(aion_gs_runtime_lifetime DIRS aion/gameserver/runtime/lifetime DEPENDS aion_gs_runtime_sync)
aion_gs_add_library(aion_gs_runtime_fields DIRS aion/gameserver/runtime/fields DEPENDS aion_gs_runtime_lifetime)
aion_gs_add_library(aion_gs_runtime_collections DIRS aion/gameserver/runtime/collections DEPENDS aion_gs_runtime_lifetime)
aion_gs_add_library(aion_gs_runtime_sched
	DIRS aion/gameserver/runtime/sched
	FILES aion/gameserver/utils/ThreadPoolManager.h aion/gameserver/utils/ThreadPoolManager.cpp
	DEPENDS aion_gs_runtime_lifetime
)
aion_gs_add_library(aion_gs_runtime_services
	DIRS aion/gameserver/runtime/services aion/gameserver/services/cron aion/gameserver/utils/cron aion/gameserver/utils/idfactory
	DEPENDS aion_gs_runtime_sched aion_gs_runtime_collections aion_gs_runtime_fields aion_commons_configuration
)

add_library(aion_gs_runtime INTERFACE)
target_link_libraries(aion_gs_runtime INTERFACE aion_gs_runtime_services aion_gs_runtime_sched aion_gs_runtime_collections aion_gs_runtime_fields
	aion_gs_runtime_lifetime aion_gs_runtime_sync aion_gs_runtime_base)
add_library(aion::gs_runtime ALIAS aion_gs_runtime)

# ---------------------------------------------------------------------------------------------------------------------------------------
# Tests: one executable per kernel library. Tests see tests/runtime (for shared helpers in tests/runtime/support) and their own directory.
# ---------------------------------------------------------------------------------------------------------------------------------------
foreach(area base sync lifetime fields collections sched services)
	aion_add_tests(aion_gs_runtime_${area}_tests "${GS_TESTS}/runtime/${area}" DEPENDS aion_gs_runtime_${area})
	if(TARGET aion_gs_runtime_${area}_tests)
		target_include_directories(aion_gs_runtime_${area}_tests PRIVATE "${GS_TESTS}/runtime")
	endif()
endforeach()

# Kernel stress harness (P2). Long runs are opt-in (see tests/runtime/stress); the default test run only executes short smoke variants.
aion_add_tests(aion_gs_runtime_stress_tests "${GS_TESTS}/runtime/stress" DEPENDS aion_gs_runtime)
if(TARGET aion_gs_runtime_stress_tests)
	target_include_directories(aion_gs_runtime_stress_tests PRIVATE "${GS_TESTS}/runtime")
endif()

# Benchmarks (P4): not part of ctest.
option(AION_BUILD_BENCH "Build the game server benchmarks (game-server/bench)" ON)
if(AION_BUILD_BENCH)
	add_subdirectory(bench)
endif()
