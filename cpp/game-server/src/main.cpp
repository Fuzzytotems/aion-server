// Entry point of the game server (Java: com.aionemu.gameserver.GameServer.main).
//
// main() does what Java's static initializer and the JVM do around GameServer.main: Logging::init, the command line, GameServer::main (the
// startup in Java order, logged as "startup step N: name"), then the run mode: the process waits until the ShutdownHook runs (Ctrl+C, closing
// the console, GameServer.initShutdown or the stop file) and ends in the hook thread (std::quick_exit). A startup that stops at an unported
// function or an exception is logged ("Game server startup stopped at an unported function: ..."), the kernel shuts down in order and the
// process exits with ExitCode::ERROR_.
//
// Run it with ../game-server (the Java module directory) as working directory, so ./config, ./data and ./log resolve like for the Java server.
// Configuration properties can be overridden with -D<key>=<value> arguments; they are applied over config/mygs.properties.
//
// C++-only options:
//   --stop-file=<path>         run mode: the server polls the file every 200 ms; when it exists, the file is deleted and the server shuts down
//                              like on Ctrl+C (ShutdownHook countdown of gameserver.shutdown.delay seconds while players are online)
//   --check-output=<dir>       the report directory. Run mode: unported_trace.txt, partial_trace.txt, live_counts_baseline.txt (after startup),
//                              census.txt (final census after the players logged out), live_counts.txt, lockdep.txt, watchdog.txt and
//                              m5a_summary.txt (CheckOutput.h; m5a-plan.md F-02, F-07). A startup that never reaches the run mode writes the
//                              same seven files, with "started false" in m5a_summary.txt, no baseline and an empty census, so a reader of the
//                              reports fails with the real cause instead of a missing file. Check modes: the M4 files below (default <log>/m4)
//   --log-folder=<dir>         the log directory (logback.xml property "logFolder", Logging::Config::logFolder), default ./log. Logging::init
//                              archives and deletes the *.log files of the previous run in it, so two server processes must never share one:
//                              every test that starts a game server passes its own directory (cmake/RunStartupSmoke.cmake, RunM4Check.cmake)
// M4 check modes (M4 gate, CTest gs.m4.check_static_data, cmake/RunM4Check.cmake): the startup without the handler engines up to World, then an
// orderly runtime shutdown and exit code 0:
//   --check-static-data        the M4 report files in the check output directory:
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
#include <thread>
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
#include "aion/gameserver/CheckOutput.h"
#include "aion/gameserver/GameServer.h"
#include "aion/gameserver/ShutdownHook.h"
#include "aion/gameserver/configs/Config.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/XMLQuests.h"
#include "aion/gameserver/geoEngine/GeoCallbacks.h"
#include "aion/gameserver/geoEngine/GeoWorldLoader.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/network/aion/AionClientPacketFactory.h"
#include "aion/gameserver/questEngine/handlers/models/XMLQuest.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/services/LeakCensus.h"
#include "aion/gameserver/runtime/services/RuntimeLifecycle.h"
#include "aion/gameserver/runtime/sync/MinidumpWriter.h"
#include "aion/gameserver/runtime/sync/Watchdog.h"
#include "aion/gameserver/services/AtreianPassportService.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/world/zone/ZoneName.h"

#include <Windows.h>
#include <psapi.h>

#include "aion/commons/utils/WindowsMacroGuard.h"

namespace {

using aion::commons::configuration::Properties;
using aion::gameserver::CheckOutput;
using aion::gameserver::GameServer;
using aion::gameserver::ShutdownHook;
using aion::gameserver::runtime::RuntimeLifecycle;
namespace Logging = aion::commons::logging::Logging;
namespace LoggerFactory = aion::commons::logging::LoggerFactory;
namespace UncaughtExceptionHandler = aion::commons::utils::concurrent::UncaughtExceptionHandler;

const aion::commons::logging::Logger& log() {
	static const auto* logger = new aion::commons::logging::Logger(LoggerFactory::getLogger("com.aionemu.gameserver.GameServer"));
	return *logger;
}

/** The command line: -Dkey=value overrides (like the login server) and the C++-only options (see the file comment). */
struct Arguments {
	Properties overrides;
	std::vector<std::string> unknown;
	bool checkStaticData = false;
	bool checkIdFactory = false;
	std::optional<std::filesystem::path> checkOutput;
	std::optional<std::filesystem::path> geoProbes;
	std::optional<std::filesystem::path> stopFile;
	std::optional<std::filesystem::path> logFolder;

	/** the M4 check modes: the startup ends after the runtime or World */
	bool checkMode() const { return checkStaticData || checkIdFactory; }

	/** the log directory (default ./log, like logback.xml) */
	std::filesystem::path logDirectory() const { return logFolder.value_or("./log"); }

	/** the report directory of the check modes (default <log>/m4) */
	std::filesystem::path checkModeOutput() const { return checkOutput.value_or(logDirectory() / "m4"); }
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
		else if (arg.starts_with("--stop-file="))
			arguments.stopFile = std::filesystem::path(std::string(arg.substr(std::string_view("--stop-file=").size())));
		else if (arg.starts_with("--log-folder="))
			arguments.logFolder = std::filesystem::path(std::string(arg.substr(std::string_view("--log-folder=").size())));
		else
			arguments.unknown.emplace_back(arg);
	}
	return arguments;
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

/** The M4 measurement name of a startup step (m4_summary.txt keys stay those of the M4 startup), nullptr for the others */
const char* measuredStepName(std::string_view step) {
	if (step == "DataManager.getInstance()")
		return "static data";
	if (step == "ZoneService.init()")
		return "zones";
	if (step == "GeoService.init()")
		return "geo";
	if (step == "World.getInstance()")
		return "world";
	return nullptr;
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
	const std::filesystem::path dir = arguments.checkModeOutput();

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

void writeM4Summary(const Arguments& arguments, int64_t startupMillis) {
	std::filesystem::create_directories(arguments.checkModeOutput());
	std::ofstream out = openOutput(arguments.checkModeOutput() / "m4_summary.txt");
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

/** The watchdog dumps of the run (check output: watchdog.txt) */
struct WatchdogDumps {
	std::mutex mutex; // confined: main.cpp check output only; the listener runs on the watchdog thread
	std::vector<std::string> dumps;
	std::atomic<uint64_t> count{0};
};

WatchdogDumps& watchdogDumps() {
	static auto* dumps = new WatchdogDumps(); // lint: L5 check output state of main, filled by the watchdog listener and never destroyed
	return *dumps;
}

/** main.cpp's hooks into GameServer::main: the M4 check modes, their measurements and the command line log lines */
class MainStartupObserver final : public GameServer::StartupObserver {
public:
	MainStartupObserver(const Arguments& arguments, std::chrono::steady_clock::time_point processStart)
		: arguments(arguments), processStart(processStart) {}

	void runStep(int32_t, std::string_view name, const std::function<void()>& body) override {
		const char* measured = measuredStepName(name);
		if (!arguments.checkMode() || measured == nullptr) {
			body();
			return;
		}
		const auto start = std::chrono::steady_clock::now();
		measurements().step.store(measured, std::memory_order_release);
		if (arguments.checkStaticData && name == "GeoService.init()")
			aion::gameserver::geoEngine::GeoCallbacks::setMaterialZoneListener(&recordMaterialZone);
		body();
		aion::gameserver::geoEngine::GeoCallbacks::setMaterialZoneListener(nullptr);
		sampleBacklog();
		uint64_t backlogAtEnd = aion::gameserver::runtime::Reclaimer::getInstance().stats().backlog;
		int64_t millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
		log().info("M4 check: startup step '{}' took {} ms (Reclaimer backlog {} objects at its end)", measured, millis, backlogAtEnd);
		StartupMeasurements& m = measurements();
		std::scoped_lock lock(m.mutex);
		m.stepMillis.emplace_back(measured, millis);
		m.stepBacklogs.push_back(std::to_string(backlogAtEnd));
	}

	void afterConfigLoad() override {
		if (!arguments.overrides.isEmpty()) {
			std::set<std::string> keys = arguments.overrides.stringPropertyNames();
			log().info("Override properties from the command line (unknown keys are warned above): " +
				aion::commons::utils::StringUtils::join({keys.begin(), keys.end()}, ", "));
		}
		for (const std::string& argument : arguments.unknown)
			log().warn("Unknown command line argument ignored: " + argument);
	}

	bool initHandlerEngines() override {
		// M4: the handler engines are not part of the milestone; the check modes keep its startup path
		return !arguments.checkMode();
	}

	bool continueAfterRuntime() override {
		if (arguments.checkOutput && !arguments.checkMode())
			watchdogListener = aion::gameserver::runtime::Watchdog::getInstance().addDumpListener([](const aion::gameserver::runtime::Watchdog::DumpReport& report) {
				WatchdogDumps& dumps = watchdogDumps();
				dumps.count.fetch_add(1, std::memory_order_acq_rel);
				std::scoped_lock lock(dumps.mutex);
				dumps.dumps.push_back(std::string(aion::gameserver::runtime::Watchdog::reasonName(report.reason)) + " " + report.summary);
			});
		if (arguments.checkIdFactory && !arguments.checkStaticData) {
			writeM4Summary(arguments, elapsedMillis());
			log().info("M4 check: IDFactory initialized, --check-id-factory ends the startup here");
			return false;
		}
		if (arguments.checkMode())
			backlogProbe = aion::gameserver::runtime::Watchdog::getInstance().addProbe("M4 check: Reclaimer backlog high-water mark",
				[](aion::gameserver::runtime::Watchdog&, const std::vector<aion::gameserver::runtime::Watchdog::ThreadSnapshot>&) { sampleBacklog(); });
		return true;
	}

	bool continueAfterWorld() override {
		if (!arguments.checkMode())
			return true;
		if (backlogProbe != 0)
			aion::gameserver::runtime::Watchdog::getInstance().removeProbe(backlogProbe);
		int64_t startupMillis = elapsedMillis();
		log().info("M4 startup path (Config, database, runtime, static data, zones, geo, world) completed in {} ms, peak working set {} MB", startupMillis,
			peakWorkingSetBytes() / (1024 * 1024));
		if (arguments.checkStaticData) {
			aion::gameserver::runtime::TaskScope scope(AION_TASK_INFO(aion::gameserver::runtime::TaskKind::STARTUP));
			writeGeoAndWorldReports(arguments);
			writeM4Summary(arguments, startupMillis);
		}
		log().info("M4 check: the startup ends after World");
		return false;
	}

private:
	int64_t elapsedMillis() const {
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - processStart).count();
	}

	const Arguments& arguments;
	const std::chrono::steady_clock::time_point processStart;
	uint64_t backlogProbe = 0;
	uint64_t watchdogListener = 0;
};

/** Configures the logging of the check modes before anything logs (the M4 report sinks) */
void configureCheckModeLogging(const Arguments& arguments) {
	if (!arguments.checkMode())
		return;
	const std::filesystem::path dir = arguments.checkModeOutput();
	std::filesystem::create_directories(dir);
	// the "Loaded N ..." lines of StaticData.afterUnmarshal, as the holders logged them (M4 item 4); additive: they stay in the console log
	const std::filesystem::path counts = dir / "static_data_counts.txt";
	std::filesystem::remove(counts);
	auto sink = std::make_shared<aion::commons::logging::FileAppender>(counts, std::make_unique<aion::commons::logging::PatternLayout>("%msg%n"));
	LoggerFactory::configure("com.aionemu.gameserver.dataholders.StaticData", {.sinks = {sink}, .additive = true});
	// informational: the step log lines with millisecond timestamps (the console pattern has seconds only)
	const std::filesystem::path timelineFile = dir / "startup_timeline.txt";
	std::filesystem::remove(timelineFile);
	auto timeline = std::make_shared<aion::commons::logging::FileAppender>(timelineFile,
		std::make_unique<aion::commons::logging::PatternLayout>("%d{HH:mm:ss.SSS} [%thread] %logger{0} - %msg%n"));
	for (const char* name : TIMELINE_LOGGERS)
		LoggerFactory::configure(name, {.sinks = {timeline}, .additive = true});
}

/** The reports of --check-output in the run mode, before the runtime shuts down (the final census) and after it (CheckOutput.h) */
struct RunReports {
	std::mutex mutex; // confined: written by the ShutdownHook thread only
	CheckOutput::Summary summary;
};

RunReports& runReports() {
	static auto* reports = new RunReports(); // lint: L5 check output state of main, filled on the ShutdownHook thread and never destroyed
	return *reports;
}

/** Java AtreianPassportService.isAtreianPassportDisabled() for m5a_summary.txt (unknown while its body is not ported) */
std::optional<bool> atreianPassportDisabled() {
	try {
		aion::gameserver::runtime::TaskScope scope(AION_TASK_INFO(aion::gameserver::runtime::TaskKind::SHUTDOWN));
		return aion::gameserver::services::AtreianPassportService::getInstance().isAtreianPassportDisabled();
	} catch (...) {
		return std::nullopt;
	}
}

void installRunReports(const std::filesystem::path& dir) {
	std::filesystem::create_directories(dir);
	CheckOutput::writeLiveCounts(dir / "live_counts_baseline.txt");
	ShutdownHook::getInstance().setBeforeRuntimeShutdown([dir] {
		RunReports& reports = runReports();
		std::scoped_lock lock(reports.mutex);
		reports.summary.started = true;
		reports.summary.atreianPassportDisabled = atreianPassportDisabled();
		std::vector<aion::gameserver::runtime::LeakCensus::LeakReport> leaks = CheckOutput::runFinalCensus(
			dir,
			[] {
				aion::gameserver::runtime::TaskScope scope(AION_TASK_INFO(aion::gameserver::runtime::TaskKind::SHUTDOWN));
				return !aion::gameserver::world::World::getInstance().getAllPlayers().empty();
			},
			std::chrono::seconds(10));
		reports.summary.censusLeaks = leaks.size();
		reports.summary.zombieCuts = aion::gameserver::runtime::LeakCensus::getInstance().zombieCutCount();
		log().info("Final census: {} leaks written to {}", leaks.size(), (dir / "census.txt").string());
	});
	ShutdownHook::getInstance().setAfterRuntimeShutdown([dir] {
		RunReports& reports = runReports();
		std::scoped_lock lock(reports.mutex);
		CheckOutput::writeLiveCounts(dir / "live_counts.txt");
		CheckOutput::writeUnportedTrace(dir);
		CheckOutput::writePartialTrace(dir);
		reports.summary.lockdepReports = CheckOutput::writeLockdepReports(dir);
		{
			WatchdogDumps& dumps = watchdogDumps();
			std::scoped_lock dumpsLock(dumps.mutex);
			CheckOutput::writeWatchdogDumps(dir, dumps.dumps);
			reports.summary.watchdogDumps = dumps.count.load(std::memory_order_acquire);
		}
		reports.summary.notPortedClientPackets = aion::gameserver::network::aion::AionClientPacketFactory::unportedPacketClassesSeen();
		// the code the hook thread ends the process with (the stop file and Ctrl+C request NORMAL, the restart cron job RESTART), so the summary
		// cannot claim exit code 0 for a run that exits with another one
		reports.summary.exitCode = ShutdownHook::getInstance().getExitCode();
		CheckOutput::writeSummary(dir, reports.summary);
		log().info("Check output written to {}: {} AION_UNPORTED hits, {} AION_PARTIAL hits", dir.string(), aion::gameserver::runtime::unportedHitCount(),
			aion::gameserver::runtime::partialHitCount());
	});
}

/** The end of a startup that did not reach the run mode (check modes, a failed startup): the kernel shuts down in order, reports are written */
int finishWithoutRunMode(const Arguments& arguments, int exitCode) noexcept {
	try {
		GameServer::shutdownNioServer();
		RuntimeLifecycle::ShutdownReport report = RuntimeLifecycle::shutdown(); // outside any TaskScope
		if (report.performed)
			log().info("Runtime shut down: {} tasks left, {} cleaner ids drained, reclaimer backlog {}, {} objects still tracked", report.tasksLeft,
				report.cleanerIdsDrained, report.reclaimerBacklog, report.censusTracked);
		if (aion::commons::database::DatabaseFactory::isInitialized())
			aion::commons::database::DatabaseFactory::shutdown();
		if (arguments.checkMode()) {
			std::filesystem::create_directories(arguments.checkModeOutput());
			CheckOutput::writeUnportedTrace(arguments.checkModeOutput());
			log().info("M4 check: {} AION_UNPORTED hits, trace written to {}", aion::gameserver::runtime::unportedHitCount(),
				(arguments.checkModeOutput() / "unported_trace.txt").string());
		} else if (arguments.checkOutput) {
			const std::filesystem::path& dir = *arguments.checkOutput;
			std::filesystem::create_directories(dir);
			CheckOutput::writeUnportedTrace(dir);
			CheckOutput::writePartialTrace(dir);
			CheckOutput::writeLiveCounts(dir / "live_counts.txt");
			CheckOutput::Summary summary;
			summary.exitCode = exitCode;
			// the startup never reached the run mode, so no census ran: census.txt gets its header line and no rows, the way a clean run writes
			// it. All seven report files exist on this path too, so the gate's readers fail with the real cause (m5a_summary.txt "started false")
			// instead of "cannot read <path>".
			{
				std::ofstream census(dir / "census.txt", std::ios::binary | std::ios::trunc);
				if (!census)
					throw aion::commons::utils::IOException("Cannot write " + (dir / "census.txt").string());
				CheckOutput::writeCensus(census, {});
			}
			summary.lockdepReports = CheckOutput::writeLockdepReports(dir);
			{
				WatchdogDumps& dumps = watchdogDumps();
				std::scoped_lock dumpsLock(dumps.mutex);
				CheckOutput::writeWatchdogDumps(dir, dumps.dumps);
				summary.watchdogDumps = dumps.count.load(std::memory_order_acquire);
			}
			summary.notPortedClientPackets = aion::gameserver::network::aion::AionClientPacketFactory::unportedPacketClassesSeen();
			CheckOutput::writeSummary(dir, summary);
		}
	} catch (...) {
		UncaughtExceptionHandler::uncaughtException("main", std::current_exception());
	}
	LoggerFactory::removeConfig("com.aionemu.gameserver.dataholders.StaticData");
	for (const char* name : TIMELINE_LOGGERS)
		LoggerFactory::removeConfig(name);
	LoggerFactory::flushAll();
	Logging::shutdown();
	return exitCode;
}

/** The run mode: waits for the shutdown (stop file, console events, GameServer.initShutdown); the process ends in the ShutdownHook thread */
[[noreturn]] void runUntilShutdown(const Arguments& arguments) {
	using namespace std::chrono_literals;
	ShutdownHook& hook = ShutdownHook::getInstance();
	while (!hook.isExitRequested()) {
		std::error_code error;
		if (arguments.stopFile && std::filesystem::exists(*arguments.stopFile, error)) {
			std::filesystem::remove(*arguments.stopFile, error);
			log().info("Stop file " + arguments.stopFile->string() + " found, shutting down");
			hook.exit(aion::commons::utils::ExitCode::NORMAL); // Java: System.exit(0)
			break;
		}
		std::this_thread::sleep_for(200ms);
	}
	for (;;) // the ShutdownHook thread ends the process (std::quick_exit); main must not return and run static destructors meanwhile
		std::this_thread::sleep_for(1h);
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
	bool started = false;
	try {
		// Java: GameServer's static initializer
		Logging::Config loggingConfig = aion::gameserver::configs::Config::loadLoggingConfig();
		if (arguments.logFolder) // C++ only: --log-folder, so two test servers never archive each other's log files (file comment)
			loggingConfig.logFolder = *arguments.logFolder;
		Logging::init(loggingConfig); // must run before anything logs to the files
		configureCheckModeLogging(arguments);
		// C++ addition: command line overrides are layered where Java layers the active events' properties (over mygs.properties), so they also
		// survive later Config::load calls. EventService (P5-12b) must keep them when it registers its provider.
		if (!arguments.overrides.isEmpty()) {
			Properties overrides = arguments.overrides;
			aion::gameserver::configs::Config::setEventConfigPropertiesProvider([overrides] { return overrides; });
		}
		MainStartupObserver observer(arguments, processStart);
		started = GameServer::main(observer);
		exitCode = aion::commons::utils::ExitCode::NORMAL;
		if (started && arguments.checkOutput)
			installRunReports(*arguments.checkOutput);
	} catch (const aion::gameserver::runtime::UnportedException& e) {
		// the site was already logged with its stack trace by AION_UNPORTED
		log().error("Game server startup stopped at an unported function: " + std::string(e.what()));
		started = false;
		exitCode = aion::commons::utils::ExitCode::ERROR_;
	} catch (...) {
		UncaughtExceptionHandler::uncaughtException("main", std::current_exception());
		started = false;
		exitCode = aion::commons::utils::ExitCode::ERROR_;
	}
	if (!started)
		return finishWithoutRunMode(arguments, exitCode);
	runUntilShutdown(arguments);
}
