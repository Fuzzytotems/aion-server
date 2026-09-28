// A-03 (m5b-plan.md §4, P5-05): NpcAI's 27 bodies - the 13 narrowing accessors, isInRange, isMoveSupported, ask() per AIQuestion (D8) and
// isDestinationReached() per AIState.
//
// ask() is the switch that turns an npc death into a reward: with the all-false answer of AITemplate, NpcController::onDie schedules no
// respawn, runs no reward and takes the instant delete_() arm (m5b-plan.md D8). The REWARD_AP arm is asserted false on an ELYSEA map, which is
// what makes the gate's R2 self-enforcing (D16): AbyssPointsService::addAp stays AION_UNPORTED, so a wrong `true` would throw.
//
// The accessors are reached through a probe subclass, because Java declares them protected (package access inside com.aionemu.gameserver.ai)
// and C++ protected is per class. The probe is a real NpcAI over a real Npc, so the bodies under test are the shipped ones.

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/TribeClass.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/skill/NpcSkillList.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

#include "AiWorldTestSupport.h"

namespace aion::gameserver::ai::testing {
namespace {

using poll::AIQuestion;

/** A leaf NpcAI that adds nothing but access to the protected accessors (Java: package access inside com.aionemu.gameserver.ai). */
class NpcAIProbe final : public NpcAI {
public:
	explicit NpcAIProbe(model::gameobjects::Npc& owner) : NpcAI(owner) {}

	using NpcAI::getAggroList;
	using NpcAI::getCreator;
	using NpcAI::getCreatorId;
	using NpcAI::getEffectController;
	using NpcAI::getKnownList;
	using NpcAI::getLifeStats;
	using NpcAI::getMoveController;
	using NpcAI::getNpcId;
	using NpcAI::getObjectTemplate;
	using NpcAI::getRace;
	using NpcAI::getSkillList;
	using NpcAI::getSpawnTemplate;
	using NpcAI::getTribe;
	using NpcAI::isInRange;
};

class NpcAITest : public AiWorldTest {
protected:
	/** replaces the npc's AI with a real NpcAI leaf (the test executable links the empty registry, so newAI cannot build one) */
	NpcAIProbe& installProbe(model::gameobjects::Npc& npc) {
		auto probe = std::make_unique<NpcAIProbe>(npc);
		NpcAIProbe& result = *probe;
		npc.replaceAi(std::move(probe));
		return result;
	}
};

TEST_F(NpcAITest, TheNarrowingAccessorsReturnTheOwnersOwn) {
	AI_TEST_SCOPE;
	runtime::Ref<model::gameobjects::Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
	NpcAIProbe& ai = installProbe(*npc);

	EXPECT_EQ(ai.getObjectTemplate(), npc->getObjectTemplate());
	EXPECT_EQ(ai.getSpawnTemplate().get(), npc->getSpawn().get());
	EXPECT_EQ(ai.getLifeStats().get(), npc->getLifeStats().get());
	EXPECT_EQ(ai.getRace(), npc->getRace());
	EXPECT_EQ(ai.getTribe(), npc->getTribe());
	EXPECT_EQ(ai.getTribe(), model::TribeClass::MONSTER);
	EXPECT_EQ(ai.getEffectController().get(), npc->getEffectController().get());
	EXPECT_EQ(&ai.getKnownList(), &npc->getKnownList());
	EXPECT_EQ(&ai.getAggroList(), &npc->getAggroList());
	EXPECT_EQ(ai.getSkillList().get(), npc->getSkillList().get());
	EXPECT_EQ(ai.getCreator().get(), npc->getCreator().get());
	EXPECT_EQ(ai.getMoveController().get(), npc->getMoveController().get());
	EXPECT_EQ(ai.getNpcId(), SPARKIE_NPC_ID);
	EXPECT_EQ(ai.getCreatorId(), npc->getCreatorId());
	EXPECT_EQ(&ai.getOwner(), npc.get()) << "AITemplate<Npc> narrows getOwner";
}

TEST_F(NpcAITest, IsInRangeMeasuresFromTheOwner) {
	AI_TEST_SCOPE;
	runtime::Ref<model::gameobjects::Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
	runtime::Ref<model::gameobjects::Npc> other = makeWorldNpc(SPARKIE_NPC_ID, 505, 500, 100);
	NpcAIProbe& ai = installProbe(*npc);

	// PositionUtil::isInRange(owner, object, range) is the bound-radius form: 5 m apart, bound radius 0.5 on each side
	EXPECT_TRUE(ai.isInRange(*other, 6));
	EXPECT_FALSE(ai.isInRange(*other, 3));
}

TEST_F(NpcAITest, AskAnswersEveryQuestionAsJavaDoes) {
	AI_TEST_SCOPE;
	runtime::Ref<model::gameobjects::Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
	NpcAIProbe& ai = installProbe(*npc);

	// CAN_SHOUT short-circuits on AIConfig.SHOUTS_ENABLE, which the M5b profile leaves off (m5b-plan.md D1); NpcShoutsService::mayShout is
	// AION_UNPORTED, so a port that drops the config term throws here instead of answering.
	EXPECT_FALSE(configs::main::AIConfig::SHOUTS_ENABLE.load());
	EXPECT_FALSE(ai.ask(AIQuestion::CAN_SHOUT));

	EXPECT_TRUE(ai.ask(AIQuestion::ALLOW_RESPAWN)) << "SiegeService::isRespawnAllowed for a plain npc (SiegeService.java:511-522)";
	EXPECT_TRUE(ai.ask(AIQuestion::ALLOW_DECAY));
	EXPECT_TRUE(ai.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	EXPECT_TRUE(ai.ask(AIQuestion::REWARD_LOOT));

	// NORMAL rating and static id 0: neither a boss nor a static npc
	EXPECT_FALSE(ai.ask(AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES));

	// Poeta is world_type ELYSEA, so the `wt != ELYSEA && ...` term is false and the BEAST/MONSTER race term is never reached (NpcAI.java:153-156)
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_AP)) << "m5b-plan.md D16: a wrong true would reach the unported AbyssPointsService::addAp";

	EXPECT_TRUE(ai.ask(AIQuestion::REMOVE_EFFECTS_ON_MAP_REGION_DEACTIVATE)) << "Poeta is no instance map";

	// the default arm
	EXPECT_FALSE(ai.ask(AIQuestion::CONSIDER_BOUNDS_IN_CAN_SEE_CHECK_WHEN_ATTACKED));
	EXPECT_FALSE(ai.ask(AIQuestion::CONSIDER_BOUNDS_IN_CAN_SEE_CHECK_WHEN_ATTACKING));

	// every question answered: the loop is the guard against a case that silently falls through to `default`
	int32_t trueCount = 0;
	for (size_t i = 0; i < xml::EnumTraits<AIQuestion>::names.size(); ++i)
		trueCount += ai.ask(static_cast<AIQuestion>(i)) ? 1 : 0;
	EXPECT_EQ(trueCount, 5) << "ALLOW_DECAY, ALLOW_RESPAWN, REMOVE_EFFECTS_ON_MAP_REGION_DEACTIVATE, REWARD_AP_XP_DP_LOOT and REWARD_LOOT";
}

TEST_F(NpcAITest, IsDestinationReachedFollowsTheState) {
	AI_TEST_SCOPE;
	runtime::Ref<model::gameobjects::Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
	NpcAIProbe& ai = installProbe(*npc);

	// CREATED and IDLE take the default arm
	EXPECT_TRUE(ai.isDestinationReached());
	ai.setStateIfNot(AIState::IDLE);
	EXPECT_TRUE(ai.isDestinationReached());

	// RETURNING: the npc stands on its spawn point
	ai.setStateIfNot(AIState::RETURNING);
	EXPECT_TRUE(ai.isDestinationReached());
	npc->getPosition()->setXYZH(520.0f, std::nullopt, std::nullopt, std::nullopt);
	EXPECT_FALSE(ai.isDestinationReached()) << "20 m from the spawn point";
	npc->getPosition()->setXYZH(500.0f, std::nullopt, std::nullopt, std::nullopt);

	// FIGHT: SimpleAttackManager::isTargetInAttackRange, which is false without a Creature target
	ai.setStateIfNot(AIState::FIGHT);
	EXPECT_FALSE(ai.isDestinationReached());

	// FOLLOWING: FollowEventHandler::isInRange over the owner's target, false for a null target
	ai.setStateIfNot(AIState::FOLLOWING);
	EXPECT_FALSE(ai.isDestinationReached());
	runtime::Ref<model::gameobjects::Npc> near = makeWorldNpc(SPARKIE_NPC_ID, 501, 500, 100);
	npc->setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*near));
	EXPECT_TRUE(ai.isDestinationReached()) << "1 m apart, the follow range is 2";
	npc->setTarget(nullptr);

	// WALKING: the TALK sub state answers true without asking the move controller
	ai.setStateIfNot(AIState::WALKING);
	ai.setSubStateIfNot(AISubState::TALK);
	EXPECT_TRUE(ai.isDestinationReached());
}

TEST_F(NpcAITest, IsMoveSupportedReadsTheMovementSpeedAndTheFreezeSubState) {
	AI_TEST_SCOPE;
	runtime::Ref<model::gameobjects::Npc> npc = makeWorldNpc(SPARKIE_NPC_ID, 500, 500, 100);
	NpcAIProbe& ai = installProbe(*npc);

	EXPECT_GT(npc->getGameStats()->getMovementSpeed()->getCurrent(), 0);
	EXPECT_TRUE(ai.isMoveSupported());

	ai.setSubStateIfNot(AISubState::FREEZE);
	EXPECT_FALSE(ai.isMoveSupported()) << "a frozen npc does not move";
	ai.setSubStateIfNot(AISubState::NONE);
	EXPECT_TRUE(ai.isMoveSupported());
}

} // namespace
} // namespace aion::gameserver::ai::testing
