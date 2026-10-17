// A1, lane C, slice 7 (worlds/eltnen): DracusBox and Kratia (game-server/data/handlers/ai/worlds/eltnen) on npcs of a real Eltnen map
// instance, each AI installed with replaceAi on a spawn that respawns after 10 s. The respawn is checked as RespawnService's pending task
// (its run needs the rift data this fixture has not got) and cancelled.

#include "ZoneA1TestSupport.h"

#include <cstdint>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/handlers/ai/worlds/eltnen/DracusBox.h"
#include "aion/gameserver/handlers/ai/worlds/eltnen/Kratia.h"
#include "aion/gameserver/services/RespawnService.h"

#define AION_ELTNEN_AIS(X)                                                                                                                    \
	X(DracusBox)                                                                                                                              \
	X(Kratia)

#define AION_DECLARE_FACTORY(cls)                                                                                                             \
	namespace aion::gameserver::handlers::ai::worlds::eltnen {                                                                                \
	::aion::gameserver::handlers::AIFactory cls##_aiFactory;                                                                                  \
	}
AION_ELTNEN_AIS(AION_DECLARE_FACTORY)
#undef AION_DECLARE_FACTORY

namespace aion::gameserver::handlers::ai::worlds::test {
namespace {

namespace elt = ::aion::gameserver::handlers::ai::worlds::eltnen;
using gameserver::ai::poll::AIQuestion;
using model::gameobjects::Npc;

template <class AI>
struct Probe : AI {
	using AI::AI;
	using AI::handleDied;
};

class EltnenA1AiTest : public ZoneA1AiTest {
protected:
	void SetUp() override {
		ZoneA1AiTest::SetUp();
		open(ELTNEN, 1000.0f, 1000.0f, 300.0f);
	}

	size_t crateSpawns() { return count(211792) + count(211799) + count(211800); }
};

/** The two AION_AI factories build their classes; neither respawns on its own */
TEST_F(EltnenA1AiTest, TheFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(900200, 1002.0f, 1000.0f, 300.0f);
#define AION_CHECK_FACTORY(cls)                                                                                                               \
	{                                                                                                                                         \
		auto ai = elt::cls##_aiFactory(*npc);                                                                                               \
		EXPECT_NE(dynamic_cast<elt::cls*>(ai.get()), nullptr) << #cls;                                                                        \
		EXPECT_FALSE(ai->ask(AIQuestion::ALLOW_RESPAWN)) << #cls;                                                                             \
	}
	AION_ELTNEN_AIS(AION_CHECK_FACTORY)
#undef AION_CHECK_FACTORY
}

/** DracusBox.java:26-42: the crate's death leaves one of three mobs and deletes it; it comes back at once, after Chaos Dracus with his death */
TEST_F(EltnenA1AiTest, TheMysteriousCrateHoldsOneOfThree) {
	commons::utils::Rnd::seedCurrentThreadForTests(16);
	bool sawDracus = false;
	bool sawOther = false;
	for (int32_t round = 0; round < 12 && !(sawDracus && sawOther); round++) {
		runtime::Ptr<Npc> crate = placeRespawning(900200, 1002.0f, 1000.0f, 300.0f, 10);
		auto& ai = install<Probe<elt::DracusBox>>(*crate);
		ai.handleDied();
		ASSERT_EQ(crateSpawns(), 1u);
		EXPECT_FALSE(crate->isSpawned()) << "deleted at once";
		if (count(211800) == 1) {
			sawDracus = true;
			EXPECT_FALSE(services::RespawnService::hasRespawnTask(*crate)) << "the crate waits for the death of Chaos Dracus";
			npcs(*instance, 211800)[0]->getObserveController()->notifyDeathObservers(player());
		} else {
			sawOther = true;
		}
		EXPECT_TRUE(services::RespawnService::hasRespawnTask(*crate)) << "the crate comes back";
		services::RespawnService::cancelRespawn(*crate);
		for (int32_t pass = 0; pass < 3; pass++) {
			for (const runtime::Ptr<Npc>& npc : npcs(*instance))
				npc->getController().delete_();
		}
	}
	EXPECT_TRUE(sawDracus);
	EXPECT_TRUE(sawOther);
}

/** Kratia.java:23-29: her death leaves Harpback where she stood; she comes back with his death */
TEST_F(EltnenA1AiTest, KratiaLeavesHarpback) {
	runtime::Ptr<Npc> kratia = placeRespawning(900200, 1002.0f, 1000.0f, 300.0f, 10);
	auto& ai = install<Probe<elt::Kratia>>(*kratia);
	ai.handleDied();
	ASSERT_EQ(count(211812), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 211812)[0]->getX(), 1002.0f);
	EXPECT_FALSE(services::RespawnService::hasRespawnTask(*kratia)) << "no respawn of her own";
	npcs(*instance, 211812)[0]->getObserveController()->notifyDeathObservers(player());
	EXPECT_TRUE(services::RespawnService::hasRespawnTask(*kratia));
	services::RespawnService::cancelRespawn(*kratia);
}

} // namespace
} // namespace aion::gameserver::handlers::ai::worlds::test
