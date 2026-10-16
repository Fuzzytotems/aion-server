// A1, lane C, slice 6 (worlds/pernon): GaleCycloneAI (game-server/data/handlers/ai/worlds/pernon) on an npc of a real Pernon map instance,
// installed with replaceAi. The observers' life is checked here; the cast of 20528 (a plain template here, no effect) is left to a retest.

#include "ZoneA1TestSupport.h"

#include <cstdint>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/handlers/ai/worlds/pernon/GaleCycloneAI.h"

namespace aion::gameserver::handlers::ai::worlds::pernon {
::aion::gameserver::handlers::AIFactory GaleCycloneAI_aiFactory;
}

namespace aion::gameserver::handlers::ai::worlds::test {
namespace {

namespace per = ::aion::gameserver::handlers::ai::worlds::pernon;
using model::gameobjects::Npc;

struct Probe : per::GaleCycloneAI {
	using GaleCycloneAI::GaleCycloneAI;
	using GaleCycloneAI::handleCreatureSee;
	using GaleCycloneAI::handleDespawned;
	using GaleCycloneAI::handleDied;
};

class PernonA1AiTest : public ZoneA1AiTest {
protected:
	void SetUp() override {
		ZoneA1AiTest::SetUp();
		open(PERNON, 1000.0f, 1000.0f, 100.0f);
		publishSkills({20528});
	}

	void moveTo(float x, float y) {
		world::World::getInstance().updatePosition(player(), x, y, player().getZ(), int8_t{0}, false);
		player().getObserveController()->notifyMoveObservers();
	}
};

/** The AION_AI factory builds its class */
TEST_F(PernonA1AiTest, TheFactoryBuildsItsClass) {
	runtime::Ptr<Npc> npc = place(900200, 1020.0f, 1000.0f, 100.0f);
	auto ai = per::GaleCycloneAI_aiFactory(*npc);
	EXPECT_NE(dynamic_cast<per::GaleCycloneAI*>(ai.get()), nullptr);
}

/** GaleCycloneAI.java:28-53: one observer per player it sees (npcs get none); its death removes them */
TEST_F(PernonA1AiTest, ACycloneWatchesThePlayersItSees) {
	runtime::Ptr<Npc> cyclone = place(900200, 1020.0f, 1000.0f, 100.0f);
	auto& ai = install<Probe>(*cyclone);
	ZoneKnownListPairing::pair(*cyclone, player());
	player().getObserveController()->clear();
	ASSERT_FALSE(player().getObserveController()->hasObservers());
	ai.handleCreatureSee(*place(900300, 1021.0f, 1000.0f, 100.0f));
	ai.handleCreatureSee(player());
	ai.handleCreatureSee(player());
	EXPECT_TRUE(player().getObserveController()->hasObservers());
	moveTo(1015.0f, 1000.0f); // into 12 m: the cast
	moveTo(1000.0f, 1000.0f);
	EXPECT_TRUE(player().getObserveController()->hasObservers()) << "still seen";
	ai.handleDied();
	EXPECT_FALSE(player().getObserveController()->hasObservers());
	ai.handleCreatureSee(player());
	EXPECT_TRUE(player().getObserveController()->hasObservers()) << "removed from the map, a new one can come";
	ai.handleDespawned();
	EXPECT_FALSE(player().getObserveController()->hasObservers());
}

/** GaleCycloneAI.java:68-74: a move the cyclone no longer sees removes the player's observer */
TEST_F(PernonA1AiTest, AnUnseenPlayerIsLetGo) {
	runtime::Ptr<Npc> cyclone = place(900200, 1020.0f, 1000.0f, 100.0f);
	auto& ai = install<Probe>(*cyclone);
	ZoneKnownListPairing::pair(*cyclone, player());
	player().getObserveController()->clear();
	ai.handleCreatureSee(player());
	ASSERT_TRUE(player().getObserveController()->hasObservers());
	world::World::getInstance().updatePosition(player(), 2500.0f, 2500.0f, player().getZ(), int8_t{0}, true); // out of sight
	ASSERT_FALSE(cyclone->getKnownList().sees(player()));
	player().getObserveController()->notifyMoveObservers();
	EXPECT_FALSE(player().getObserveController()->hasObservers());
	ai.handleCreatureSee(player());
	EXPECT_TRUE(player().getObserveController()->hasObservers()) << "onRemoved took it out of the map";
	ai.handleDied();
}

} // namespace
} // namespace aion::gameserver::handlers::ai::worlds::test
