// A1 cp5 (docs/design/a1-laneb-plan.md; the owner moved Ahserion's Flight into phase 6 on 2026-10-15): AhserionRaid's paths that need no map
// 400030000, no spawn data and no teams, and PanesterraService's start/stop/isStarted around it (PanesterraService.java:151-171, 276-278).
// Not covered here, and by no gate (the raid cannot run in a gate yet): the raid's spawns, its 30 s progress steps, the door opening, the
// corridor shield eliminations with teams, the rewards and the clean-up of map 400030000.

#include <gtest/gtest.h>

#include "../../playersvc/PlayerEventsTestSupport.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/panesterra/PanesterraService.h"
#include "aion/gameserver/services/panesterra/ahserion/AhserionRaid.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraFaction.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraTeam.h"

namespace aion::gameserver::playerevents::test {
namespace {

using services::panesterra::PanesterraService;
using services::panesterra::ahserion::AhserionRaid;
using services::panesterra::ahserion::PanesterraFaction;
using services::panesterra::ahserion::PanesterraTeam;

class AhserionRaidTest : public PlayerEventsTest {};

/** AhserionRaid.java:56-62, 300-302; PanesterraService.java:162-171, 276-278: a raid that never started stops as a no-op */
TEST_F(AhserionRaidTest, ARaidThatNeverStartedStopsQuietly) {
	runtime::resetUnportedHitsForTests();
	EXPECT_FALSE(AhserionRaid::getInstance().isStarted());
	EXPECT_FALSE(PanesterraService::getInstance().isAhserionRaidStarted());
	AhserionRaid::getInstance().stop();
	PanesterraService::getInstance().stopAhserionRaid(); // no team to eliminate
	EXPECT_FALSE(AhserionRaid::getInstance().isStarted());
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

/** AhserionRaid.java:214-216: a corridor shield destroyed outside the raid changes nothing */
TEST_F(AhserionRaidTest, ACorridorShieldOutsideTheRaidIsIgnored) {
	AhserionRaid::getInstance().handleCorridorShieldDestruction(297306);
	AhserionRaid::getInstance().handleCorridorShieldDestruction(1); // not a shield: still nothing, the raid is not started
	EXPECT_FALSE(AhserionRaid::getInstance().isStarted());
}

/** AhserionRaid.java:194-198, 287-293: without teams forEachTeam visits none and a team's stage spawns nothing */
TEST_F(AhserionRaidTest, WithoutTeamsNoTeamIsVisitedAndNoTeamStageSpawns) {
	int32_t visited = 0;
	AhserionRaid::getInstance().forEachTeam([&visited](PanesterraTeam&) { visited++; });
	EXPECT_EQ(visited, 0);
	for (PanesterraFaction faction : {PanesterraFaction::BELUS, PanesterraFaction::ASPIDA, PanesterraFaction::ATANATOS, PanesterraFaction::DISILLON})
		AhserionRaid::getInstance().spawnStage(0, faction); // no team: returns before the spawn data
}

} // namespace
} // namespace aion::gameserver::playerevents::test
