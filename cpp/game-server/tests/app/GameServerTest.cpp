// GameServer (m5a-plan.md F-01a): the startup step order of GameServer::main against the statements of GameServer.java:92-191, the faction
// ratios of updateRatio/getRatiosFor/getCountFor (GameServer.java:252-299), the shutdown queries over the ShutdownHook, START_TIME_SECONDS
// and the StartupObserver defaults. Expectations derived by hand from GameServer.java; the Java server is never run.

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "aion/commons/utils/ExitCode.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/GameServer.h"
#include "aion/gameserver/ShutdownHook.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/model/Race.h"

namespace aion::gameserver {
namespace {

using configs::main::GSConfig;
using model::Race;

void noExit(int32_t) {
}

class GameServerTest : public testing::Test {
protected:
	void SetUp() override { minCharacters = GSConfig::RATIO_MIN_CHARACTERS_COUNT.load(); }

	void TearDown() override {
		GSConfig::RATIO_MIN_CHARACTERS_COUNT.store(minCharacters);
		ShutdownHook::resetForTests();
	}

	int32_t minCharacters = 0;
};

TEST_F(GameServerTest, RatiosFollowJavasIntegerArithmetic) {
	GSConfig::RATIO_MIN_CHARACTERS_COUNT.store(2);
	// a null race returns before anything changes
	GameServer::updateRatio(std::nullopt, 5);
	EXPECT_EQ(GameServer::getCountFor(Race::ELYOS), 0);
	EXPECT_EQ(GameServer::getRatiosFor(Race::ELYOS), 0.0f);

	// both counts at or below the minimum: 50 % each
	GameServer::updateRatio(Race::ELYOS, 1);
	EXPECT_EQ(GameServer::getCountFor(Race::ELYOS), 1);
	EXPECT_EQ(GameServer::getRatiosFor(Race::ELYOS), 50.0f);
	EXPECT_EQ(GameServer::getRatiosFor(Race::ASMODIANS), 50.0f);

	// 2 elyos, 1 asmodian: still both <= 2
	GameServer::updateRatio(Race::ELYOS, 1);
	GameServer::updateRatio(Race::ASMODIANS, 1);
	EXPECT_EQ(GameServer::getRatiosFor(Race::ASMODIANS), 50.0f);

	// 3 elyos, 1 asmodian: 3 * 100 / 4 = 75, 1 * 100 / 4 = 25
	GameServer::updateRatio(Race::ELYOS, 1);
	EXPECT_EQ(GameServer::getRatiosFor(Race::ELYOS), 75.0f);
	EXPECT_EQ(GameServer::getRatiosFor(Race::ASMODIANS), 25.0f);

	// 3 elyos, 2 asmodians: 300 / 5 = 60, 200 / 5 = 40; 4 : 3 truncates: 400 / 7 = 57, 300 / 7 = 42
	GameServer::updateRatio(Race::ASMODIANS, 1);
	EXPECT_EQ(GameServer::getRatiosFor(Race::ELYOS), 60.0f);
	GameServer::updateRatio(Race::ELYOS, 1);
	GameServer::updateRatio(Race::ASMODIANS, 1);
	EXPECT_EQ(GameServer::getCountFor(Race::ELYOS), 4);
	EXPECT_EQ(GameServer::getCountFor(Race::ASMODIANS), 3);
	EXPECT_EQ(GameServer::getRatiosFor(Race::ELYOS), 57.0f);
	EXPECT_EQ(GameServer::getRatiosFor(Race::ASMODIANS), 42.0f);

	// other races: 0 (Java default branch); their updates change no count
	GameServer::updateRatio(Race::NPC, 10);
	EXPECT_EQ(GameServer::getCountFor(Race::NPC), 0);
	EXPECT_EQ(GameServer::getRatiosFor(Race::NPC), 0.0f);
	EXPECT_EQ(GameServer::getCountFor(Race::ELYOS), 4);

	// back to 0 : 0 with a negative minimum: Java divides by zero
	GameServer::updateRatio(Race::ELYOS, -4);
	GSConfig::RATIO_MIN_CHARACTERS_COUNT.store(-1);
	EXPECT_THROW(GameServer::updateRatio(Race::ASMODIANS, -3), commons::utils::ArithmeticException);
	EXPECT_EQ(GameServer::getCountFor(Race::ASMODIANS), 0); // the count changed before the division, as in Java
}

TEST_F(GameServerTest, ShutdownQueriesReadTheShutdownHook) {
	ShutdownHook::setExitFunctionForTests(&noExit);
	std::atomic<bool> release{false};
	ShutdownHook::Operations ops;
	ops.worldHasPlayers = [&release] {
		while (!release.load())
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		return false;
	};
	ops.announceShutdown = [](int32_t) {};
	ops.shutdownNetwork = [] {};
	ops.dumpStats = [] {};
	ops.saveData = [] {};
	ops.shutdownRuntime = [] {};
	ops.sleep = [](std::chrono::milliseconds) {};
	ShutdownHook::setOperationsForTests(ops);

	EXPECT_FALSE(GameServer::isShutdownScheduled());
	EXPECT_FALSE(GameServer::isShuttingDownSoon());
	GameServer::initShutdown(commons::utils::ExitCode::NORMAL, 120);
	EXPECT_TRUE(GameServer::isShutdownScheduled());
	EXPECT_FALSE(GameServer::isShuttingDownSoon()); // more than 30 seconds left
	GameServer::initShutdown(commons::utils::ExitCode::NORMAL, 30);
	EXPECT_TRUE(GameServer::isShuttingDownSoon());
	release.store(true);
	EXPECT_TRUE(ShutdownHook::getInstance().awaitCompletion(std::chrono::seconds(10)));
}

TEST_F(GameServerTest, StartTimeIsTheProcessStart) {
	int64_t nowSeconds = commons::utils::currentTimeMillis() / 1000;
	EXPECT_LE(GameServer::START_TIME_SECONDS, nowSeconds);
	EXPECT_GT(GameServer::START_TIME_SECONDS, nowSeconds - 24 * 3600);
	EXPECT_EQ(GameServer::getNioServer(), nullptr);
	GameServer::shutdownNioServer(); // no server: nothing happens
}

TEST_F(GameServerTest, StartupObserverDefaultsRunTheJavaStartup) {
	GameServer::StartupObserver observer;
	bool ran = false;
	observer.runStep(1, "Config.load()", [&ran] { ran = true; });
	EXPECT_TRUE(ran);
	EXPECT_TRUE(observer.initHandlerEngines());
	EXPECT_TRUE(observer.continueAfterRuntime());
	EXPECT_TRUE(observer.continueAfterWorld());
}

/**
 * A StartupObserver that records the step numbers and names and runs **no** body: every side effect of GameServer::main sits inside a step
 * body, so the whole startup can be walked without a configuration, a database, static data or a world.
 */
class DryRunObserver : public GameServer::StartupObserver {
public:
	std::vector<std::string> steps;
	std::vector<int32_t> numbers;

	void runStep(int32_t number, std::string_view name, const std::function<void()>&) override {
		numbers.push_back(number);
		steps.emplace_back(name);
	}
};

/**
 * The startup order itself (m5a-plan.md F-01a: "GameServer.h/.cpp skeleton in Java order"). The expected list is hand-derived from
 * GameServer.java:92-191 and initUtilityServicesAndConfig (GameServer.java:214-231), read statement by statement; the two documented
 * differences are that the parallel engine stream runs in its source order and that Java's IDFactory.getInstance() is part of the C++ runtime
 * step (docs/deviations/P5-14.md). A step that moves, is duplicated or disappears fails here, which neither the unit tests nor
 * gs.smoke.startup_progress (it only checks the numbering) would notice.
 */
TEST_F(GameServerTest, StartupStepsAreTheStatementsOfJavaGameServerMainInOrder) {
	const std::vector<std::string> expected = {
		// initUtilityServicesAndConfig (GameServer.java:214-231); IDFactory.getInstance() of main is part of the runtime step
		"Config.load()", "DatabaseFactory.init()", "ThreadPoolManager, CronService, IDFactory",
		// main (GameServer.java:92-191)
		"DataManager.getInstance()",
		// Stream.of(QuestEngine, AIEngine, InstanceEngine, ChatProcessor, ZoneService, GeoService).parallel().forEach(GameEngine::init)
		"QuestEngine.init()", "AIEngine.init()", "InstanceEngine.init()", "ChatProcessor.init()", "ZoneService.init()", "GeoService.init()",
		"World.getInstance()", "GameTimeService.getInstance()", "DropRegistrationService.getInstance()",
		"BaseService.getInstance()", "SiegeService.getInstance()", "WorldRaidService.initWorldRaidLocations()",
		"VortexService.initVortexLocations()", "RiftService.initRiftLocations()", "LegionDominionService.initLocations()",
		"HousingService.getInstance()", "HousingBidService.getInstance()", "AuctionEndTask.getInstance()", "AuctionAutoFillTask.getInstance()",
		"MaintenanceTask.getInstance()", "ChallengeTaskService.getInstance()",
		"SpawnEngine.spawnAll()", "TownService.getInstance()", "FlyRingService.getInstance()", "RiftService.initRifts()",
		"ratio limitation", "LimitedItemTradeService.start()", "PlayerLimitService.scheduleUpdate()",
		"SiegeService.initSieges()", "BaseService.initBases()", "WorldRaidService.initWorldRaids()", "ConquerorAndProtectorService.init()",
		"AnnouncementService.getInstance()", "DebugService.getInstance()", "WeatherService.getInstance()", "BrokerService.getInstance()",
		"Influence.getInstance()", "ExchangeService.getInstance()", "PeriodicSaveService.getInstance()", "AtreianPassportService.getInstance()",
		"CronJobService.getInstance()",
		"CuringZoneService.getInstance()", "RoadService.getInstance()", "HTMLCache.getInstance()", "AbyssRankingCache.getInstance()",
		"AbyssRankUpdateService.scheduleUpdate()", "PeriodicInstanceManager.getInstance()", "EventService.start()",
		"AdminService.getInstance()", "CommandsAccessService.loadAccesses()",
		"PlayerTransferService.getInstance()",
		"GameTimeService.startClock()",
		"PvpMapService.init()", "CustomInstanceService.getInstance()", "DataManager.waitForValidationToFinishAndShutdownOnFail()",
		// System.gc() has no counterpart
		"VersionInfo.logAll()",
		"initNioServer()", "ShutdownHook",
		// "Game server started in N seconds." is logged here, after the hook and before the login server
		"LoginServer.connect(nioServer)", "ChatServer.connect(nioServer)",
	};

	DryRunObserver observer;
	EXPECT_TRUE(GameServer::main(observer));
	EXPECT_EQ(observer.steps, expected);
	ASSERT_EQ(observer.numbers.size(), observer.steps.size());
	for (size_t i = 0; i < observer.numbers.size(); i++)
		EXPECT_EQ(observer.numbers[i], static_cast<int32_t>(i) + 1) << "the step numbers of the log line run from 1 without gaps";
	ShutdownHook::resetForTests();
}

/** The check modes of main.cpp end the startup early: the observer hooks must cut the step list where Java's M4 path ended. */
TEST_F(GameServerTest, AnObserverCanSkipTheHandlerEnginesAndEndAfterTheWorld) {
	class M4Observer final : public DryRunObserver {
	public:
		bool initHandlerEngines() override { return false; }
		bool continueAfterWorld() override { return false; }
	} observer;
	EXPECT_FALSE(GameServer::main(observer));
	ASSERT_FALSE(observer.steps.empty());
	EXPECT_EQ(observer.steps.back(), "World.getInstance()");
	EXPECT_EQ(std::ranges::find(observer.steps, "QuestEngine.init()"), observer.steps.end()) << "the handler engines are skipped";
	EXPECT_EQ(std::ranges::find(observer.steps, "ChatProcessor.init()"), observer.steps.end());
	EXPECT_NE(std::ranges::find(observer.steps, "ZoneService.init()"), observer.steps.end()) << "ZoneService and GeoService still run";
	EXPECT_NE(std::ranges::find(observer.steps, "GeoService.init()"), observer.steps.end());
}

/** An observer that ends after the runtime: initUtilityServicesAndConfig returns false and main stops there (the --check-id-factory mode). */
TEST_F(GameServerTest, AnObserverCanEndTheStartupAfterTheRuntime) {
	class RuntimeOnlyObserver final : public DryRunObserver {
	public:
		bool continueAfterRuntime() override { return false; }
	} observer;
	EXPECT_FALSE(GameServer::main(observer));
	EXPECT_EQ(observer.steps, (std::vector<std::string>{"Config.load()", "DatabaseFactory.init()", "ThreadPoolManager, CronService, IDFactory"}));
}

} // namespace
} // namespace aion::gameserver
