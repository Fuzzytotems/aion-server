// P5-12b PanesterraService::teleportToStartPosition (m5b-plan.md C-03): the body PlayerReviveService::bindRevive calls for every character that
// is neither in prison nor in EVENT_MODE, which is the M5b-1 gate's death case on Poeta. Until this wave it was AION_UNPORTED and every
// bind-revive outside those two states threw.
//
// Java: PanesterraService.java:297-307.
//
// The test lives in tests/playersvc rather than in tests/worldevents because the only caller at M5b-1 is P5-08's revive path, and the case
// this wave had to unblock is that caller's. The second case below is what keeps the first one honest: with getTeam(Player&) still
// AION_UNPORTED (nothing fills activeFactionTeams before createTeams is ported), a Panesterra map is the one input on which the body is
// *observably* allowed to continue past its first statement. Delete the `isPanesterraMap` guard and the Poeta case throws instead of
// answering false - which is exactly what the gate would see.

#include "../cm_ak/InWorldPacketRunSupport.h"

#include <cstdint>
#include <string>

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/panesterra/PanesterraService.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using services::panesterra::PanesterraService;

/** The five world ids WorldMapType.isPanesterraMap answers true for (WorldMapTypeInfo.h:102-118) */
constexpr int32_t BELUS = 400020000;
constexpr int32_t TRANSIDIUM_ANNEX = 400030000;
constexpr int32_t ASPIDA = 400040000;
constexpr int32_t ATANATOS = 400050000;
constexpr int32_t DISILLON = 400060000;
/** the gate's map, and the one every M5b-1 death happens on */
constexpr int32_t POETA = 210010000;

class PanesterraStartPositionTest : public InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		actor = makePlayer(500001, 9501, "Panesterran");
		actor.player->getPosition()->setIsSpawned(true);
	}

	void TearDown() override {
		if (actor.player)
			actor.player->setTarget(nullptr);
		actor = {};
		InWorldPacketTest::TearDown();
	}

	/** Moves the fixture's character to a map without touching the World (the body reads getWorldId() only) */
	void putOnMap(int32_t worldId) { actor.player->setPosition(world::WorldPosition::create(worldId, 100.0f, 100.0f, 50.0f, int8_t{0})); }

	PlayerFixture actor;
};

TEST_F(PanesterraStartPositionTest, AnswersFalseOnAMapOutsidePanesterraWithoutLookingForATeam) {
	putOnMap(POETA);
	ASSERT_FALSE(world::isPanesterraMap(actor.player->getWorldId()));
	runtime::resetUnportedHitsForTests();

	// Java PanesterraService.java:298-299: `if (!WorldMapType.isPanesterraMap(player.getWorldId())) return false;`
	EXPECT_FALSE(PanesterraService::getInstance().teleportToStartPosition(*actor.player));
	// the whole point of the guard for M5b-1: the body must *return*, not reach getTeam(Player&), which is still AION_UNPORTED
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "the map guard must answer before the team lookup";
}

TEST_F(PanesterraStartPositionTest, TheMapGuardCoversAllFivePanesterraWorlds) {
	// the guard is `!isPanesterraMap`, not `worldId != BELUS`: every one of the five continues into the team lookup, which throws today.
	// Ishalgen and Reshanta, two ordinary maps, answer false with no lookup at all.
	for (int32_t worldId : {BELUS, TRANSIDIUM_ANNEX, ASPIDA, ATANATOS, DISILLON}) {
		putOnMap(worldId);
		EXPECT_THROW(PanesterraService::getInstance().teleportToStartPosition(*actor.player), runtime::UnportedException)
			<< "world " << worldId << " is a Panesterra map and must reach getTeam";
	}
	for (int32_t worldId : {220010000, 400010000}) {
		putOnMap(worldId);
		EXPECT_FALSE(PanesterraService::getInstance().teleportToStartPosition(*actor.player)) << "world " << worldId;
	}
}

TEST_F(PanesterraStartPositionTest, OnAPanesterraMapTheBodyContinuesIntoTheTeamLookup) {
	putOnMap(ASPIDA);
	ASSERT_TRUE(world::isPanesterraMap(actor.player->getWorldId()));

	// Java PanesterraService.java:301: `PanesterraTeam team = getTeam(player);`. getTeam(Player&) is the next statement and is AION_UNPORTED
	// until the Panesterra work; the throw is what proves the guard let this map through and that the second statement is the team lookup.
	try {
		PanesterraService::getInstance().teleportToStartPosition(*actor.player);
		FAIL() << "getTeam(Player&) is unported";
	} catch (const runtime::UnportedException& unported) {
		EXPECT_NE(std::string(unported.what()).find("getTeam"), std::string::npos) << unported.what();
	}
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
