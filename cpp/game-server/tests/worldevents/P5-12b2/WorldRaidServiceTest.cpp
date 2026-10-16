// P5-12b2, M5i I-5 (m5i-plan.md 15.3): WorldRaidService's start and stop guards and WorldRaidRunnable's choice of raid locations, over test
// locations published as WORLD_RAID_DATA (world raids enabled for the case).
//
// Expectations are derived by hand from WorldRaidService.java:73-117 and WorldRaidRunnable.java:27-57. A raid's timeline (the flag, the vortex,
// the markers, the boss and its death) spawns npcs on a world map: the gate's (I-11, a world raid's first stage). A raid stopped before its
// first minute has no flag yet, and WorldRaid.onWorldRaidFinish dereferences it: Java's NullPointerException, kept.

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "../../playersvc/PlayerEventsTestSupport.h"
#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/configs/schedule/WorldRaidSchedules.bind.h"
#include "aion/gameserver/configs/schedule/WorldRaidSchedules.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WorldRaidData.bind.h"
#include "aion/gameserver/dataholders/WorldRaidData.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/WorldRaidService.h"
#include "aion/gameserver/services/worldraid/WorldRaid.h"
#include "aion/gameserver/services/worldraid/WorldRaidRunnable.h"

namespace aion::gameserver::playerevents::test {
namespace {

constexpr const char* RAID_LOCATIONS_XML =
	R"(<world_raid_locations>)"
	R"(<world_raid_location location_id="1" map_id="210050000" x="1" y="2" z="3"><world_raid_npcs><world_raid_npc npc_id="235000"/></world_raid_npcs><location_markers><spot x="1" y="1" z="1"/></location_markers></world_raid_location>)"
	R"(<world_raid_location location_id="2" map_id="210050000" x="4" y="5" z="6"><world_raid_npcs><world_raid_npc npc_id="235001"/></world_raid_npcs><location_markers><spot x="1" y="1" z="1"/></location_markers></world_raid_location>)"
	R"(<world_raid_location location_id="3" map_id="220070000" x="7" y="8" z="9"><world_raid_npcs><world_raid_npc npc_id="235002"/></world_raid_npcs><location_markers><spot x="1" y="1" z="1"/></location_markers></world_raid_location>)"
	R"(</world_raid_locations>)";

class WorldRaidServiceTest : public PlayerEventsTest {
protected:
	void SetUp() override {
		PlayerEventsTest::SetUp();
		enabled.emplace(configs::main::EventsConfig::ENABLE_WORLDRAID, true);
		dataholders::DataManager::WORLD_RAID_DATA.publish(xml::bindString<dataholders::WorldRaidData>(context, RAID_LOCATIONS_XML));
		services::WorldRaidService::getInstance().initWorldRaidLocations();
	}

	void TearDown() override {
		// the active raids of the cases never reached their first minute (the fixture's executor is not advanced)
		for (int32_t id : {1, 2, 3})
			dropRaid(id);
		enabled.reset();
		services::WorldRaidService::getInstance().initWorldRaidLocations(); // disabled again: no locations into the holder reset below
		dataholders::DataManager::WORLD_RAID_DATA.resetForTests();
		PlayerEventsTest::TearDown();
	}

	/** stopRaid removes the raid, then its finish throws Java's NullPointerException for the flag a fresh raid has not spawned yet */
	static void dropRaid(int32_t id) {
		try {
			services::WorldRaidService::getInstance().stopRaid(id);
		} catch (const runtime::NullPointerException&) {
		}
	}

	const configs::schedule::WorldRaidSchedules::WorldRaidSchedule& schedule(const std::string& attributes) {
		schedules.push_back(xml::bindString<configs::schedule::WorldRaidSchedules>(context,
			"<world_raid_schedules><world_raid_schedule id=\"raid\" " + attributes + "/></world_raid_schedules>"));
		return schedules.back()->getWorldRaidSchedules().front();
	}

	xml::LoadContext context;
	std::vector<std::unique_ptr<configs::schedule::WorldRaidSchedules>> schedules;
	std::optional<AtomicConfigScope<bool>> enabled;
};

} // namespace
} // namespace aion::gameserver::playerevents::test

namespace aion::gameserver::playerevents::test {
namespace {

TEST_F(WorldRaidServiceTest, OnlyAValidIdleLocationStartsARaid) {
	services::WorldRaidService& service = services::WorldRaidService::getInstance();
	service.startRaid(99, false); // logs "Attempt to start world raid for an invalid location"
	EXPECT_FALSE(service.isWorldRaidInProgress(99));
	service.startRaid(1, false);
	EXPECT_TRUE(service.isWorldRaidInProgress(1));
	service.startRaid(1, true); // logs "Attempt to start world raid twice"
	EXPECT_EQ(service.getActiveWorldRaidLocations().size(), 1u);
	EXPECT_EQ(service.getActiveWorldRaidLocations().front()->getLocationId(), 1);
}

TEST_F(WorldRaidServiceTest, StoppingAnUnknownRaidDoesNothingAndAFreshRaidHasNoFlagYet) {
	services::WorldRaidService& service = services::WorldRaidService::getInstance();
	service.stopRaid(2); // "Attempt to stop world raid twice"
	service.startRaid(2, false);
	EXPECT_THROW(service.stopRaid(2), runtime::NullPointerException) << "Java: despawnNpcs(flag, vortex, boss) with a null flag";
	EXPECT_FALSE(service.isWorldRaidInProgress(2)) << "removed before the raid stops";
}

TEST_F(WorldRaidServiceTest, AScheduleWithAnInvalidOrRunningLocationStartsNothing) {
	services::WorldRaidService& service = services::WorldRaidService::getInstance();
	services::worldraid::WorldRaidRunnable{.worldRaidSchedule = &schedule(R"(locations="1 99")")}();
	EXPECT_TRUE(service.getActiveWorldRaidLocations().empty()) << "99 is no location";
	service.startRaid(3, false);
	services::worldraid::WorldRaidRunnable{.worldRaidSchedule = &schedule(R"(locations="1 3")")}();
	EXPECT_FALSE(service.isWorldRaidInProgress(1)) << "3 is running: the whole schedule is refused";
}

TEST_F(WorldRaidServiceTest, AScheduleStartsMinCountOfItsLocations) {
	services::WorldRaidService& service = services::WorldRaidService::getInstance();
	services::worldraid::WorldRaidRunnable{.worldRaidSchedule = &schedule(R"(locations="1 2 3" min_count="2" is_special_raid="true")")}();
	EXPECT_EQ(service.getActiveWorldRaidLocations().size(), 2u) << "min_count caps the count";
	for (int32_t id : {1, 2, 3})
		dropRaid(id);
	services::worldraid::WorldRaidRunnable{.worldRaidSchedule = &schedule(R"(locations="1 2 3")")}();
	EXPECT_EQ(service.getActiveWorldRaidLocations().size(), 3u) << "no counts: every location";
}

} // namespace
} // namespace aion::gameserver::playerevents::test
