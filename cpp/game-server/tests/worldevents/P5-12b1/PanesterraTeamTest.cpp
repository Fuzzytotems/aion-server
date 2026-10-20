// M5i I-10 (P5-12b1): PanesterraTeam's member list and positions (PanesterraTeam.java:65-126) - adding once, the membership query and the
// count, Java's remove(int) taken as an index (kept, P5-12b.md), the members iterated through the World (none online here), and the start
// position of a faction without one. Teleporting members is the gate's (Ahserion's Flight is far future, m5i-plan.md D6).

#include <gtest/gtest.h>

#include <string>

#include "../../playersvc/PlayerEventsTestSupport.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/MaterialData.bind.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/dataholders/ShieldData.bind.h"
#include "aion/gameserver/dataholders/ShieldData.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/ZoneData.bind.h"
#include "aion/gameserver/dataholders/ZoneData.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraFaction.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraTeam.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::playerevents::test {
namespace {

using services::panesterra::ahserion::PanesterraFaction;
using services::panesterra::ahserion::PanesterraTeam;

class PanesterraTeamTest : public PlayerEventsTest {
protected:
	void SetUp() override {
		PlayerEventsTest::SetUp();
		// World (World.getPlayer) needs its holders: none, published once for the executable and never reset (as EventBuffsAndHeadhunterTest)
		static const bool worldPublished = [] {
			static xml::LoadContext worldContext;
			if (!dataholders::DataManager::WORLD_MAPS_DATA)
				dataholders::DataManager::WORLD_MAPS_DATA.publish(xml::bindString<dataholders::WorldMapsData>(worldContext, "<world_maps/>"));
			if (!dataholders::DataManager::ZONE_DATA)
				dataholders::DataManager::ZONE_DATA.publish(xml::bindString<dataholders::ZoneData>(worldContext, "<zones/>"));
			if (!dataholders::DataManager::SHIELD_DATA)
				dataholders::DataManager::SHIELD_DATA.publish(xml::bindString<dataholders::ShieldData>(worldContext, "<shields/>"));
			if (!dataholders::DataManager::MATERIAL_DATA)
				dataholders::DataManager::MATERIAL_DATA.publish(xml::bindString<dataholders::MaterialData>(worldContext, "<material_templates/>"));
			return true;
		}();
		static_cast<void>(worldPublished);
	}
};

TEST_F(PanesterraTeamTest, MembersAreAddedOnceAndCounted) {
	runtime::resetUnportedHitsForTests();
	runtime::Ref<PanesterraTeam> team = PanesterraTeam::create(PanesterraFaction::BELUS);
	EXPECT_EQ(team->getMemberCount(), 0);
	team->addTeamMemberIfAbsent(101);
	team->addTeamMemberIfAbsent(101);
	team->addTeamMemberIfAbsent(102);
	EXPECT_EQ(team->getMemberCount(), 2);
	EXPECT_TRUE(team->isTeamMember(101));
	EXPECT_FALSE(team->isTeamMember(103));
	team->removeTeamMember(103); // not a member: nothing
	EXPECT_EQ(team->getMemberCount(), 2);
	int32_t visited = 0;
	team->forEachMember([&visited](model::gameobjects::player::Player&) { visited++; });
	EXPECT_EQ(visited, 0) << "no member is online in the World";
	team->moveTeamMembersToOriginPosition(); // nobody online: nothing
	EXPECT_EQ(team->getFaction(), PanesterraFaction::BELUS);
	EXPECT_EQ(team->getStartPosition()->getMapId(), 400030000);
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(PanesterraTeamTest, RemovingAMemberRemovesItsId) {
	// PanesterraTeam.java:107-110, corrected (owner's decision 2026-10-19): Java's remove(int) took the id as an index; the C++ removes the id
	runtime::Ref<PanesterraTeam> team = PanesterraTeam::create(PanesterraFaction::IVY_TEMPLE);
	team->addTeamMemberIfAbsent(2);
	team->addTeamMemberIfAbsent(101);
	team->addTeamMemberIfAbsent(102);
	team->removeTeamMember(2); // an id that is also an index: Java's remove(int) took 102, the element at index 2
	EXPECT_EQ(team->getMemberCount(), 2);
	EXPECT_FALSE(team->isTeamMember(2));
	EXPECT_TRUE(team->isTeamMember(102));
	team->removeTeamMember(101); // Java: IndexOutOfBoundsException, index 101 of a list of 2
	EXPECT_EQ(team->getMemberCount(), 1);
	EXPECT_TRUE(team->isTeamMember(102));
}

TEST_F(PanesterraTeamTest, AFactionWithoutAStartPositionCannotBeMovedThere) {
	runtime::Ref<PanesterraTeam> peace = PanesterraTeam::create(PanesterraFaction::PEACE);
	EXPECT_FALSE(peace->getStartPosition());
	PlayerFixture f = makePlayer(51, 5100);
	EXPECT_THROW(peace->movePlayerToStartPosition(*f.player), runtime::NullPointerException);
}

} // namespace
} // namespace aion::gameserver::playerevents::test
