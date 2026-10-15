// P5-12b2 (rifts, the vortex and world raids; the D1 split of P5-12b, m5i-plan.md 15.5), M5a subset (m5a-plan.md E1-07): split from tests/worldevents/WorldEventsM5aTest.cpp
// with its fixture, under this file's own suite name.
//
// Expectations are derived by hand from RiftService.java:25-40,220-233, RiftInformer.java:21-26,35-57, RiftManager.java:107-115, VortexService.java:33-41,157-165 and WorldRaidService.java:36-71.

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <unordered_set>

#include "../../playersvc/PlayerEventsTestSupport.h"
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

class RiftsAndWorldRaidsM5aTest : public PlayerEventsTest {
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

TEST_F(RiftsAndWorldRaidsM5aTest, DisabledRiftsVortexAndWorldRaidsHaveNoLocations) {
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

TEST_F(RiftsAndWorldRaidsM5aTest, RiftInfoOfAMapWithoutRiftsIsOneAnnouncePacket) {
	PlayerFixture f = makePlayerInPoeta(2);
	runtime::resetUnportedHitsForTests();
	// Poeta has no twin map (RiftInformer.getTwinId), so only the player gets the announce data of 12 zero counters
	services::rift::RiftInformer::sendRiftsInfo(*f.player);
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

} // namespace
} // namespace aion::gameserver::playerevents::test
