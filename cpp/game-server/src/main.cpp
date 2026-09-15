// Entry point of the game server (Java: com.aionemu.gameserver.GameServer.main).
//
// Milestone M4 (docs/design/handlers-and-porting-plan.md §2.7): the executable links every chunk library and the handler registries and runs the
// ported part of Java's startup in Java order: Logging, Config.load, DatabaseFactory.init, PlayerDAO.setAllPlayersOffline (and
// DatabaseCleaningService if enabled), the runtime kernel (ThreadPoolManager, CronService, IDFactory with the used ids of the eight DAOs, and the
// C++-only Reclaimer, LeakCensus, CleanerQueue and Watchdog; RuntimeLifecycle), DataManager (all static data), ZoneService.init and
// GeoService.init (Java initializes them in a parallel stream together with the handler engines QuestEngine, AIEngine, InstanceEngine and
// ChatProcessor, which are not part of M4 and not called; C++ runs the two in the stream's source order) and World (all world maps with their
// instances and zone instances). The startup then ends: the kernel shuts down in order and the process exits with ExitCode::NORMAL; an
// AION_UNPORTED on the way is logged and ends it with ExitCode::ERROR_. P5-14 (aion_gs_app) replaces this file's body with GameServer::main and
// the ShutdownHook.
//
// Run it with ../game-server (the Java module directory) as working directory, so ./config, ./data and ./log resolve like for the Java server.
// Configuration properties can be overridden with -D<key>=<value> arguments; they are applied over config/mygs.properties.
//
// C++-only check modes (M4 gate, CTest gs.m4.check_static_data, cmake/RunM4Check.cmake):
//   --check-static-data        the same startup path, plus the M4 report files in the check output directory:
//                              static_data_counts.txt  the "Loaded N ..." lines of StaticData (a file sink on its logger, message only)
//                              static_data_extras.txt  counts Java does not log (XMLQuests distinct quest ids)
//                              geo_statistics.txt      GeoWorldLoader statistics of GeoService.init ("key value" lines)
//                              material_zone_names.txt the material zone names GeoWorldLoader handed to ZoneService (sorted)
//                              geo_probes_actual.txt   GeoService.getZ of every probe of --check-geo-probes (float bits)
//                              world_zones.txt         per world map: its instances and the zone names of every instance
//                              unported_trace.txt      the AION_UNPORTED trace (runtime/base/Unported.h)
//                              m4_summary.txt          IDs used, load time, peak working set, unported hits, the duration of each startup
//                                                      step and the Reclaimer backlog high-water mark (informational)
//                              startup_timeline.txt    the log lines of the startup steps with millisecond timestamps (informational)
//   --check-id-factory         stops after the runtime start (IDFactory initialized from the database), writes m4_summary.txt
//   --check-output=<dir>       the check output directory (default ./log/m4)
//   --check-geo-probes=<file>  getZ probes, one per line: "mapId instanceId xBits yBits zMaxBits zMinBits" (python -m geo m4-probes)
//
// Helper mode: --write-minidump <pid> <file> as the only arguments writes a snapshot minidump of process <pid> and exits (the watchdog starts
// its own executable this way, runtime/sync/MinidumpWriter.h).

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/configuration/Properties.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/logging/FileAppender.h"
#include "aion/commons/logging/Logger.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/logging/Logging.h"
#include "aion/commons/logging/PatternLayout.h"
#include "aion/commons/utils/ExitCode.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/concurrent/ThreadName.h"
#include "aion/commons/utils/concurrent/UncaughtExceptionHandler.h"
#include "aion/gameserver/configs/Config.h"
#include "aion/gameserver/configs/main/CleaningConfig.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/configs/main/RuntimeConfig.h"
#include "aion/gameserver/dao/GuideDAO.h"
#include "aion/gameserver/dao/HousesDAO.h"
#include "aion/gameserver/dao/InventoryDAO.h"
#include "aion/gameserver/dao/LegionDAO.h"
#include "aion/gameserver/dao/MailDAO.h"
#include "aion/gameserver/dao/PlayerDAO.h"
#include "aion/gameserver/dao/PlayerPetsDAO.h"
#include "aion/gameserver/dao/PlayerRegisteredItemsDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/XMLQuests.h"
#include "aion/gameserver/geoEngine/GeoCallbacks.h"
#include "aion/gameserver/geoEngine/GeoWorldLoader.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/questEngine/handlers/models/XMLQuest.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/services/RuntimeLifecycle.h"
#include "aion/gameserver/runtime/sync/MinidumpWriter.h"
#include "aion/gameserver/runtime/sync/Watchdog.h"
#include "aion/gameserver/services/DatabaseCleaningService.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/world/zone/ZoneName.h"
#include "aion/gameserver/world/zone/ZoneService.h"

#include <Windows.h>
#include <psapi.h>

#include "aion/commons/utils/WindowsMacroGuard.h"

namespace {

using aion::commons::configuration::Properties;
using aion::gameserver::runtime::RuntimeLifecycle;
namespace Logging = aion::commons::logging::Logging;
namespace LoggerFactory = aion::commons::logging::LoggerFactory;
namespace UncaughtExceptionHandler = aion::commons::utils::concurrent::UncaughtExceptionHandler;

const aion::commons::logging::Logger& log() {
	static const auto* logger = new aion::commons::logging::Logger(LoggerFactory::getLogger("com.aionemu.gameserver.GameServer"));
	return *logger;
}

/** The command line: -Dkey=value overrides (like the login server) and the C++-only check options (see the file comment). */
struct Arguments {
	Properties overrides;
	std::vector<std::string> unknown;
	bool checkStaticData = false;
	bool checkIdFactory = false;
	std::filesystem::path checkOutput = "./log/m4";
	std::optional<std::filesystem::path> geoProbes;

	bool checkMode() const { return checkStaticData || checkIdFactory; }
};

Arguments parseArguments(int argc, char* argv[]) {
	Arguments arguments;
	for (int i = 1; i < argc; i++) {
		std::string_view arg = argv[i];
		size_t separator = arg.find('=');
		if (arg.starts_with("-D") && separator != std::string_view::npos && separator > 2)
			arguments.overrides.setProperty(std::string(arg.substr(2, separator - 2)), std::string(arg.substr(separator + 1)));
		else if (arg == "--check-static-data")
			arguments.checkStaticData = true;
		else if (arg == "--check-id-factory")
			arguments.checkIdFactory = true;
		else if (arg.starts_with("--check-output="))
			arguments.checkOutput = std::filesystem::path(std::string(arg.substr(std::string_view("--check-output=").size())));
		else if (arg.starts_with("--check-geo-probes="))
			arguments.geoProbes = std::filesystem::path(std::string(arg.substr(std::string_view("--check-geo-probes=").size())));
		else
			arguments.unknown.emplace_back(arg);
	}
	return arguments;
}

/** The kernel configuration from the loaded configs (RuntimeLifecycle::Options documents the keys). */
RuntimeLifecycle::Options runtimeOptions() {
	using aion::gameserver::configs::main::GSConfig;
	using aion::gameserver::configs::main::RuntimeConfig;
	namespace dao = aion::gameserver::dao;
	RuntimeLifecycle::Options options;
	options.reclaimer = RuntimeConfig::reclaimerConfig();
	options.leakCensus = RuntimeConfig::leakCensusConfig();
	options.watchdog = RuntimeConfig::watchdogConfig();
	options.threadPool = RuntimeConfig::threadPoolManagerConfig();
	options.idFactory = RuntimeConfig::idFactoryConfig();
	options.cronTimeZone = GSConfig::TIME_ZONE_ID.load();
	// Java IDFactory.initializeUsedIds order
	options.usedIds = {
		{"PlayerDAO", [] { return dao::PlayerDAO::getUsedIDs(); }},
		{"InventoryDAO", [] { return dao::InventoryDAO::getUsedIDs(); }},
		{"PlayerRegisteredItemsDAO", [] { return dao::PlayerRegisteredItemsDAO::getUsedIDs(); }},
		{"LegionDAO", [] { return dao::LegionDAO::getUsedIDs(); }},
		{"MailDAO", [] { return dao::MailDAO::getUsedIDs(); }},
		{"GuideDAO", [] { return dao::GuideDAO::getUsedIDs(); }},
		{"HousesDAO", [] { return dao::HousesDAO::getUsedIDs(); }},
		{"PlayerPetsDAO", [] { return dao::PlayerPetsDAO::getUsedIDs(); }},
	};
	return options;
}

/** The material zone names GeoWorldLoader hands to ZoneService while GeoService.init loads the geo data (check mode only). */
struct MaterialZoneNames {
	std::mutex mutex; // confined: main.cpp check mode only; the listener runs on the ForkJoin threads of the geo load
	std::vector<std::string> names;
};

MaterialZoneNames& materialZoneNames() {
	static auto* names = new MaterialZoneNames(); // lint: L5 check mode state of main, created before the geo load and never destroyed
	return *names;
}

void recordMaterialZone(aion::gameserver::geoEngine::scene::Spatial&, int32_t, std::string_view zoneName) {
	MaterialZoneNames& state = materialZoneNames();
	std::scoped_lock lock(state.mutex);
	state.names.emplace_back(zoneName);
}

std::ofstream openOutput(const std::filesystem::path& file) {
	std::ofstream out(file, std::ios::binary | std::ios::trunc);
	if (!out)
		throw aion::commons::utils::IOException("Cannot write " + file.string());
	return out;
}

/** Java Float.floatToRawIntBits as an unsigned decimal */
std::string floatBits(float value) {
	return std::to_string(std::bit_cast<uint32_t>(value));
}

float parseFloatBits(std::string_view text) {
	return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(std::string(text))));
}

/**
 * Check mode measurements: the duration of each startup step and the Reclaimer backlog high-water mark, sampled by a watchdog probe (every
 * watchdog period) and at the end of each step inside its scope (the main thread's own unflushed retire list is not part of the backlog).
 */
struct StartupMeasurements {
	std::atomic<const char*> step{"runtime start"};
	std::atomic<uint64_t> maxBacklogObjects{0};
	std::atomic<uint64_t> maxBacklogBytes{0};
	std::atomic<const char*> maxBacklogStep{"none"};
	std::mutex mutex; // confined: main.cpp check mode only
	std::vector<std::pair<std::string, int64_t>> stepMillis;
	std::vector<std::string> stepBacklogs;
};

StartupMeasurements& measurements() {
	static auto* state = new StartupMeasurements(); // lint: L5 check mode state of main, read by the watchdog probe and never destroyed
	return *state;
}

void sampleBacklog() {
	aion::gameserver::runtime::Reclaimer::Stats stats = aion::gameserver::runtime::Reclaimer::getInstance().stats();
	StartupMeasurements& m = measurements();
	uint64_t previous = m.maxBacklogObjects.load(std::memory_order_acquire);
	while (stats.backlog > previous && !m.maxBacklogObjects.compare_exchange_weak(previous, stats.backlog, std::memory_order_acq_rel))
		;
	if (stats.backlog >= previous)
		m.maxBacklogStep.store(m.step.load(std::memory_order_acquire), std::memory_order_release);
	uint64_t previousBytes = m.maxBacklogBytes.load(std::memory_order_acquire);
	while (stats.backlogBytes > previousBytes && !m.maxBacklogBytes.compare_exchange_weak(previousBytes, stats.backlogBytes, std::memory_order_acq_rel))
		;
}

/** Runs one startup step in its own STARTUP task scope (see startup()); in check mode it records the step's duration and backlog. */
template <class Body>
void runStartupStep(const Arguments& arguments, const aion::gameserver::runtime::TaskInfo& info, const char* name, Body&& body) {
	const auto start = std::chrono::steady_clock::now();
	measurements().step.store(name, std::memory_order_release);
	uint64_t backlogAtEnd = 0;
	{
		aion::gameserver::runtime::TaskScope scope(info);
		body();
		if (arguments.checkMode()) {
			sampleBacklog();
			backlogAtEnd = aion::gameserver::runtime::Reclaimer::getInstance().stats().backlog;
		}
	}
	if (!arguments.checkMode())
		return;
	int64_t millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	log().info("M4 check: startup step '{}' took {} ms (Reclaimer backlog {} objects at its end)", name, millis, backlogAtEnd);
	StartupMeasurements& m = measurements();
	std::scoped_lock lock(m.mutex);
	m.stepMillis.emplace_back(name, millis);
	m.stepBacklogs.push_back(std::to_string(backlogAtEnd));
}

size_t peakWorkingSetBytes() {
	PROCESS_MEMORY_COUNTERS counters{};
	counters.cb = sizeof(counters);
	if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
		return 0;
	return counters.PeakWorkingSetSize;
}

/** Writes the extra count, geo and world report files of --check-static-data (after World was created). */
void writeGeoAndWorldReports(const Arguments& arguments) {
	using aion::gameserver::dataholders::DataManager;
	namespace world = aion::gameserver::world;
	const std::filesystem::path& dir = arguments.checkOutput;

	{
		// the count Java does not log (tools/oracle static_data_counts.json "extras"): XMLQuests distinct quest ids
		std::set<int32_t> questIds;
		for (const aion::gameserver::questEngine::handlers::models::XMLQuest* quest : DataManager::XML_QUESTS->getAllQuests())
			questIds.insert(quest->getId());
		std::ofstream out = openOutput(dir / "static_data_extras.txt");
		out << "xml_quests " << questIds.size() << '\n';
	}

	aion::gameserver::geoEngine::GeoWorldLoader::Statistics s = aion::gameserver::geoEngine::GeoWorldLoader::getLastLoadStatistics();
	{
		std::ofstream out = openOutput(dir / "geo_statistics.txt");
		out << "meshEntries " << s.meshEntries << "\nmeshes " << s.meshes << "\nmeshNames " << s.meshNames << "\ngeoFiles " << s.geoFiles
			<< "\nplacements " << s.placements << "\nmissingMeshPlacements " << s.missingMeshPlacements << "\nplacementGeometries "
			<< s.placementGeometries << "\nattachedNodes " << s.attachedNodes << "\ngeometries " << s.geometries << "\nmaterialGeometries "
			<< s.materialGeometries << "\nterrainMaps " << s.terrainMaps << "\ncollisionTrees " << s.collisionTrees << "\nworldMaps "
			<< DataManager::WORLD_MAPS_DATA->size() << "\n";
	}
	{
		std::vector<std::string> names;
		{
			MaterialZoneNames& state = materialZoneNames();
			std::scoped_lock lock(state.mutex);
			names = state.names;
		}
		std::sort(names.begin(), names.end());
		std::ofstream out = openOutput(dir / "material_zone_names.txt");
		for (const std::string& name : names)
			out << name << '\n';
	}
	if (arguments.geoProbes) {
		std::ifstream in(*arguments.geoProbes, std::ios::binary);
		if (!in)
			throw aion::commons::utils::IOException("Cannot read " + arguments.geoProbes->string());
		std::ofstream out = openOutput(dir / "geo_probes_actual.txt");
		std::string line;
		int32_t probes = 0;
		while (std::getline(in, line)) {
			if (line.empty() || line.starts_with('#'))
				continue;
			std::istringstream fields(line);
			int32_t mapId = 0;
			int32_t instanceId = 0;
			std::string x, y, zMax, zMin;
			if (!(fields >> mapId >> instanceId >> x >> y >> zMax >> zMin))
				throw aion::commons::utils::IllegalArgumentException("Invalid probe line: " + line);
			float z = world::geo::GeoService::getInstance().getZ(mapId, parseFloatBits(x), parseFloatBits(y), parseFloatBits(zMax), parseFloatBits(zMin),
				instanceId);
			out << mapId << ' ' << instanceId << ' ' << x << ' ' << y << ' ' << zMax << ' ' << zMin << ' ' << floatBits(z) << '\n';
			probes++;
		}
		log().info("M4 check: {} getZ probes evaluated", probes);
	}
	{
		std::ofstream out = openOutput(dir / "world_zones.txt");
		int32_t maps = 0;
		size_t zoneInstances = 0;
		for (const aion::gameserver::model::templates::world::WorldMapTemplate* mapTemplate : *DataManager::WORLD_MAPS_DATA) {
			aion::gameserver::runtime::Ptr<world::WorldMap> map = world::World::getInstance().getWorldMap(mapTemplate->getMapId());
			if (!map) {
				out << "map " << mapTemplate->getMapId() << " missing\n";
				continue;
			}
			maps++;
			std::vector<aion::gameserver::runtime::Ptr<world::WorldMapInstance>> instances;
			for (aion::gameserver::runtime::Ptr<world::WorldMapInstance> instance : *map)
				instances.push_back(instance);
			std::sort(instances.begin(), instances.end(), [](const auto& a, const auto& b) { return a->getInstanceId() < b->getInstanceId(); });
			out << "map " << map->getMapId() << " instances " << instances.size() << '\n';
			for (const aion::gameserver::runtime::Ptr<world::WorldMapInstance>& instance : instances) {
				std::vector<std::string> names;
				for (const world::zone::ZoneName* zoneName : instance->getZoneNames())
					names.push_back(zoneName->name());
				std::sort(names.begin(), names.end());
				zoneInstances += names.size();
				out << "instance " << instance->getInstanceId() << " zones " << names.size() << '\n';
				for (const std::string& name : names)
					out << "zone " << name << '\n';
			}
		}
		log().info("M4 check: {} world maps with {} zone instances in their map instances", maps, zoneInstances);
	}
}

void writeSummary(const Arguments& arguments, int64_t startupMillis) {
	std::ofstream out = openOutput(arguments.checkOutput / "m4_summary.txt");
	out << "idsUsed " << aion::gameserver::utils::idfactory::IDFactory::getInstance().getUsedCount() << '\n';
	out << "startupMillis " << startupMillis << '\n';
	out << "peakWorkingSetBytes " << peakWorkingSetBytes() << '\n';
	out << "unportedHits " << aion::gameserver::runtime::unportedHitCount() << '\n';
	StartupMeasurements& m = measurements();
	std::scoped_lock lock(m.mutex);
	for (size_t i = 0; i < m.stepMillis.size(); ++i) {
		std::string key = m.stepMillis[i].first;
		std::replace(key.begin(), key.end(), ' ', '_');
		out << "stepMillis." << key << ' ' << m.stepMillis[i].second << '\n';
		out << "stepEndBacklogObjects." << key << ' ' << m.stepBacklogs[i] << '\n';
	}
	out << "reclaimerBacklogMaxObjects " << m.maxBacklogObjects.load(std::memory_order_acquire) << '\n';
	out << "reclaimerBacklogMaxBytes " << m.maxBacklogBytes.load(std::memory_order_acquire) << '\n';
	out << "reclaimerBacklogMaxStep " << std::string(m.maxBacklogStep.load(std::memory_order_acquire)) << '\n';
}

/** loggers whose lines go to startup_timeline.txt in check mode */
constexpr std::array<const char*, 5> TIMELINE_LOGGERS{"com.aionemu.gameserver.GameServer", "com.aionemu.gameserver.dataholders.DataManager",
	"com.aionemu.gameserver.world.zone.ZoneService", "com.aionemu.gameserver.geoEngine.GeoWorldLoader", "com.aionemu.gameserver.world.World"};

void startup(const Arguments& arguments, std::chrono::steady_clock::time_point processStart) {
	using aion::gameserver::configs::Config;
	Logging::init(Config::loadLoggingConfig()); // must run before anything logs to the files

	if (arguments.checkMode()) {
		std::filesystem::create_directories(arguments.checkOutput);
		// the "Loaded N ..." lines of StaticData.afterUnmarshal, as the holders logged them (M4 item 4); additive: they stay in the console log
		const std::filesystem::path counts = arguments.checkOutput / "static_data_counts.txt";
		std::filesystem::remove(counts);
		auto sink = std::make_shared<aion::commons::logging::FileAppender>(counts, std::make_unique<aion::commons::logging::PatternLayout>("%msg%n"));
		LoggerFactory::configure("com.aionemu.gameserver.dataholders.StaticData", {.sinks = {sink}, .additive = true});
		// informational: the step log lines with millisecond timestamps (the console pattern has seconds only)
		const std::filesystem::path timelineFile = arguments.checkOutput / "startup_timeline.txt";
		std::filesystem::remove(timelineFile);
		auto timeline = std::make_shared<aion::commons::logging::FileAppender>(timelineFile,
			std::make_unique<aion::commons::logging::PatternLayout>("%d{HH:mm:ss.SSS} [%thread] %logger{0} - %msg%n"));
		for (const char* name : TIMELINE_LOGGERS)
			LoggerFactory::configure(name, {.sinks = {timeline}, .additive = true});
	}

	// C++ addition: command line overrides are layered where Java layers the active events' properties (over mygs.properties), so they also
	// survive later Config::load calls. EventService (P5-12b) must keep them when it registers its provider.
	if (!arguments.overrides.isEmpty()) {
		Properties overrides = arguments.overrides;
		Config::setEventConfigPropertiesProvider([overrides] { return overrides; });
	}
	Config::load();
	if (!arguments.overrides.isEmpty()) {
		std::set<std::string> keys = arguments.overrides.stringPropertyNames();
		log().info("Override properties from the command line (unknown keys are warned above): " +
			aion::commons::utils::StringUtils::join({keys.begin(), keys.end()}, ", "));
	}
	for (const std::string& argument : arguments.unknown)
		log().warn("Unknown command line argument ignored: " + argument);

	// Java: DatabaseFactory.init(); PlayerDAO.setAllPlayersOffline(); if (CleaningConfig.CLEANING_ENABLE) DatabaseCleaningService.deletePlayers...
	aion::commons::database::DatabaseFactory::init(aion::commons::database::DatabaseFactory::gameServerOptions());
	aion::gameserver::dao::PlayerDAO::setAllPlayersOffline();
	if (aion::gameserver::configs::main::CleaningConfig::CLEANING_ENABLE.load()) {
		aion::gameserver::runtime::TaskScope scope(AION_TASK_INFO(aion::gameserver::runtime::TaskKind::STARTUP));
		aion::gameserver::services::DatabaseCleaningService::deletePlayersOnInactiveAccounts();
	}

	// Java: ThreadPoolManager.getInstance(); CronService.initSingleton(...); IDFactory.getInstance()
	RuntimeLifecycle::start(runtimeOptions());
	if (arguments.checkIdFactory && !arguments.checkStaticData) {
		writeSummary(arguments, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - processStart).count());
		log().info("M4 check: IDFactory initialized, --check-id-factory ends the startup here");
		return;
	}

	// One STARTUP task scope per step, so the Reclaimer frees what a step unlinked before the next one starts (the geo load alone retires more
	// than a million objects); nothing borrowed in one step is used in the next.
	using aion::gameserver::runtime::TaskKind;
	uint64_t backlogProbe = 0;
	if (arguments.checkMode())
		backlogProbe = aion::gameserver::runtime::Watchdog::getInstance().addProbe("M4 check: Reclaimer backlog high-water mark",
			[](aion::gameserver::runtime::Watchdog&, const std::vector<aion::gameserver::runtime::Watchdog::ThreadSnapshot>&) { sampleBacklog(); });
	runStartupStep(arguments, AION_TASK_INFO(TaskKind::STARTUP), "static data", [] { aion::gameserver::dataholders::DataManager::getInstance(); });
	// Java: Stream.of(QuestEngine, AIEngine, InstanceEngine, ChatProcessor, ZoneService, GeoService).parallel().forEach(GameEngine::init)
	// M4: the handler engines are not part of the milestone (their registries are empty); ZoneService and GeoService in the stream's order
	runStartupStep(arguments, AION_TASK_INFO(TaskKind::STARTUP), "zones", [] { aion::gameserver::world::zone::ZoneService::getInstance().init(); });
	runStartupStep(arguments, AION_TASK_INFO(TaskKind::STARTUP), "geo", [&arguments] {
		if (arguments.checkStaticData)
			aion::gameserver::geoEngine::GeoCallbacks::setMaterialZoneListener(&recordMaterialZone);
		aion::gameserver::world::geo::GeoService::getInstance().init();
		aion::gameserver::geoEngine::GeoCallbacks::setMaterialZoneListener(nullptr);
	});
	runStartupStep(arguments, AION_TASK_INFO(TaskKind::STARTUP), "world", [] {
		// World may unpublish the step while its maps are created in their own task scopes (World::World)
		aion::gameserver::runtime::QuiescentScope quiescent; // quiescent-safe: this frame and runStartupStep hold no borrow (values only)
		aion::gameserver::runtime::QuiescentOptIn worldCreation(aion::gameserver::runtime::QuiescentOptIn::WORLD_CREATION);
		aion::gameserver::world::World::getInstance();
	});
	if (backlogProbe != 0)
		aion::gameserver::runtime::Watchdog::getInstance().removeProbe(backlogProbe);
	int64_t startupMillis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - processStart).count();
	log().info("M4 startup path (Config, database, runtime, static data, zones, geo, world) completed in {} ms, peak working set {} MB", startupMillis,
		peakWorkingSetBytes() / (1024 * 1024));
	if (arguments.checkStaticData) {
		aion::gameserver::runtime::TaskScope scope(AION_TASK_INFO(TaskKind::STARTUP));
		writeGeoAndWorldReports(arguments);
		writeSummary(arguments, startupMillis);
	}
	log().info("M4 startup sequence complete (the rest of GameServer.main is not ported yet)");
}

void shutdown(const Arguments& arguments) noexcept {
	try {
		RuntimeLifecycle::ShutdownReport report = RuntimeLifecycle::shutdown(); // outside any TaskScope
		if (report.performed)
			log().info("Runtime shut down: {} tasks left, {} cleaner ids drained, reclaimer backlog {}, {} objects still tracked", report.tasksLeft,
				report.cleanerIdsDrained, report.reclaimerBacklog, report.censusTracked);
		if (aion::commons::database::DatabaseFactory::isInitialized())
			aion::commons::database::DatabaseFactory::shutdown();
		if (arguments.checkMode()) {
			std::filesystem::create_directories(arguments.checkOutput);
			std::ofstream out = openOutput(arguments.checkOutput / "unported_trace.txt");
			aion::gameserver::runtime::writeUnportedTrace(out);
			log().info("M4 check: {} AION_UNPORTED hits, trace written to {}", aion::gameserver::runtime::unportedHitCount(),
				(arguments.checkOutput / "unported_trace.txt").string());
		}
	} catch (...) {
		UncaughtExceptionHandler::uncaughtException("main", std::current_exception());
	}
	LoggerFactory::removeConfig("com.aionemu.gameserver.dataholders.StaticData");
	for (const char* name : TIMELINE_LOGGERS)
		LoggerFactory::removeConfig(name);
	LoggerFactory::flushAll();
	Logging::shutdown();
}

} // namespace

int main(int argc, char* argv[]) {
	// C++ addition: the watchdog's minidump helper (runtime/sync/MinidumpWriter.h) is this executable started with --write-minidump <pid> <file>
	if (std::optional<int> helperExitCode = aion::gameserver::runtime::MinidumpWriter::runIfRequested(argc, argv))
		return *helperExitCode;
	const auto processStart = std::chrono::steady_clock::now();
	aion::commons::utils::concurrent::setCurrentThreadName("main"); // Java: the main thread's name in log lines
	UncaughtExceptionHandler::install();
	Arguments arguments = parseArguments(argc, argv);
	int exitCode = aion::commons::utils::ExitCode::ERROR_;
	try {
		startup(arguments, processStart);
		exitCode = aion::commons::utils::ExitCode::NORMAL;
	} catch (const aion::gameserver::runtime::UnportedException& e) {
		// the site was already logged with its stack trace by AION_UNPORTED
		log().error("Game server startup stopped at an unported function: " + std::string(e.what()));
	} catch (...) {
		UncaughtExceptionHandler::uncaughtException("main", std::current_exception());
	}
	shutdown(arguments);
	return exitCode;
}
