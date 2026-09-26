// P5-12b world events, M5a subset (m5a-plan.md E1-07): EventService with gameserver.event.service.disabled_events=* (plan D1), the disabled rift,
// vortex, world raid and conqueror/protector initializations, the rift informer for a map without rifts, Panesterra on a normal map and the
// BaseService warn stub.
//
// Expectations are derived by hand from EventService.java:45-133,181-215, RiftService.java:25-40,220-233, RiftInformer.java:21-26,35-57,
// RiftManager.java:107-115, VortexService.java:33-41,157-165, WorldRaidService.java:36-71, ConquerorAndProtectorService.java:42-100,254-257,
// PanesterraService.java:238-274 and BaseService.java:30-51.

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <unordered_set>

#include "../playersvc/PlayerEventsTestSupport.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/EventData.bind.h"
#include "aion/gameserver/dataholders/EventData.h"
#include "aion/gameserver/model/EventTheme.h"
#include "aion/gameserver/model/templates/cp/CPType.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/CPInfo.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/BaseService.h"
#include "aion/gameserver/services/RiftService.h"
#include "aion/gameserver/services/VortexService.h"
#include "aion/gameserver/services/WorldRaidService.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/ConquerorAndProtectorService.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/services/event/EventService.h"
#include "aion/gameserver/services/panesterra/PanesterraService.h"
#include "aion/gameserver/services/rift/RiftInformer.h"
#include "aion/gameserver/services/rift/RiftManager.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::playerevents::test {
namespace {

using runtime::Ptr;

class WorldEventsM5aTest : public PlayerEventsTest {
protected:
	void TearDown() override {
		// EventService is a process-wide singleton: a test that leaves checkTask set makes the next test's start() return false, and the stale
		// JobDetail would outlive the CronService that handed it out. stop() is a no-op when the service was never started.
		services::event::EventService::getInstance().stop();
		services::cron::CronService::resetForTests();
		PlayerEventsTest::TearDown();
	}

	void SetUp() override {
		PlayerEventsTest::SetUp();
		// ServerTime (EventService.collectActiveEvents) reads the server time zone
		timeZone.emplace(configs::main::GSConfig::TIME_ZONE_ID, std::chrono::locate_zone("UTC"));
	}

	std::optional<AtomicConfigScope<const std::chrono::time_zone*>> timeZone;

	static void startCronService() {
		services::cron::CronService::initSingleton(std::make_unique<services::cron::CurrentThreadRunnableRunner>(), std::chrono::locate_zone("UTC"),
			services::cron::CronService::Driver::EXECUTOR);
	}

	/** a test player standing in Poeta */
	PlayerFixture makePlayerInPoeta(int32_t objectId) {
		PlayerFixture f = makePlayer(objectId, objectId * 100);
		f.player->setPosition(Ptr<world::WorldPosition>(world::WorldPosition::create(210010000, 1212.94f, 1044.85f, 140.76f, 0)));
		return f;
	}
};

TEST_F(WorldEventsM5aTest, AllEventsDisabledStartsTheCheckTaskWithoutActiveEvents) {
	startCronService();
	ConfigValueScope<std::unordered_set<std::string>> disabled(configs::main::EventsConfig::DISABLED_EVENTS, {"*"});
	services::event::EventService& service = services::event::EventService::getInstance();
	runtime::resetUnportedHitsForTests();

	// "*" skips validateConfiguredEventNames and collectActiveEvents returns the empty set (EventService.java:69,133), so no event data is read
	EXPECT_TRUE(service.start());
	EXPECT_FALSE(service.start()) << "the check task exists already";
	EXPECT_EQ(services::cron::CronService::getInstance().getJobCount(), 1u) << "the 5 minute check (0 0/5 * ? * *)";
	ASSERT_TRUE(service.getActiveEvents());
	EXPECT_EQ(service.getActiveEvents()->size(), 0);
	EXPECT_EQ(service.getEventTheme(), model::EventTheme::NONE);
	EXPECT_FALSE(service.isEventActive("Beyond Aion Server Buffs"));
	EXPECT_FALSE(service.isActiveEventQuest(80000));
	ASSERT_TRUE(service.getActiveEventDropRules());
	EXPECT_EQ(service.getActiveEventDropRules()->size(), 0);
	EXPECT_EQ(service.getActiveEventConfigProperties().size(), 0u);

	// login and map entry of M5a add nothing (plan §5.8)
	PlayerFixture f = makePlayerInPoeta(1);
	service.onPlayerLogin(*f.player);
	service.onEnterMap(*f.player);
	EXPECT_EQ(runtime::unportedHitCount(), 0u);

	service.stop();
	EXPECT_EQ(services::cron::CronService::getInstance().getJobCount(), 0u) << "stop cancels the check task";
	EXPECT_TRUE(service.start()) << "a stopped service starts again";
}

TEST_F(WorldEventsM5aTest, EventsOutsideTheirPeriodStayInactiveAndUnknownDisabledNamesAreReported) {
	startCronService();
	PublishedHolder events(dataholders::DataManager::EVENT_DATA,
		bindXml<dataholders::EventData>(R"(<timed_events><event name="Past" start="2014-03-01T00:00:00" end="2014-04-01T00:00:00"/>)"
										R"(<event name="Future" start="2199-01-01T00:00:00"/></timed_events>)"));
	// "Unknown" is not an event name: EventService logs a warning; "Past" and "Future" are outside their period, so nothing starts
	ConfigValueScope<std::unordered_set<std::string>> disabled(configs::main::EventsConfig::DISABLED_EVENTS, {"Unknown"});
	services::event::EventService& service = services::event::EventService::getInstance();
	runtime::resetUnportedHitsForTests();
	EXPECT_TRUE(service.start());
	EXPECT_EQ(service.getActiveEvents()->size(), 0);
	EXPECT_EQ(service.getEventTheme(), model::EventTheme::NONE);
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(WorldEventsM5aTest, DisabledRiftsVortexAndWorldRaidsHaveNoLocations) {
	AtomicConfigScope<bool> rifts(configs::main::CustomConfig::RIFT_ENABLED, false);
	AtomicConfigScope<bool> vortex(configs::main::CustomConfig::VORTEX_ENABLED, false);
	AtomicConfigScope<bool> worldRaids(configs::main::EventsConfig::ENABLE_WORLDRAID, false);
	runtime::resetUnportedHitsForTests();

	services::RiftService& riftService = services::RiftService::getInstance();
	riftService.initRiftLocations();
	riftService.initRifts();
	ASSERT_TRUE(riftService.getRiftLocations());
	EXPECT_EQ(riftService.getRiftLocations()->size(), 0);
	EXPECT_FALSE(riftService.isValidId(1)) << "a rift id (< 10000) without a location";
	EXPECT_FALSE(riftService.isValidId(210020000)) << "a map id without a rift location";
	EXPECT_FALSE(riftService.isRiftOpened(1));
	EXPECT_FALSE(riftService.getRiftLocation(1));
	EXPECT_TRUE(services::rift::RiftManager::getSpawnedRifts(210010000).empty()) << "Java: Collections.emptyList()";

	services::VortexService::getInstance().initVortexLocations();
	EXPECT_FALSE(services::VortexService::getInstance().getLocationByWorld(210010000)) << "only Theobomos and Brusthonin have a vortex";

	services::WorldRaidService& worldRaidService = services::WorldRaidService::getInstance();
	worldRaidService.initWorldRaidLocations();
	worldRaidService.initWorldRaids();
	EXPECT_FALSE(worldRaidService.isValidWorldRaidLocation(1));
	EXPECT_FALSE(worldRaidService.isWorldRaidInProgress(1));
	EXPECT_TRUE(worldRaidService.getActiveWorldRaidLocations().empty());
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(WorldEventsM5aTest, RiftInfoOfAMapWithoutRiftsIsOneAnnouncePacket) {
	PlayerFixture f = makePlayerInPoeta(2);
	runtime::resetUnportedHitsForTests();
	// Poeta has no twin map (RiftInformer.getTwinId), so only the player gets the announce data of 12 zero counters
	services::rift::RiftInformer::sendRiftsInfo(*f.player);
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(WorldEventsM5aTest, PanesterraAndConquerorProtectorIgnoreNormalMaps) {
	PlayerFixture f = makePlayerInPoeta(3);
	runtime::resetUnportedHitsForTests();
	services::panesterra::PanesterraService::getInstance().onEnterPanesterra(*f.player); // getSiegeId(210010000) == 0

	services::conquerorAndProtectorSystem::ConquerorAndProtectorService& cp = services::conquerorAndProtectorSystem::ConquerorAndProtectorService::getInstance();
	{
		AtomicConfigScope<bool> cpDisabled(configs::main::CustomConfig::CONQUEROR_AND_PROTECTOR_SYSTEM_ENABLED, false);
		cp.init();
		cp.onEnterMap(*f.player);
		cp.onLeaveMap(*f.player);
		EXPECT_FALSE(cp.getCPInfoForCurrentMap(*f.player));
	}
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(WorldEventsM5aTest, ConquerorProtectorWorldsNeedARaceSpecificMap) {
	AtomicConfigScope<bool> cpEnabled(configs::main::CustomConfig::CONQUEROR_AND_PROTECTOR_SYSTEM_ENABLED, true);
	AtomicConfigScope<int32_t> interval(configs::main::CustomConfig::CONQUEROR_AND_PROTECTOR_KILLS_DECREASE_INTERVAL, 1);
	ConfigValueScope<std::unordered_set<int32_t>> worlds(configs::main::CustomConfig::CONQUEROR_AND_PROTECTOR_WORLDS, {400010000});
	// Reshanta (400010000): the second digit 0 means "all races" (ConquerorAndProtectorService.java:47-53)
	EXPECT_THROW(services::conquerorAndProtectorSystem::ConquerorAndProtectorService::getInstance().init(), commons::utils::IllegalArgumentException);
}

TEST_F(WorldEventsM5aTest, ConquerorProtectorTypeFollowsTheRaceOfTheHandledMap) {
	AtomicConfigScope<bool> cpEnabled(configs::main::CustomConfig::CONQUEROR_AND_PROTECTOR_SYSTEM_ENABLED, true);
	AtomicConfigScope<int32_t> interval(configs::main::CustomConfig::CONQUEROR_AND_PROTECTOR_KILLS_DECREASE_INTERVAL, 1);
	ConfigValueScope<std::unordered_set<int32_t>> worlds(configs::main::CustomConfig::CONQUEROR_AND_PROTECTOR_WORLDS, {210020000, 220020000});
	services::conquerorAndProtectorSystem::ConquerorAndProtectorService& cp = services::conquerorAndProtectorSystem::ConquerorAndProtectorService::getInstance();
	cp.init();
	EXPECT_EQ(executor->pendingTaskCount(), 1u) << "the kills decrease timer";

	PlayerFixture elyos = makePlayer(4, 400, model::Race::ELYOS);
	elyos.player->setPosition(Ptr<world::WorldPosition>(world::WorldPosition::create(220020000, 0.0f, 0.0f, 0.0f, 0)));
	runtime::resetUnportedHitsForTests();
	cp.onEnterMap(*elyos.player); // an Elyos in Morheim is a conqueror without CP info yet: no packet
	EXPECT_FALSE(cp.getCPInfoForCurrentMap(*elyos.player));
	Ptr<services::conquerorAndProtectorSystem::CPInfo> created;
	SKIP_IF_UNPORTED(created = cp.getCPInfoForCurrentMap(*elyos.player, true));
	ASSERT_TRUE(created);
	EXPECT_EQ(created->getType(), model::templates::cp::CPType::CONQUEROR);
	EXPECT_EQ(cp.getCPInfoForCurrentMap(*elyos.player).get(), created.get()) << "computeIfAbsent stored it";
}

TEST_F(WorldEventsM5aTest, BaseServiceIsAWarnStubUntilTheBaseLocationsArePorted) {
	runtime::resetPartialHitsForTests();
	runtime::resetUnportedHitsForTests();
	services::BaseService& service = services::BaseService::getInstance();
	service.initBases(); // no base locations: nothing starts
	EXPECT_EQ(runtime::partialHitCount(), 1u) << "the constructor's AION_PARTIAL";
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

} // namespace
} // namespace aion::gameserver::playerevents::test
