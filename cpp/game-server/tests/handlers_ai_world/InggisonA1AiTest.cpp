// A1, lane C, slice 1 (worlds/inggison): the seven Inggison AIs (game-server/data/handlers/ai/worlds/inggison) on npcs of a real Inggison
// map instance (Sematariux spawns at his chamber's coordinates), each AI installed with replaceAi. The casts' effects (18671, 19186, 18726),
// the forced walk to the nest and Omega's summons percentages (an ai template) are left to a retest in game.

#include "ZoneA1TestSupport.h"

#include <chrono>
#include <cmath>
#include <cstdint>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/handlers/ai/worlds/inggison/CloneOfBarrierAI.h"
#include "aion/gameserver/handlers/ai/worlds/inggison/GeyserBlockerAI.h"
#include "aion/gameserver/handlers/ai/worlds/inggison/HugeEggAI.h"
#include "aion/gameserver/handlers/ai/worlds/inggison/OmegaAI.h"
#include "aion/gameserver/handlers/ai/worlds/inggison/SematariuxAI.h"
#include "aion/gameserver/handlers/ai/worlds/inggison/SematariuxEggAI.h"
#include "aion/gameserver/handlers/ai/worlds/inggison/SematariuxThunderShieldAI.h"
#include "aion/gameserver/model/stats/calc/AdditionStat.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

#define AION_INGGISON_AIS(X)                                                                                                                  \
	X(CloneOfBarrierAI)                                                                                                                       \
	X(GeyserBlockerAI)                                                                                                                        \
	X(HugeEggAI)                                                                                                                              \
	X(OmegaAI)                                                                                                                                \
	X(SematariuxAI)                                                                                                                           \
	X(SematariuxEggAI)                                                                                                                        \
	X(SematariuxThunderShieldAI)

#define AION_DECLARE_FACTORY(cls)                                                                                                             \
	namespace aion::gameserver::handlers::ai::worlds::inggison {                                                                              \
	::aion::gameserver::handlers::AIFactory cls##_aiFactory;                                                                                  \
	}
AION_INGGISON_AIS(AION_DECLARE_FACTORY)
#undef AION_DECLARE_FACTORY

namespace aion::gameserver::handlers::ai::worlds::test {
namespace {

namespace ing = ::aion::gameserver::handlers::ai::worlds::inggison;
using gameserver::ai::AIState;
using gameserver::ai::poll::AIQuestion;
using model::gameobjects::Creature;
using model::gameobjects::Npc;

// the protected handlers the cases drive directly (Java: the AI's events)
template <class AI>
struct Probe : AI {
	using AI::AI;
	using AI::handleAttack;
	using AI::handleBackHome;
	using AI::handleCreatureSee;
	using AI::handleDespawned;
	using AI::handleDied;
	using AI::handleSpawned;
};
struct OmegaProbe : ing::OmegaAI {
	using OmegaAI::OmegaAI;
	using OmegaAI::checkBeforeSpawn;
};

class InggisonA1AiTest : public ZoneA1AiTest {
protected:
	void SetUp() override {
		ZoneA1AiTest::SetUp();
		open(INGGISON, 140.0f, 2140.0f, 441.0f); // Sematariux's chamber
	}

	/** The player is on the npc's aggro list and they know each other */
	void aggro(Npc& npc) {
		ZoneKnownListPairing::pair(npc, player());
		npc.getAggroList().addDamage(player(), 1, true, std::nullopt);
	}

	void kill(Npc& npc) { npc.getObserveController()->notifyDeathObservers(player()); }
};

/** The seven AION_AI factories build their classes */
TEST_F(InggisonA1AiTest, TheFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(900200, 150.0f, 2140.0f, 441.0f);
#define AION_CHECK_FACTORY(cls)                                                                                                               \
	{                                                                                                                                         \
		auto ai = ing::cls##_aiFactory(*npc);                                                                                               \
		EXPECT_NE(dynamic_cast<ing::cls*>(ai.get()), nullptr) << #cls;                                                                        \
	}
	AION_INGGISON_AIS(AION_CHECK_FACTORY)
#undef AION_CHECK_FACTORY
}

/** CloneOfBarrierAI.java:21-31: 3 s after its spawn the clone targets its creator Omega (and casts 18671 on him) */
TEST_F(InggisonA1AiTest, TheBarrierCloneTargetsOmegaAfterThreeSeconds) {
	publishSkills({18671});
	runtime::Ptr<Npc> omega = place(900300, 150.0f, 2140.0f, 441.0f);
	runtime::Ptr<Npc> clone = placeCreatedBy(*omega, 900200, 152.0f, 2140.0f, 441.0f);
	ZoneKnownListPairing::pair(*clone, *omega);
	auto& ai = install<Probe<ing::CloneOfBarrierAI>>(*clone);
	ai.handleSpawned();
	executor().advance(std::chrono::milliseconds(2999));
	EXPECT_NE(clone->getTarget(), runtime::Ptr<model::gameobjects::VisibleObject>(omega));
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(clone->getTarget(), runtime::Ptr<model::gameobjects::VisibleObject>(omega));
	ai.handleDied(); // takes 18671 off Omega (he has none here: a no-op)
}

/** GeyserBlockerAI.java:29-48: a geyser seen within 5 m is deleted, the blocker's death brings it back with its static id at the spawn point */
TEST_F(InggisonA1AiTest, TheGeyserBlockerHoldsTheGeyserUntilItDies) {
	runtime::Ptr<Npc> blocker = place(900200, 200.0f, 2200.0f, 441.0f);
	auto& ai = install<Probe<ing::GeyserBlockerAI>>(*blocker);
	runtime::Ptr<Npc> far = spawnStaticNpc(*instance, 700545, 210.0f, 2200.0f, 441.0f, 4711);
	ai.handleCreatureSee(*far);
	EXPECT_TRUE(far->isSpawned()) << "10 m away";
	runtime::Ptr<Npc> geyser = spawnStaticNpc(*instance, 700545, 203.0f, 2200.0f, 441.0f, 4712);
	ai.handleCreatureSee(*geyser);
	EXPECT_FALSE(geyser->isSpawned());
	ASSERT_EQ(count(700545), 1u);
	ZoneKnownListPairing::pair(*blocker, player()); // the player stands 60 m away: no message
	ai.handleDied();
	ASSERT_EQ(count(700545), 2u);
	runtime::Ptr<Npc> back = npcs(*instance, 700545)[0] == far ? npcs(*instance, 700545)[1] : npcs(*instance, 700545)[0];
	EXPECT_EQ(back->getSpawn()->getStaticId(), 4712);
	EXPECT_FLOAT_EQ(back->getX(), 200.0f) << "at the blocker's spawn point";
	EXPECT_EQ(countSent(serialized(network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_WINDBOX_TRIGGER_ON_INFO())), 0);
}

/** GeyserBlockerAI.java:30-37: the players within 30 m are told; without a deleted geyser the death spawns nothing */
TEST_F(InggisonA1AiTest, TheGeyserBlockerTellsThePlayersNearby) {
	runtime::Ptr<Npc> blocker = place(900200, 150.0f, 2140.0f, 441.0f);
	auto& ai = install<Probe<ing::GeyserBlockerAI>>(*blocker);
	ZoneKnownListPairing::pair(*blocker, player());
	ai.handleCreatureSee(*spawnStaticNpc(*instance, 700545, 151.0f, 2140.0f, 441.0f, 4712));
	ai.handleDied();
	EXPECT_EQ(countSent(serialized(network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_WINDBOX_TRIGGER_ON_INFO())), 1);
	runtime::Ptr<Npc> other = place(900300, 160.0f, 2140.0f, 441.0f);
	auto& otherAi = install<Probe<ing::GeyserBlockerAI>>(*other);
	otherAi.handleDied();
	EXPECT_EQ(count(700545), 1u) << "no geyser was taken, none comes back";
}

/** HugeEggAI.java:22-39: no thinking, 1 damage, and at even odds its death hatches 217097 and deletes it */
TEST_F(InggisonA1AiTest, TheHugeEggHatchesAtEvenOdds) {
	commons::utils::Rnd::seedCurrentThreadForTests(16);
	int32_t hatched = 0;
	for (int32_t i = 0; i < 20; i++) {
		runtime::Ptr<Npc> egg = place(900200, 150.0f + i, 2140.0f, 441.0f);
		auto& ai = install<Probe<ing::HugeEggAI>>(*egg);
		EXPECT_FALSE(ai.canThink());
		EXPECT_FLOAT_EQ(ai.modifyDamage(player(), 5000.0f, nullptr), 1.0f);
		size_t before = count(217097);
		ai.handleDied();
		bool hatch = count(217097) == before + 1;
		EXPECT_EQ(egg->isSpawned(), !hatch) << "a hatching egg is deleted, the other stays";
		hatched += hatch ? 1 : 0;
	}
	EXPECT_GT(hatched, 0);
	EXPECT_LT(hatched, 20);
}

/** OmegaAI.java:26-29: the summons come only while a player is within 30 m */
TEST_F(InggisonA1AiTest, OmegaSummonsOnlyWithAPlayerNearby) {
	runtime::Ptr<Npc> omega = place(900200, 200.0f, 2140.0f, 441.0f);
	auto& ai = install<OmegaProbe>(*omega);
	ZoneKnownListPairing::pair(*omega, player());
	EXPECT_FALSE(ai.checkBeforeSpawn()) << "60 m away";
	runtime::Ptr<Npc> near = place(900300, 160.0f, 2140.0f, 441.0f);
	auto& nearAi = install<OmegaProbe>(*near);
	ZoneKnownListPairing::pair(*near, player());
	EXPECT_TRUE(nearAi.checkBeforeSpawn()) << "20 m away";
}

/** SematariuxThunderShieldAI.java:22-31: a tenth of the max HP base, half the damage */
TEST_F(InggisonA1AiTest, TheThunderShieldsAreWeaker) {
	runtime::Ptr<Npc> shield = place(281931, 150.0f, 2140.0f, 441.0f);
	auto& ai = install<ing::SematariuxThunderShieldAI>(*shield);
	model::stats::calc::AdditionStat maxHp(model::stats::container::StatEnum::MAXHP, 1000.0f, *shield);
	ai.modifyOwnerStat(maxHp);
	EXPECT_FLOAT_EQ(maxHp.getBaseRate(), 0.1f);
	model::stats::calc::AdditionStat attack(model::stats::container::StatEnum::PHYSICAL_ATTACK, 1000.0f, *shield);
	ai.modifyOwnerStat(attack);
	EXPECT_FLOAT_EQ(attack.getBaseRate(), 1.0f);
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(300.0f, player(), nullptr), 150.0f);
}

/** SematariuxEggAI.java:24-59: 4 minutes after its spawn the egg hatches 281456; its death first stops that; no decay, loot or rewards */
TEST_F(InggisonA1AiTest, SematariuxsEggHatchesUnlessItDies) {
	publishSkills({18726, 19199});
	runtime::Ptr<Npc> egg = place(281451, 141.77f, 2142.31f, 440.0f);
	auto& ai = install<Probe<ing::SematariuxEggAI>>(*egg);
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_DECAY));
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_LOOT));
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	ai.handleSpawned();
	executor().advance(std::chrono::milliseconds(4 * 60 * 1000 - 1));
	EXPECT_EQ(count(281456), 0u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count(281456), 1u);
	runtime::Ptr<Npc> second = place(281451, 141.77f, 2142.31f, 440.0f);
	auto& secondAi = install<Probe<ing::SematariuxEggAI>>(*second);
	secondAi.handleSpawned();
	place(216520, 150.0f, 2140.0f, 441.0f); // Sematariux, whose 18726 its death takes off and whose 19199 it queues
	secondAi.handleDied();
	executor().advance(std::chrono::minutes(5));
	EXPECT_EQ(count(281456), 1u) << "the dead egg hatches nothing";
}

/** SematariuxAI.java:43-69, 165-185: six thunder shields that split twice (2 x 281932, then 2 x 281933) and the three 282123 */
TEST_F(InggisonA1AiTest, SematariuxsThunderShieldsSplitTwice) {
	publishSkills({19186});
	runtime::Ptr<Npc> sematariux = place(216520, 141.77f, 2142.31f, 440.0f);
	auto& ai = install<Probe<ing::SematariuxAI>>(*sematariux);
	ai.handleSpawned();
	ASSERT_EQ(count(281931), 6u);
	EXPECT_EQ(count(282123), 3u);
	for (const runtime::Ptr<Npc>& shield : npcs(*instance, 281931))
		kill(*shield);
	ASSERT_EQ(count(281932), 12u);
	for (const runtime::Ptr<Npc>& split : npcs(*instance, 281932)) { // within (5, 3) m of a dead shield, 0.4 m above it
		bool nearAShield = false;
		for (const runtime::Ptr<Npc>& shield : npcs(*instance, 281931))
			nearAShield |= std::abs(split->getX() - shield->getX()) <= 5.0f && std::abs(split->getY() - shield->getY()) <= 3.0f &&
				std::abs(split->getZ() - (shield->getZ() + 0.4f)) < 0.001f;
		EXPECT_TRUE(nearAShield) << split->getX() << " " << split->getY() << " " << split->getZ();
	}
	for (const runtime::Ptr<Npc>& shield : npcs(*instance, 281932))
		kill(*shield);
	ASSERT_EQ(count(281933), 24u);
	for (const runtime::Ptr<Npc>& shield : npcs(*instance, 281933))
		kill(*shield); // the 24th takes the Protective Slumber off (none here: a no-op)
	EXPECT_EQ(count(281933), 24u) << "the last shields do not split";
	ai.handleBackHome();
	EXPECT_EQ(count(281931) + count(281932) + count(281933), 0u) << "back home he takes his spawns away";
	EXPECT_EQ(count(282123), 3u) << "the 282123 go only with the slumber";
}

/** SematariuxAI.java:50-56, 77-83, 159-163: his stats, a tornado on the hated player at 90 %, the berserk state at 5 % */
TEST_F(InggisonA1AiTest, SematariuxsPhasesCallTornadoes) {
	runtime::Ptr<Npc> sematariux = place(216520, 141.77f, 2142.31f, 440.0f);
	auto& ai = install<Probe<ing::SematariuxAI>>(*sematariux);
	model::stats::calc::AdditionStat maxHp(model::stats::container::StatEnum::MAXHP, 1000.0f, *sematariux);
	ai.modifyOwnerStat(maxHp);
	EXPECT_FLOAT_EQ(maxHp.getExactBaseWithoutBaseRate(), 20'880'000.0f);
	model::stats::calc::AdditionStat attack(model::stats::container::StatEnum::PHYSICAL_ATTACK, 1000.0f, *sematariux);
	ai.modifyOwnerStat(attack);
	EXPECT_FLOAT_EQ(attack.getExactBaseWithoutBaseRate(), 2200.0f);
	sematariux->getLifeStats()->setCurrentHpPercent(100); // of the 20,880,000 the stat change gives him
	aggro(*sematariux);
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	EXPECT_EQ(count(281453), 0u) << "at full HP no phase";
	sematariux->getLifeStats()->setCurrentHpPercent(89);
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	ASSERT_EQ(count(281453), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 281453)[0]->getX(), player().getX()) << "on the player";
	sematariux->getLifeStats()->setCurrentHpPercent(4);
	for (int32_t phase = 0; phase < 6; phase++) // one phase per attack (HpPhases.tryEnterNextPhase)
		ai.handleAttack(runtime::Ptr<Creature>(player()));
	EXPECT_EQ(count(281453), 6u) << "70, 50, 30, 20 and 10 % on the way down; 5 % queues the berserk state";
	ai.handleDied();
	EXPECT_EQ(count(281453), 0u);
}

/** SematariuxAI.java:85-119: the forced walk to the nest lays one egg; Extract Essence's end sends him back; Deadly Bolt marks a player */
TEST_F(InggisonA1AiTest, SematariuxsNestAndBolt) {
	publishSkills({18726, 19178});
	runtime::Ptr<Npc> sematariux = place(216520, 141.77f, 2142.31f, 440.0f);
	auto& ai = install<Probe<ing::SematariuxAI>>(*sematariux);
	ai.isDestinationReached();
	EXPECT_EQ(count(281451), 0u) << "not forced walking";
	ai.setStateIfNot(AIState::FORCED_WALKING);
	ai.isDestinationReached();
	ai.isDestinationReached();
	EXPECT_EQ(count(281451), 1u) << "one egg per event";
	aggro(*sematariux);
	ai.onEndUseSkill(dataholders::DataManager::SKILL_DATA->getSkillTemplate(18726), 1);
	EXPECT_EQ(ai.getState(), AIState::FIGHT);
	EXPECT_EQ(sematariux->getTarget(), runtime::Ptr<model::gameobjects::VisibleObject>(player()));
	ai.onEndUseSkill(dataholders::DataManager::SKILL_DATA->getSkillTemplate(19178), 1);
	ASSERT_EQ(count(281452), 1u) << "a fifth of one player, at least one";
	EXPECT_FLOAT_EQ(npcs(*instance, 281452)[0]->getX(), player().getX());
	ai.handleDespawned();
	EXPECT_EQ(count(281451), 0u);
}

} // namespace
} // namespace aion::gameserver::handlers::ai::worlds::test
