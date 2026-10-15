// P5-12b1 (bases and Panesterra; the D1 split of P5-12b, m5i-plan.md 15.5), M5a subset (m5a-plan.md E1-07): split from tests/worldevents/WorldEventsM5aTest.cpp
// with its fixture, under this file's own suite name.
//
// Expectations are derived by hand from BaseService.java:30-51 and PanesterraService.java:238-274.

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

class BaseAndPanesterraM5aTest : public PlayerEventsTest {
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

TEST_F(BaseAndPanesterraM5aTest, PanesterraIgnoresNormalMaps) {
	PlayerFixture f = makePlayerInPoeta(3);
	runtime::resetUnportedHitsForTests();
	services::panesterra::PanesterraService::getInstance().onEnterPanesterra(*f.player); // getSiegeId(210010000) == 0
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(BaseAndPanesterraM5aTest, BaseServiceIsAWarnStubUntilTheBaseLocationsArePorted) {
	runtime::resetPartialHitsForTests();
	runtime::resetUnportedHitsForTests();
	services::BaseService& service = services::BaseService::getInstance();
	service.initBases(); // no base locations: nothing starts
	EXPECT_EQ(runtime::partialHitCount(), 1u) << "the constructor's AION_PARTIAL";
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

} // namespace
} // namespace aion::gameserver::playerevents::test
