#include "aion/gameserver/GameServer.h"

#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/network/NioServer.h"
#include "aion/commons/network/ServerCfg.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/commons/utils/info/SystemInfo.h"
#include "aion/commons/utils/info/VersionInfo.h"
#include "aion/gameserver/GameServerError.h"
#include "aion/gameserver/ShutdownHook.h"
#include "aion/gameserver/ai/AIEngine.h"
#include "aion/gameserver/cache/HTMLCache.h"
#include "aion/gameserver/configs/Config.h"
#include "aion/gameserver/configs/main/CleaningConfig.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/configs/main/RuntimeConfig.h"
#include "aion/gameserver/configs/network/NetworkConfig.h"
#include "aion/gameserver/custom/instance/CustomInstanceService.h"
#include "aion/gameserver/custom/pvpmap/PvpMapService.h"
#include "aion/gameserver/dao/GuideDAO.h"
#include "aion/gameserver/dao/HousesDAO.h"
#include "aion/gameserver/dao/InventoryDAO.h"
#include "aion/gameserver/dao/LegionDAO.h"
#include "aion/gameserver/dao/MailDAO.h"
#include "aion/gameserver/dao/PlayerDAO.h"
#include "aion/gameserver/dao/PlayerPetsDAO.h"
#include "aion/gameserver/dao/PlayerRegisteredItemsDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/instance/InstanceEngine.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/siege/Influence.h"
#include "aion/gameserver/network/aion/GameConnectionFactoryImpl.h"
#include "aion/gameserver/network/chatserver/ChatServer.h"
#include "aion/gameserver/network/loginserver/LoginServer.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/services/RuntimeLifecycle.h"
#include "aion/gameserver/services/AdminService.h"
#include "aion/gameserver/services/AnnouncementService.h"
#include "aion/gameserver/services/AtreianPassportService.h"
#include "aion/gameserver/services/BaseService.h"
#include "aion/gameserver/services/BrokerService.h"
#include "aion/gameserver/services/ChallengeTaskService.h"
#include "aion/gameserver/services/CommandsAccessService.h"
#include "aion/gameserver/services/CronJobService.h"
#include "aion/gameserver/services/CuringZoneService.h"
#include "aion/gameserver/services/DatabaseCleaningService.h"
#include "aion/gameserver/services/DebugService.h"
#include "aion/gameserver/services/ExchangeService.h"
#include "aion/gameserver/services/FlyRingService.h"
#include "aion/gameserver/services/GameTimeService.h"
#include "aion/gameserver/services/HousingBidService.h"
#include "aion/gameserver/services/HousingService.h"
#include "aion/gameserver/services/LegionDominionService.h"
#include "aion/gameserver/services/LimitedItemTradeService.h"
#include "aion/gameserver/services/PeriodicSaveService.h"
#include "aion/gameserver/services/RiftService.h"
#include "aion/gameserver/services/RoadService.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/TownService.h"
#include "aion/gameserver/services/VortexService.h"
#include "aion/gameserver/services/WeatherService.h"
#include "aion/gameserver/services/WorldRaidService.h"
#include "aion/gameserver/services/abyss/AbyssRankUpdateService.h"
#include "aion/gameserver/services/abyss/AbyssRankingCache.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/ConquerorAndProtectorService.h"
#include "aion/gameserver/services/drop/DropRegistrationService.h"
#include "aion/gameserver/services/event/EventService.h"
#include "aion/gameserver/services/instance/PeriodicInstanceManager.h"
#include "aion/gameserver/services/player/PlayerLimitService.h"
#include "aion/gameserver/services/transfers/PlayerTransferService.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/taskmanager/tasks/housing/AuctionAutoFillTask.h"
#include "aion/gameserver/taskmanager/tasks/housing/AuctionEndTask.h"
#include "aion/gameserver/taskmanager/tasks/housing/MaintenanceTask.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/chathandlers/ChatProcessor.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldLeakProbe.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/world/zone/ZoneService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver {

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.GameServer"));
	return *logger;
}

/** The kernel configuration from the loaded configs (RuntimeLifecycle::Options documents the keys); IDFactory's used ids in Java order */
runtime::RuntimeLifecycle::Options runtimeOptions() {
	using configs::main::GSConfig;
	using configs::main::RuntimeConfig;
	runtime::RuntimeLifecycle::Options options;
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

} // namespace

const int32_t GameServer::START_TIME_SECONDS = static_cast<int32_t>(
	std::chrono::duration_cast<std::chrono::seconds>(commons::utils::info::SystemInfo::getProcessStartTime().time_since_epoch()).count());

const commons::utils::info::VersionInfo& GameServer::versionInfo() {
	static const commons::utils::info::VersionInfo info = commons::utils::info::VersionInfo::ofExecutable();
	return info;
}

void GameServer::StartupObserver::runStep(int32_t, std::string_view, const std::function<void()>& body) {
	body();
}

void GameServer::main() {
	StartupObserver observer;
	main(observer);
}

bool GameServer::main(StartupObserver& observer) {
	int32_t step = 0;
	// Java: JAXBUtil.preLoadContextAsync(StaticData.class) - the C++ binders need no context
	if (!initUtilityServicesAndConfig(observer, step))
		return false;

	runStep(observer, step, "DataManager.getInstance()", [] { dataholders::DataManager::getInstance(); });

	// Java: Stream.of(QuestEngine, AIEngine, InstanceEngine, ChatProcessor, ZoneService, GeoService).parallel().forEach(GameEngine::init)
	if (observer.initHandlerEngines()) {
		runStep(observer, step, "QuestEngine.init()", [] { questEngine::QuestEngine::getInstance().init(); });
		runStep(observer, step, "AIEngine.init()", [] { ai::AIEngine::getInstance().init(); });
		runStep(observer, step, "InstanceEngine.init()", [] { instance::InstanceEngine::getInstance().init(); });
		runStep(observer, step, "ChatProcessor.init()", [] { utils::chathandlers::ChatProcessor::getInstance().init(); });
	}
	runStep(observer, step, "ZoneService.init()", [] { world::zone::ZoneService::getInstance().init(); });
	runStep(observer, step, "GeoService.init()", [] { world::geo::GeoService::getInstance().init(); });
	// ZoneService.getInstance().saveMaterialZones();

	runStep(observer, step, "World.getInstance()", [] {
		// World may unpublish the step while its maps are created in their own task scopes (World::World)
		runtime::QuiescentScope quiescent; // quiescent-safe: this frame and runStep hold no borrow (values only)
		runtime::QuiescentOptIn worldCreation(runtime::QuiescentOptIn::WORLD_CREATION);
		world::World::getInstance();
		world::WorldLeakProbe::install(); // C++ only: a leak census report names the world structures that hold the leaked object
	});
	if (!observer.continueAfterWorld())
		return false;
	runStep(observer, step, "GameTimeService.getInstance()", [] { services::GameTimeService::getInstance(); });

	runStep(observer, step, "DropRegistrationService.getInstance()", [] { services::drop::DropRegistrationService::getInstance(); });

	// This is loading only siege location data, no siege schedule or spawns
	runStep(observer, step, "BaseService.getInstance()", [] { services::BaseService::getInstance(); });
	runStep(observer, step, "SiegeService.getInstance()", [] { services::SiegeService::getInstance(); });
	runStep(observer, step, "WorldRaidService.initWorldRaidLocations()", [] { services::WorldRaidService::getInstance().initWorldRaidLocations(); });
	// DAOManager.getDAO(SiegeMercenariesDAO.class).loadActiveMercenaries();
	runStep(observer, step, "VortexService.initVortexLocations()", [] { services::VortexService::getInstance().initVortexLocations(); });
	runStep(observer, step, "RiftService.initRiftLocations()", [] { services::RiftService::getInstance().initRiftLocations(); });
	runStep(observer, step, "LegionDominionService.initLocations()", [] { services::LegionDominionService::getInstance().initLocations(); });

	// init housing service before spawns since it gets called on every instance spawn
	runStep(observer, step, "HousingService.getInstance()", [] { services::HousingService::getInstance(); });
	runStep(observer, step, "HousingBidService.getInstance()", [] { services::HousingBidService::getInstance(); });
	runStep(observer, step, "AuctionEndTask.getInstance()", [] { taskmanager::tasks::housing::AuctionEndTask::getInstance(); });
	runStep(observer, step, "AuctionAutoFillTask.getInstance()", [] { taskmanager::tasks::housing::AuctionAutoFillTask::getInstance(); });
	runStep(observer, step, "MaintenanceTask.getInstance()", [] { taskmanager::tasks::housing::MaintenanceTask::getInstance(); });
	runStep(observer, step, "ChallengeTaskService.getInstance()", [] { services::ChallengeTaskService::getInstance(); });

	runStep(observer, step, "SpawnEngine.spawnAll()", [] { spawnengine::SpawnEngine::spawnAll(); });
	runStep(observer, step, "TownService.getInstance()", [] { services::TownService::getInstance(); });
	runStep(observer, step, "FlyRingService.getInstance()", [] { services::FlyRingService::getInstance(); });
	runStep(observer, step, "RiftService.initRifts()", [] { services::RiftService::getInstance().initRifts(); });

	runStep(observer, step, "ratio limitation", [] {
		if (configs::main::GSConfig::ENABLE_RATIO_LIMITATION.load()) { // TODO move all of this stuff in a separate class / service
			ASMOS_COUNT.set(dao::PlayerDAO::getCharacterCountForRace(model::Race::ASMODIANS));
			ELYOS_COUNT.set(dao::PlayerDAO::getCharacterCountForRace(model::Race::ELYOS));
			updateRatio(std::nullopt, 0);
		}
	});
	runStep(observer, step, "LimitedItemTradeService.start()", [] { services::LimitedItemTradeService::getInstance().start(); });
	runStep(observer, step, "PlayerLimitService.scheduleUpdate()", [] {
		if (configs::main::CustomConfig::LIMITS_ENABLED.load())
			services::player::PlayerLimitService::getInstance().scheduleUpdate();
	});

	// Init Sieges... It's separated due to spawn engine.
	// It should not spawn siege NPCs
	runStep(observer, step, "SiegeService.initSieges()", [] { services::SiegeService::getInstance().initSieges(); });

	runStep(observer, step, "BaseService.initBases()", [] { services::BaseService::getInstance().initBases(); });

	runStep(observer, step, "WorldRaidService.initWorldRaids()", [] { services::WorldRaidService::getInstance().initWorldRaids(); });

	runStep(observer, step, "ConquerorAndProtectorService.init()",
		[] { services::conquerorAndProtectorSystem::ConquerorAndProtectorService::getInstance().init(); });

	runStep(observer, step, "AnnouncementService.getInstance()", [] { services::AnnouncementService::getInstance(); });
	runStep(observer, step, "DebugService.getInstance()", [] { services::DebugService::getInstance(); });
	runStep(observer, step, "WeatherService.getInstance()", [] { services::WeatherService::getInstance(); });
	runStep(observer, step, "BrokerService.getInstance()", [] { services::BrokerService::getInstance(); });
	runStep(observer, step, "Influence.getInstance()", [] { model::siege::Influence::getInstance(); });
	runStep(observer, step, "ExchangeService.getInstance()", [] { services::ExchangeService::getInstance(); });
	runStep(observer, step, "PeriodicSaveService.getInstance()", [] { services::PeriodicSaveService::getInstance(); });
	runStep(observer, step, "AtreianPassportService.getInstance()", [] { services::AtreianPassportService::getInstance(); });
	runStep(observer, step, "CronJobService.getInstance()", [] { services::CronJobService::getInstance(); });

	runStep(observer, step, "CuringZoneService.getInstance()", [] {
		if (!configs::main::GeoDataConfig::GEO_MATERIALS_ENABLE.load())
			services::CuringZoneService::getInstance();
	});
	runStep(observer, step, "RoadService.getInstance()", [] { services::RoadService::getInstance(); });
	runStep(observer, step, "HTMLCache.getInstance()", [] { cache::HTMLCache::getInstance(); });
	runStep(observer, step, "AbyssRankingCache.getInstance()", [] { services::abyss::AbyssRankingCache::getInstance(); });
	runStep(observer, step, "AbyssRankUpdateService.scheduleUpdate()", [] { services::abyss::AbyssRankUpdateService::scheduleUpdate(); });
	runStep(observer, step, "PeriodicInstanceManager.getInstance()", [] { services::instance::PeriodicInstanceManager::getInstance(); });
	runStep(observer, step, "EventService.start()", [] { services::event::EventService::getInstance().start(); });

	runStep(observer, step, "AdminService.getInstance()", [] { services::AdminService::getInstance(); });
	runStep(observer, step, "CommandsAccessService.loadAccesses()", [] { services::CommandsAccessService::loadAccesses(); });

	runStep(observer, step, "PlayerTransferService.getInstance()", [] { services::transfers::PlayerTransferService::getInstance(); });

	runStep(observer, step, "GameTimeService.startClock()", [] { services::GameTimeService::getInstance().startClock(); });

	runStep(observer, step, "PvpMapService.init()", [] { custom::pvpmap::PvpMapService::getInstance().init(); });
	runStep(observer, step, "CustomInstanceService.getInstance()", [] { custom::instance::CustomInstanceService::getInstance(); });
	runStep(observer, step, "DataManager.waitForValidationToFinishAndShutdownOnFail()",
		[] { dataholders::DataManager::waitForValidationToFinishAndShutdownOnFail(); });

	// System.gc();

	runStep(observer, step, "VersionInfo.logAll()", [] {
		commons::utils::info::VersionInfo::logAll(versionInfo(), configs::main::GSConfig::TIME_ZONE_ID.load());
		commons::utils::info::SystemInfo::logAll();
	});

	runStep(observer, step, "initNioServer()", [] { nioServer.store(initNioServer(), std::memory_order_release); });
	runStep(observer, step, "ShutdownHook", [] { ShutdownHook::getInstance().install(); }); // Java: Runtime.getRuntime().addShutdownHook(...)
	log().info("Game server started in " + std::to_string(commons::utils::currentTimeMillis() / 1000 - START_TIME_SECONDS) + " seconds.");

	runStep(observer, step, "LoginServer.connect(nioServer)", [] { network::loginserver::LoginServer::getInstance().connect(*getNioServer()); });
	runStep(observer, step, "ChatServer.connect(nioServer)", [] {
		if (configs::main::GSConfig::ENABLE_CHAT_SERVER.load())
			network::chatserver::ChatServer::getInstance().connect(*getNioServer());
	});
	return true;
}

commons::network::NioServer* GameServer::initNioServer() {
	using configs::network::NetworkConfig;
	if (NetworkConfig::NIO_READ_WRITE_THREADS.load() > 1 && !NetworkConfig::NIO_READ_WRITE_THREADS_UNSAFE_ALLOW.load())
		throw GameServerError("gameserver.network.nio.threads must not exceed 1 (the game server is not thread-safe)"); // Java: new Error(...)
	runtime::Ref<network::aion::GameConnectionFactoryImpl> factory = network::aion::GameConnectionFactoryImpl::create();
	commons::network::ServerCfg cfg{*NetworkConfig::CLIENT_SOCKET_ADDRESS.get(), "Aion game clients", factory->toConnectionFactory()};
	// never freed: LoginServer and ChatServer keep a plain pointer to it for the whole run (class comment)
	auto* server = new commons::network::NioServer(NetworkConfig::NIO_READ_WRITE_THREADS.load(), std::vector{cfg});
	// Java: nioServer.connect(ThreadPoolManager.getInstance()): onDisconnect callbacks run on the instant pool
	server->connect([](std::function<void()> task) { utils::ThreadPoolManager::getInstance().execute(runtime::Pin(), [task = std::move(task)] { task(); }); });
	return server;
}

bool GameServer::initUtilityServicesAndConfig(StartupObserver& observer, int32_t& step) {
	// Set default uncaught exception handler: main.cpp installs it before Logging::init (Java: in this method)

	// PropertyTransformers.register(new CronExpressionTransformer()): a compile-time PropertyTransformer specialization in C++
	runStep(observer, step, "Config.load()", [&observer] {
		configs::Config::load();
		observer.afterConfigLoad();
	});
	// Second should be database factory
	runStep(observer, step, "DatabaseFactory.init()", [] {
		commons::database::DatabaseFactory::init(commons::database::DatabaseFactory::gameServerOptions());
		dao::PlayerDAO::setAllPlayersOffline();
		if (configs::main::CleaningConfig::CLEANING_ENABLE.load())
			services::DatabaseCleaningService::deletePlayersOnInactiveAccounts();
	});

	// Initialize thread pools; Initialize cron service (Java); C++: RuntimeLifecycle also starts IDFactory (Java: IDFactory.getInstance() in main)
	runStep(observer, step, "ThreadPoolManager, CronService, IDFactory", [] { runtime::RuntimeLifecycle::start(runtimeOptions()); });
	return observer.continueAfterRuntime();
}

void GameServer::runStep(StartupObserver& observer, int32_t& step, std::string_view name, const std::function<void()>& body) {
	step++;
	log().info("startup step " + std::to_string(step) + ": " + std::string(name));
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::STARTUP));
	observer.runStep(step, name, body);
}

void GameServer::shutdownNioServer() {
	if (commons::network::NioServer* server = nioServer.exchange(nullptr, std::memory_order_acq_rel)) {
		server->shutdown();
	}
}

bool GameServer::isShutdownScheduled() {
	return ShutdownHook::getInstance().isRunning();
}

bool GameServer::isShuttingDownSoon() {
	return ShutdownHook::getInstance().isRunning() && ShutdownHook::getInstance().getRemainingSeconds() <= 30;
}

void GameServer::initShutdown(int32_t exitCode, int32_t delaySeconds) {
	ShutdownHook::getInstance().initShutdown(exitCode, delaySeconds);
}

void GameServer::updateRatio(std::optional<model::Race> race, int32_t i) {
	if (!race)
		return;
	{
		lock.lock();
		struct Unlock { // Java: finally { lock.unlock(); }
			~Unlock() { lock.unlock(); }
		} unlock;
		switch (*race) {
			case model::Race::ASMODIANS:
				ASMOS_COUNT.set(ASMOS_COUNT.get() + i);
				break;
			case model::Race::ELYOS:
				ELYOS_COUNT.set(ELYOS_COUNT.get() + i);
				break;
			default:
				break;
		}

		if ((ASMOS_COUNT.get() <= configs::main::GSConfig::RATIO_MIN_CHARACTERS_COUNT.load()) &&
			(ELYOS_COUNT.get() <= configs::main::GSConfig::RATIO_MIN_CHARACTERS_COUNT.load())) {
			ASMOS_RATIO.set(50.0f);
			ELYOS_RATIO.set(50.0f);
		} else {
			int32_t total = ASMOS_COUNT.get() + ELYOS_COUNT.get();
			if (total == 0) // Java int division
				throw commons::utils::ArithmeticException("/ by zero");
			ASMOS_RATIO.set(static_cast<float>(ASMOS_COUNT.get() * 100 / total));
			ELYOS_RATIO.set(static_cast<float>(ELYOS_COUNT.get() * 100 / total));
		}
	}

	char buffer[128];
	std::snprintf(buffer, sizeof(buffer), "FACTIONS RATIO UPDATED: E %.1f %% / A %.1f %%", ELYOS_RATIO.get(), ASMOS_RATIO.get());
	log().info(buffer);
}

float GameServer::getRatiosFor(model::Race race) {
	switch (race) {
		case model::Race::ASMODIANS:
			return ASMOS_RATIO.get();
		case model::Race::ELYOS:
			return ELYOS_RATIO.get();
		default:
			return 0.0f;
	}
}

int32_t GameServer::getCountFor(model::Race race) {
	switch (race) {
		case model::Race::ASMODIANS:
			return ASMOS_COUNT.get();
		case model::Race::ELYOS:
			return ELYOS_COUNT.get();
		default:
			return 0;
	}
}

commons::network::NioServer* GameServer::getNioServer() noexcept {
	return nioServer.load(std::memory_order_acquire);
}

} // namespace aion::gameserver
