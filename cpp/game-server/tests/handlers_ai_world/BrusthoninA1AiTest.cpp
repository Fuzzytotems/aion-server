// A1, lane C, slice 8 (worlds/brusthonin): UnfaithfulNtuamuAI (game-server/data/handlers/ai/worlds/brusthonin) on an npc of a real Brusthonin
// map instance, installed with replaceAi on a spawn that respawns after 10 s. The respawn is checked as RespawnService's pending task (its
// run needs the rift data this fixture has not got) and cancelled.

#include "ZoneA1TestSupport.h"

#include <cstdint>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/handlers/ai/worlds/brusthonin/UnfaithfulNtuamuAI.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/services/RespawnService.h"

namespace aion::gameserver::handlers::ai::worlds::brusthonin {
::aion::gameserver::handlers::AIFactory UnfaithfulNtuamuAI_aiFactory;
}

namespace aion::gameserver::handlers::ai::worlds::test {
namespace {

namespace bru = ::aion::gameserver::handlers::ai::worlds::brusthonin;
using model::gameobjects::Creature;
using model::gameobjects::Npc;

struct Probe : bru::UnfaithfulNtuamuAI {
	using UnfaithfulNtuamuAI::UnfaithfulNtuamuAI;
	using UnfaithfulNtuamuAI::handleAttack;
};

class BrusthoninA1AiTest : public ZoneA1AiTest {
protected:
	void SetUp() override {
		ZoneA1AiTest::SetUp();
		open(BRUSTHONIN, 1000.0f, 1000.0f, 300.0f);
	}
};

/** The AION_AI factory builds its class */
TEST_F(BrusthoninA1AiTest, TheFactoryBuildsItsClass) {
	runtime::Ptr<Npc> npc = place(900200, 1002.0f, 1000.0f, 300.0f);
	auto ai = bru::UnfaithfulNtuamuAI_aiFactory(*npc);
	EXPECT_NE(dynamic_cast<bru::UnfaithfulNtuamuAI*>(ai.get()), nullptr);
}

/** UnfaithfulNtuamuAI.java:24-37: at 50 % she becomes the Vampire Queen at half HP and is deleted; she comes back with the queen's death */
TEST_F(BrusthoninA1AiTest, NtuamuBecomesTheVampireQueen) {
	runtime::Ptr<Npc> ntuamu = placeRespawning(900200, 1002.0f, 1000.0f, 300.0f, 10);
	auto& ai = install<Probe>(*ntuamu);
	ZoneKnownListPairing::pair(*ntuamu, player());
	ntuamu->getAggroList().addDamage(player(), 1, true, std::nullopt);
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	EXPECT_EQ(count(214583), 0u) << "above 50 %";
	ntuamu->getLifeStats()->setCurrentHpPercent(50);
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	ASSERT_EQ(count(214583), 1u);
	runtime::Ptr<Npc> queen = npcs(*instance, 214583)[0];
	EXPECT_EQ(queen->getLifeStats()->getHpPercentage(), 50);
	EXPECT_FLOAT_EQ(queen->getX(), 1002.0f);
	EXPECT_FALSE(ntuamu->isSpawned());
	EXPECT_FALSE(services::RespawnService::hasRespawnTask(*ntuamu)) << "no respawn before the queen dies";
	queen->getObserveController()->notifyDeathObservers(player());
	EXPECT_TRUE(services::RespawnService::hasRespawnTask(*ntuamu));
	services::RespawnService::cancelRespawn(*ntuamu);
}

} // namespace
} // namespace aion::gameserver::handlers::ai::worlds::test
