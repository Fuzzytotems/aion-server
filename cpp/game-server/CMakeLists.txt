# Port of ../game-server (com.aionemu.gameserver): every library comes from the chunk manifest (chunks.cmake, docs/design/handlers-and-porting-plan.md
# §2.2), plus the handler registries, the aion_game_server executable, the header check and the game server tests.
#
# Layout
#   chunks.cmake                              ownership manifest: chunk parts -> globs -> library targets (owned by the integrator)
#   cmake/AionChunks.cmake                    aion_gs_chunk(), the configure-time ownership checks, library and test creation, chunks.json
#   src/aion/gameserver/runtime/<area>/...    kernel code without a Java counterpart, namespace aion::gameserver::runtime (flat, for every area)
#   src/aion/gameserver/<java package>/...    ported Java classes at their Java package paths; src/main.cpp is the executable's entry point
#   generated/aion/gameserver/...             tools/xmlgen output (enums, data structs, member blocks, binders): library aion_gs_staticdata
#   handlers/aion/gameserver/handlers/...     ported data/handlers (include root handlers/; category preludes are the PCH)
#   tests/<dir>/                              one GoogleTest executable per chunk target that has a test directory (aion_gs_chunk TESTS)
#   bench/                                    benchmarks (P4), built only with AION_BUILD_BENCH
#
# Kernel libraries (chunks P4-02a/b) and their dependency direction (arrows point at the dependency; no cycles):
#
#   aion_gs_runtime_base         checked-build macros (AION_CHECKED), Java exception types, TaskInfo, PCT yield points, per-thread context
#        ^                       registry, AION_UNPORTED. Depends on commons core only.
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
#   aion_gs_runtime_services     CleanerQueue/CleanerDrain, LeakCensus, ZombieBreaker, introspection, RuntimeLifecycle, services/cron
#                                (CronExpression, CronService), utils/cron, utils/idfactory (IDFactory)
#
# Linking (§2.2): leaf chunk libraries (DEPENDS in chunks.cmake: runtime, configs, geomath, network crypt, xml, handler registry) link only their
# dependencies and their tests link only them. Geo is not a leaf (see P4-04 in chunks.cmake). Every other core library links aion_gs_core_deps;
# the INTERFACE aion_gs_core links all core libraries, and a non-leaf chunk's tests link aion_gs_core plus aion_gs_registry_empty (a handler
# library's tests also link that library). MSVC resolves symbols across all static libraries of a link, so the (cyclic) references between
# chunk libraries need no link order.

set(GS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(GS_GENERATED "${CMAKE_CURRENT_SOURCE_DIR}/generated")
set(GS_HANDLERS "${CMAKE_CURRENT_SOURCE_DIR}/handlers")
set(GS_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
get_filename_component(GS_JAVA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../game-server" ABSOLUTE)

# ---------------------------------------------------------------------------------------------------------------------------------------------
# Build options shared by all game server targets
# ---------------------------------------------------------------------------------------------------------------------------------------------
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
	# /bigobj: header-heavy template code (collection shims, schedulers) and the static data binders (dataholders.bind.obj has about 79,000
	# sections in Debug) exceed the default section limit
	target_compile_options(aion_gs_build_options INTERFACE /bigobj)
endif()
target_compile_definitions(aion_gs_build_options INTERFACE AION_GAMESERVER_JAVA_DIR="${GS_JAVA_DIR}")
# include roots of every game server target: hand-written sources and the committed generator output (shell headers include X.xml.h/X.xml.inc)
target_include_directories(aion_gs_build_options INTERFACE "${GS_SRC}" "${GS_GENERATED}")

find_package(pugixml CONFIG REQUIRED)
find_package(Python3 3.12 COMPONENTS Interpreter)

# ---------------------------------------------------------------------------------------------------------------------------------------------
# Chunk libraries (chunks.cmake) and the handler registries
# ---------------------------------------------------------------------------------------------------------------------------------------------
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/AionChunks.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/tools/regscan/AionRegscan.cmake") # aion_gs_add_registries(); the tool itself is added below
include("${CMAKE_CURRENT_SOURCE_DIR}/chunks.cmake")
aion_gs_finalize_chunks(
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
	JAVA_DIR "${GS_JAVA_DIR}"
	JSON "${CMAKE_CURRENT_BINARY_DIR}/chunks.json"
	REGISTRY_EMPTY aion_gs_registry_empty
)

# all kernel libraries (the bench and kernel-level tests use it)
add_library(aion_gs_runtime INTERFACE)
target_link_libraries(aion_gs_runtime INTERFACE aion_gs_runtime_services aion_gs_runtime_sched aion_gs_runtime_collections aion_gs_runtime_fields
	aion_gs_runtime_lifetime aion_gs_runtime_sync aion_gs_runtime_base)
add_library(aion::gs_runtime ALIAS aion_gs_runtime)

# Registry tables of HandlerRegistry.h (§1.5), scanned from the handler tree and the client packet directory. The client packets live in the core
# libraries of P5-00 and P5-15/16, whose factories the clientpackets table references.
aion_gs_add_registries(aion_gs_registry
	HANDLERS_ROOT "${GS_HANDLERS}"
	CLIENTPACKETS_ROOT "${GS_SRC}"
	JAVA_HANDLERS "${GS_JAVA_DIR}/data/handlers"
	JAVA_CLIENT_PACKET_FACTORY "${GS_JAVA_DIR}/src/com/aionemu/gameserver/network/aion/AionClientPacketFactory.java"
	HANDLER_LIBRARIES ${AION_GS_HANDLER_TARGETS} aion_gs_login_slice aion_gs_cm_ak aion_gs_cm_lz
)

# The server: every core library, every handler library (through the registries) and main.cpp (MAIN of chunk P5-14).
add_executable(aion_game_server ${AION_GS_MAIN_SOURCES})
source_group(TREE "${GS_SRC}" FILES ${AION_GS_MAIN_SOURCES})
target_link_libraries(aion_game_server PRIVATE aion_gs_core aion_gs_handlers aion_gs_registry aion::compiler_options)
set_target_properties(aion_game_server PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${GS_JAVA_DIR}" CXX_SCAN_FOR_MODULES OFF)
aion_copy_asan_runtime(aion_game_server)

# ---------------------------------------------------------------------------------------------------------------------------------------------
# aion_gs_header_check (§2.5, §3.1 item 7): one translation unit per spine header, compiled with /W4 /WX. Part of ALL: the 519 S0a TUs take
# about 7 s wall in Debug and recompile only when the headers they include change; S0b adds up to 64 hub headers.
# ---------------------------------------------------------------------------------------------------------------------------------------------
file(GLOB_RECURSE gs_fwd_headers CONFIGURE_DEPENDS LIST_DIRECTORIES false RELATIVE "${GS_SRC}" "${GS_SRC}/*/fwd.h")
file(GLOB_RECURSE gs_preludes CONFIGURE_DEPENDS LIST_DIRECTORIES false RELATIVE "${GS_HANDLERS}" "${GS_HANDLERS}/*Prelude.h")
file(GLOB_RECURSE gs_generated_headers CONFIGURE_DEPENDS LIST_DIRECTORIES false RELATIVE "${GS_GENERATED}" "${GS_GENERATED}/aion/*.h")
list(FILTER gs_generated_headers EXCLUDE REGEX "\\.(xml|bind)\\.h$")
set(gs_enum_headers)
foreach(header IN LISTS gs_generated_headers)
	file(STRINGS "${GS_GENERATED}/${header}" enum_line REGEX "^enum class " LIMIT_COUNT 1)
	if(enum_line)
		list(APPEND gs_enum_headers "${header}")
	endif()
endforeach()
list(SORT gs_fwd_headers)
list(SORT gs_preludes)
list(SORT gs_enum_headers)
# Spine step S0b hub headers (docs/design/hub-headers.md): skeleton.py HUBS without the enums xmlgen generates (checked above), plus the kernel
# ports ThreadPoolManager and IDFactory. The list is explicit and tolerates headers that do not exist yet (S0b groups write them in parallel):
# each entry is a CONFIGURE_DEPENDS glob matching exactly that file (`X.[h]`), so a header that appears later re-runs the configure step on
# the next build and joins the check. tools/gen/tests/test_skeleton_tree.py keeps the list equal to HUBS.
set(gs_hub_headers
	aion/gameserver/ai/AI.h
	aion/gameserver/ai/AITemplate.h
	aion/gameserver/ai/AbstractAI.h
	aion/gameserver/ai/NpcAI.h
	aion/gameserver/controllers/CreatureController.h
	aion/gameserver/controllers/NpcController.h
	aion/gameserver/controllers/ObserveController.h
	aion/gameserver/controllers/PlayerController.h
	aion/gameserver/controllers/VisibleObjectController.h
	aion/gameserver/controllers/attack/AggroList.h
	aion/gameserver/controllers/effect/EffectController.h
	aion/gameserver/controllers/movement/CreatureMoveController.h
	aion/gameserver/controllers/observer/ActionObserver.h
	aion/gameserver/dataholders/DataManager.h
	aion/gameserver/instance/handlers/GeneralInstanceHandler.h
	aion/gameserver/instance/handlers/InstanceHandler.h
	aion/gameserver/model/gameobjects/AionObject.h
	aion/gameserver/model/gameobjects/Creature.h
	aion/gameserver/model/gameobjects/Item.h
	aion/gameserver/model/gameobjects/Npc.h
	aion/gameserver/model/gameobjects/Persistable.h
	aion/gameserver/model/gameobjects/Summon.h
	aion/gameserver/model/gameobjects/VisibleObject.h
	aion/gameserver/model/gameobjects/player/Player.h
	aion/gameserver/model/gameobjects/player/PlayerCommonData.h
	aion/gameserver/model/house/House.h
	aion/gameserver/model/items/storage/IStorage.h
	aion/gameserver/model/items/storage/Storage.h
	aion/gameserver/model/stats/calc/Stat2.h
	aion/gameserver/model/stats/container/CreatureGameStats.h
	aion/gameserver/model/stats/container/CreatureLifeStats.h
	aion/gameserver/model/team/TemporaryPlayerTeam.h
	aion/gameserver/model/templates/spawns/SpawnGroup.h
	aion/gameserver/model/templates/spawns/SpawnTemplate.h
	aion/gameserver/network/aion/AionClientPacket.h
	aion/gameserver/network/aion/AionConnection.h
	aion/gameserver/network/aion/AionServerPacket.h
	aion/gameserver/questEngine/QuestEngine.h
	aion/gameserver/questEngine/handlers/AbstractQuestHandler.h
	aion/gameserver/questEngine/model/QuestEnv.h
	aion/gameserver/questEngine/model/QuestState.h
	aion/gameserver/skillengine/SkillEngine.h
	aion/gameserver/skillengine/effect/EffectTemplate.h
	aion/gameserver/skillengine/model/Effect.h
	aion/gameserver/skillengine/model/Skill.h
	aion/gameserver/skillengine/model/SkillTemplate.h
	aion/gameserver/spawnengine/SpawnEngine.h
	aion/gameserver/utils/PacketSendUtility.h
	aion/gameserver/utils/ThreadPoolManager.h
	aion/gameserver/utils/chathandlers/AdminCommand.h
	aion/gameserver/utils/chathandlers/ChatCommand.h
	aion/gameserver/utils/chathandlers/ConsoleCommand.h
	aion/gameserver/utils/chathandlers/PlayerCommand.h
	aion/gameserver/utils/idfactory/IDFactory.h
	aion/gameserver/world/MapRegion.h
	aion/gameserver/world/World.h
	aion/gameserver/world/WorldMapInstance.h
	aion/gameserver/world/WorldPosition.h
	aion/gameserver/world/geo/GeoService.h
	aion/gameserver/world/knownlist/KnownList.h
	aion/gameserver/world/zone/ZoneInstance.h
	aion/gameserver/world/zone/ZoneName.h
	aion/gameserver/world/zone/handler/QuestZoneHandler.h
	aion/gameserver/world/zone/handler/ZoneHandler.h
)
# C++-only spine headers the hubs and HandlerRegistry.h depend on (skeleton.py SPINE_HEADERS, hub-headers.md §3.4): frozen with the spine and
# checked alone the same way.
set(gs_spine_headers
	aion/gameserver/model/Expirable.h
	aion/gameserver/model/GameEngine.h
	aion/gameserver/model/gameobjects/player/LogoutBreakers.h
	aion/gameserver/model/stats/calc/StatOwner.h
	aion/gameserver/model/team/GeneralTeam.h
	aion/gameserver/model/team/TeamMember.h
	aion/gameserver/model/templates/L10n.h
	aion/gameserver/network/aion/SerializedBody.h
	aion/gameserver/network/aion/StateSet.h
	aion/gameserver/world/zone/handler/GeneralZoneHandler.h
)
set(gs_existing_hub_headers)
foreach(header IN LISTS gs_hub_headers gs_spine_headers)
	string(REGEX REPLACE "\\.h$" ".[h]" pattern "${header}")
	file(GLOB found CONFIGURE_DEPENDS LIST_DIRECTORIES false RELATIVE "${GS_SRC}" "${GS_SRC}/${pattern}")
	list(APPEND gs_existing_hub_headers ${found})
endforeach()
aion_gs_header_check(aion_gs_header_check
	HEADERS ${gs_fwd_headers} ${gs_enum_headers} ${gs_preludes} ${gs_existing_hub_headers}
		aion/gameserver/handlers/HandlerRegistry.h aion/gameserver/runtime/base/Unported.h aion/gameserver/runtime/services/RuntimeLifecycle.h
	INCLUDE_DIRECTORIES "${GS_HANDLERS}"
	LINK aion_gs_core_deps
)
list(LENGTH gs_fwd_headers gs_fwd_count)
list(LENGTH gs_enum_headers gs_enum_count)
list(LENGTH gs_preludes gs_prelude_count)
list(LENGTH gs_hub_headers gs_hub_count)
list(LENGTH gs_spine_headers gs_spine_count)
math(EXPR gs_hub_count "${gs_hub_count} + ${gs_spine_count}")
list(LENGTH gs_existing_hub_headers gs_existing_hub_count)
message(STATUS "aion_gs_header_check: ${gs_fwd_count} forward headers, ${gs_enum_count} generated enum headers, ${gs_prelude_count} preludes, "
	"3 runtime headers, ${gs_existing_hub_count} of ${gs_hub_count} hub and spine headers")

# ---------------------------------------------------------------------------------------------------------------------------------------------
# Tests that are not the unit tests of one chunk target
# ---------------------------------------------------------------------------------------------------------------------------------------------
# Kernel stress harness (P2). Long runs are opt-in (see tests/runtime/stress); the default test run only executes short smoke variants.
aion_add_tests(aion_gs_runtime_stress_tests "${GS_TESTS}/runtime/stress" DEPENDS aion_gs_runtime aion_gs_build_options)
if(TARGET aion_gs_runtime_stress_tests)
	target_include_directories(aion_gs_runtime_stress_tests PRIVATE "${GS_TESTS}/runtime")
endif()

if(AION_BUILD_TESTS)
	# Tests that read the Java tree (config/, src/, data/): label realdata, and pass the tree's location in the environment as well
	set(gs_realdata PROPERTIES LABELS realdata ENVIRONMENT "AION_GAMESERVER_JAVA_DIR=${GS_JAVA_DIR}")
	aion_set_discovered_test_properties(aion_gs_configs_tests REGEX "^(ConfigLoadTest|JavaAnnotationsTest)\\." ${gs_realdata})
	aion_set_discovered_test_properties(aion_gs_runtime_services_tests REGEX "^CronExpressionTest\\.EveryJavaServerCronStringParsesAndMatchesTheOracle$"
		${gs_realdata})
	aion_set_discovered_test_properties(aion_gs_xml_tests
		REGEX "^(StaticDataImportsTest\\.RealStaticDataTreeResolvesLikeXmlMerger|StaticDataRealFilesTest\\..*|GeneratedSliceTest\\..*)$" ${gs_realdata})

	# S0a link proof: the server starts against a database and stops at the unported DataManager (cmake/RunStartupSmoke.cmake). Like the other
	# database tests it is skipped unless the environment names the database: AION_TEST_GS_DATABASE_URL, e.g.
	# jdbc:mysql://127.0.0.1:3306/aion_cpp_test?characterEncoding=UTF-8 (the database must exist; user root without password unless
	# AION_TEST_GS_DATABASE_USER / AION_TEST_GS_DATABASE_PASSWORD are set).
	add_test(NAME gs.smoke.startup
		COMMAND "${CMAKE_COMMAND}" "-DEXECUTABLE=$<TARGET_FILE:aion_game_server>" "-DWORKING_DIRECTORY=${GS_JAVA_DIR}"
			-P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunStartupSmoke.cmake")
	set_tests_properties(gs.smoke.startup PROPERTIES LABELS "smoke;realdata" TIMEOUT 180 SKIP_REGULAR_EXPRESSION "gs\\.smoke\\.startup: skipped")

	# the configure-time empty registry tables (linked by the unit tests) equal the tool's
	list(JOIN AION_GS_REGISTRY_NAMES "," gs_registry_names)
	add_test(NAME gs.registry.empty_tables
		COMMAND "${CMAKE_COMMAND}" "-DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/aion_gs_registry" "-DNAMES=${gs_registry_names}"
			-P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompareEmptyRegistries.cmake")
endif()

# drift check of generated test data that lives outside cpp/tools (cpp/tools/CMakeLists.txt covers the tool suites)
if(AION_BUILD_TESTS AND Python3_Interpreter_FOUND)
	add_test(NAME gs.geomath.golden_drift COMMAND "${Python3_EXECUTABLE}" oracle/gen_golden.py --check WORKING_DIRECTORY "${GS_TESTS}/geomath")
	# concurrency lint (runtime-architecture.md §12.2) over all game server sources; warnings fail too, the RR-16 lock class check included
	# (a warning since the spine freeze). L16: every cycle edge outside game-server/data/handlers has a cycles.toml resolution (the handler edges belong to
	# the phase-6 chunks; switch to --cycles once they are resolved)
	add_test(NAME gs.lint.concurrency
		COMMAND "${Python3_EXECUTABLE}" tools/porting/lint_concurrency.py --werror --cycles=core game-server/src WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
	# tools/porting/chunks.py computes the same ownership as the configure step (its chunks.json)
	add_test(NAME gs.chunks.consistency
		COMMAND "${Python3_EXECUTABLE}" tools/porting/chunks.py verify-json "${CMAKE_CURRENT_BINARY_DIR}/chunks.json" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
	set_tests_properties(gs.chunks.consistency ${gs_realdata}) # it scans the Java tree
endif()

# C++ build tools of the game server (tools/regscan)
file(GLOB gs_tool_dirs LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/tools/*")
foreach(dir IN LISTS gs_tool_dirs)
	if(EXISTS "${dir}/CMakeLists.txt")
		add_subdirectory("${dir}")
	endif()
endforeach()

if(AION_BUILD_TESTS AND TARGET aion_gs_regscan_tests)
	set(gs_regscan_dir "${CMAKE_CURRENT_SOURCE_DIR}/tools/regscan")
	aion_set_discovered_test_properties(aion_gs_regscan_tests DIRECTORY "${gs_regscan_dir}" REGEX "^RealJavaTreeTest\\." ${gs_realdata})
	if(IS_DIRECTORY "${GS_JAVA_DIR}/data/handlers")
		set_tests_properties(aion_gs_regscan.cli.java_tree DIRECTORY "${gs_regscan_dir}" ${gs_realdata})
	endif()
endif()

# Benchmarks (P4): not part of ctest.
option(AION_BUILD_BENCH "Build the game server benchmarks (game-server/bench)" ON)
if(AION_BUILD_BENCH)
	add_subdirectory(bench)
endif()
