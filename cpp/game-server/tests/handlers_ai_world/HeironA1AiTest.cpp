// A1, lane C, slice 2 (worlds/heiron): BollvigAI and KlawspawnAI (game-server/data/handlers/ai/worlds/heiron) on npcs of a real Heiron map
// instance (Bollvig's summons and his 204655 spawn at (1001, 2828)), each AI installed with replaceAi. The casts' effects are left to a retest.

#include "ZoneA1TestSupport.h"

#include <chrono>
#include <cmath>
#include <cstdint>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/handlers/ai/worlds/heiron/BollvigAI.h"
#include "aion/gameserver/handlers/ai/worlds/heiron/KlawspawnAI.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"

#define AION_HEIRON_AIS(X)                                                                                                                    \
	X(BollvigAI)                                                                                                                              \
	X(KlawspawnAI)

#define AION_DECLARE_FACTORY(cls)                                                                                                             \
	namespace aion::gameserver::handlers::ai::worlds::heiron {                                                                                \
	::aion::gameserver::handlers::AIFactory cls##_aiFactory;                                                                                  \
	}
AION_HEIRON_AIS(AION_DECLARE_FACTORY)
#undef AION_DECLARE_FACTORY

namespace aion::gameserver::handlers::ai::worlds::test {
namespace {

namespace hei = ::aion::gameserver::handlers::ai::worlds::heiron;
using model::gameobjects::Creature;
using model::gameobjects::Npc;

template <class AI>
struct Probe : AI {
	using AI::AI;
	using AI::handleAttack;
	using AI::handleBackHome;
	using AI::handleDespawned;
	using AI::handleDied;
	using AI::handleSpawned;
};

class HeironA1AiTest : public ZoneA1AiTest {
protected:
	void SetUp() override {
		ZoneA1AiTest::SetUp();
		open(HEIRON, 1001.0f, 2840.0f, 235.66f); // 12 m from Bollvig's spot
		publishSkills({17861, 18034, 18037, 17899, 18025, 18027});
	}

	void aggro(Npc& npc) {
		ZoneKnownListPairing::pair(npc, player());
		npc.getAggroList().addDamage(player(), 1, true, std::nullopt);
		npc.setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(player()));
	}

	/** every npc of the id is 10 m from (1001, 2828) */
	void expectRing(int32_t npcId) {
		for (const runtime::Ptr<Npc>& npc : npcs(*instance, npcId))
			EXPECT_NEAR(std::hypot(npc->getX() - 1001.0f, npc->getY() - 2828.0f), 10.0f, 0.01f) << npcId;
	}
};

/** The two AION_AI factories build their classes */
TEST_F(HeironA1AiTest, TheFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(900200, 1001.0f, 2828.0f, 235.66f);
#define AION_CHECK_FACTORY(cls)                                                                                                               \
	{                                                                                                                                         \
		auto ai = hei::cls##_aiFactory(*npc);                                                                                               \
		EXPECT_NE(dynamic_cast<hei::cls*>(ai.get()), nullptr) << #cls;                                                                        \
	}
	AION_HEIRON_AIS(AION_CHECK_FACTORY)
#undef AION_CHECK_FACTORY
}

/** BollvigAI.java:34-40, 142-165: his spawn takes 204655 away; his death brings it back while no living 212314 stands there */
TEST_F(HeironA1AiTest, BollvigSwapsWith204655) {
	runtime::Ptr<Npc> keeper = place(204655, 1001.0f, 2828.0f, 235.66f);
	runtime::Ptr<Npc> bollvig = place(900200, 1001.0f, 2828.0f, 235.66f);
	auto& ai = install<Probe<hei::BollvigAI>>(*bollvig);
	ai.handleSpawned();
	EXPECT_FALSE(keeper->isSpawned());
	runtime::Ptr<Npc> other = place(212314, 1010.0f, 2828.0f, 235.66f);
	ai.handleDied();
	EXPECT_EQ(count(204655), 0u) << "212314 is alive";
	other->getController().delete_();
	runtime::Ptr<Npc> second = place(900300, 1001.0f, 2828.0f, 235.66f);
	auto& secondAi = install<Probe<hei::BollvigAI>>(*second);
	secondAi.handleDespawned();
	ASSERT_EQ(count(204655), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 204655)[0]->getX(), 1001.0f);
	secondAi.handleDied();
	EXPECT_EQ(count(204655), 1u) << "not while one stands";
}

/** BollvigAI.java:48-83: at 75 % two 280802 and two 280803 10 m around his spot; at 50 % again, and 10 s later a 280804 */
TEST_F(HeironA1AiTest, BollvigsPhasesCallHisSummons) {
	runtime::Ptr<Npc> bollvig = place(900200, 1001.0f, 2828.0f, 235.66f);
	auto& ai = install<Probe<hei::BollvigAI>>(*bollvig);
	aggro(*bollvig);
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	EXPECT_EQ(count(280802), 0u) << "at full HP no phase";
	bollvig->getLifeStats()->setCurrentHpPercent(74);
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	EXPECT_EQ(count(280802), 2u);
	EXPECT_EQ(count(280803), 2u);
	expectRing(280802);
	expectRing(280803);
	executor().advance(std::chrono::seconds(10));
	EXPECT_EQ(count(280804), 0u) << "above 50 % no Nerve Absorption";
	bollvig->getLifeStats()->setCurrentHpPercent(49);
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	EXPECT_EQ(count(280802), 4u);
	executor().advance(std::chrono::milliseconds(9999));
	EXPECT_EQ(count(280804), 0u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count(280804), 1u);
	expectRing(280804);
	ai.handleDied();
	EXPECT_EQ(count(280802) + count(280803) + count(280804), 0u) << "his death takes his summons";
}

/** BollvigAI.java:85-100: between 50 and 25 % the round comes back every 46 s (31 s, 5 s, then 10 s to the next 280804) */
TEST_F(HeironA1AiTest, BollvigsRoundRepeats) {
	runtime::Ptr<Npc> bollvig = place(900200, 1001.0f, 2828.0f, 235.66f);
	auto& ai = install<Probe<hei::BollvigAI>>(*bollvig);
	aggro(*bollvig);
	bollvig->getLifeStats()->setCurrentHpPercent(74);
	ai.handleAttack(runtime::Ptr<Creature>(player())); // 75 %
	bollvig->getLifeStats()->setCurrentHpPercent(40);
	ai.handleAttack(runtime::Ptr<Creature>(player())); // 50 %: the first 280804 10 s later, Charming Attraction 31 s later
	executor().advance(std::chrono::seconds(10));
	EXPECT_EQ(count(280804), 1u);
	executor().advance(std::chrono::seconds(21 + 5 + 10)); // 31 s: Charming Attraction, 5 s: Curse of Soul and the round, 10 s: 280804
	EXPECT_EQ(count(280804), 2u);
	ai.handleBackHome();
}

/** KlawspawnAI.java:22-48: no thinking, 1 damage; while no 212120 is there an attack may call one and die; its death deletes it */
TEST_F(HeironA1AiTest, AKlawspawnMayCallItsMother) {
	commons::utils::Rnd::seedCurrentThreadForTests(16);
	runtime::Ptr<Npc> klawspawn = place(900200, 1001.0f, 2835.0f, 235.66f);
	auto& ai = install<Probe<hei::KlawspawnAI>>(*klawspawn);
	EXPECT_FALSE(ai.canThink());
	EXPECT_FLOAT_EQ(ai.modifyDamage(player(), 5000.0f, nullptr), 1.0f);
	ZoneKnownListPairing::pair(*klawspawn, player());
	int32_t attacks = 0;
	while (count(212120) == 0 && attacks < 1000) {
		ai.handleAttack(runtime::Ptr<Creature>(player()));
		attacks++;
	}
	ASSERT_EQ(count(212120), 1u);
	EXPECT_GT(attacks, 1) << "about one attack in ten";
	EXPECT_FALSE(klawspawn->isSpawned()) << "it died and was deleted";
	runtime::Ptr<Npc> second = place(900300, 1001.0f, 2836.0f, 235.66f);
	auto& secondAi = install<Probe<hei::KlawspawnAI>>(*second);
	for (int32_t i = 0; i < 200; i++)
		secondAi.handleAttack(runtime::Ptr<Creature>(player()));
	EXPECT_EQ(count(212120), 1u) << "not while one is there";
	EXPECT_TRUE(second->isSpawned());
}

} // namespace
} // namespace aion::gameserver::handlers::ai::worlds::test
