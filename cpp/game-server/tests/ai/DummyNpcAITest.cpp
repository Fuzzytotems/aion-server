// A-00 / m5b-plan.md D15 (P5-05): AIEngine's warn-mode substitute for a missing AI handler is an NpcAI when the owner is an Npc.
//
// Why this test is the only guard. Five Java-faithful controller bodies write `(NpcAI) creature.getAi()` - NpcController.cpp:337,
// PlayerController.cpp:543 and NpcMoveController.cpp:287, 412, 416 - ported as runtime::cast<ai::NpcAI>. In Java the cast cannot fail, because
// AIEngine.newAI throws "No AI found for name X" (AIEngine.java:70-71) for an unregistered name, so an Npc whose template names an AI always has
// an NpcAI. The C++-only setting gameserver.dev.missing_ai_handlers=warn (docs/deviations/P4-01.md) breaks that invariant by substituting an AI,
// and before A-00 the substitute was a DummyAI<Creature>, which is an AITemplate<Creature> and not an NpcAI - so the cast threw
// ClassCastException the moment such an NPC was attacked or moved. The scenario gate cannot see this: its monster (210663, ai="aggressive") has a
// registered handler, and so does every NPC the scripted path touches (m5b-plan.md §8 risk 19).
//
// The assertions below are the cast itself plus the whole externally visible surface of the substitute, because D15 requires that only the static
// type changes: ask() answers false for every AIQuestion (AITemplate's answer, not NpcAI's, which says true for ALLOW_DECAY, REWARD_LOOT and
// REWARD_AP_XP_DP_LOOT), isDestinationReached() is false, and every hook NpcAI overrides stays a no-op.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/ai/AIEngine.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

#include "AiTestSupport.h"

namespace aion::gameserver::ai::testing {
namespace {

using event::AIEventType;
using poll::AIQuestion;

/** An NPC template whose ai name no registry in this executable knows (the test executable links the empty AI table). */
const model::templates::npc::NpcTemplate* unregisteredAiTemplate() {
	static const model::templates::npc::NpcTemplate* value = npcTemplate(
		R"(npc_id="210663" level="2" name="juvenile sparkie" rating="NORMAL" rank="DISCIPLINED" tribe="MONSTER" ai="a5b_unregistered_ai")");
	return value;
}

class DummyNpcAITest : public AiTest {
protected:
	void SetUp() override {
		AiTest::SetUp();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
	}
};

TEST_F(DummyNpcAITest, AnNpcWithAnUnregisteredAiNameStillCastsToNpcAI) {
	AI_TEST_SCOPE;
	runtime::Ref<AiTestNpc> npc = createNpc(unregisteredAiTemplate());

	// the expression of NpcController.cpp:337 / PlayerController.cpp:543 / NpcMoveController.cpp:287, 412, 416
	runtime::Ptr<NpcAI> npcAi;
	ASSERT_NO_THROW(npcAi = runtime::cast<ai::NpcAI>(npc->getAi())) << "Java: (NpcAI) npc.getAi() cannot fail (AIEngine.java:70-71)";
	ASSERT_TRUE(npcAi);
	EXPECT_EQ(npcAi.get(), &npc->getAi()) << "the substitute is the AI the Npc holds, not a copy";
	EXPECT_EQ(npcAi->getName(), "noname") << "no registry entry: AbstractAI::getName falls through (docs/deviations/P4-01.md)";
}

TEST_F(DummyNpcAITest, TheSubstituteAnswersEveryQuestionFalseLikeADummyAi) {
	AI_TEST_SCOPE;
	runtime::Ref<AiTestNpc> npc = createNpc(unregisteredAiTemplate());
	AbstractAI& ai = npc->getAi();

	const auto& questions = xml::EnumTraits<AIQuestion>::names;
	ASSERT_EQ(questions.size(), 10u);
	for (size_t i = 0; i < questions.size(); ++i) {
		auto question = static_cast<AIQuestion>(i);
		EXPECT_FALSE(ai.ask(question)) << questions[i] << ": NpcAI::ask would answer ALLOW_DECAY, REWARD_LOOT and REWARD_AP_XP_DP_LOOT true";
	}
	EXPECT_FALSE(ai.isDestinationReached()) << "AITemplate::isDestinationReached; NpcAI's switches on the state";
	EXPECT_TRUE(ai.canThink()) << "AITemplate";
	EXPECT_EQ(ai.getState(), AIState::CREATED);
	EXPECT_EQ(ai.getSubState(), AISubState::NONE);
}

TEST_F(DummyNpcAITest, TheSubstitutesHooksAreNoOps) {
	AI_TEST_SCOPE;
	runtime::Ref<AiTestNpc> npc = createNpc(unregisteredAiTemplate());
	runtime::Ref<AiTestNpc> other = createNpc(unregisteredAiTemplate());
	AbstractAI& ai = npc->getAi();

	// every general event NpcAI overrides a hook for; NpcAI would reach SpawnEventHandler, DiedEventHandler, ShoutEventHandler and
	// ActivateEventHandler, all of which need a ported world. The substitute must do nothing at all, exactly as a DummyAI does.
	EXPECT_NO_THROW(ai.onGeneralEvent(AIEventType::BEFORE_SPAWNED));
	EXPECT_NO_THROW(ai.onGeneralEvent(AIEventType::SPAWNED));
	EXPECT_TRUE(ai.setStateIfNot(AIState::IDLE));
	for (AIEventType event : {AIEventType::ACTIVATE, AIEventType::DEACTIVATE, AIEventType::MOVE_ARRIVED, AIEventType::MOVE_VALIDATE,
			 AIEventType::DIED, AIEventType::DESPAWNED})
		EXPECT_NO_THROW(ai.onGeneralEvent(event)) << xml::EnumTraits<AIEventType>::names[static_cast<size_t>(event)];
	EXPECT_EQ(ai.getState(), AIState::IDLE) << "no hook changed the state";

	EXPECT_NO_THROW(ai.onCreatureEvent(AIEventType::CREATURE_MOVED, *other));
	EXPECT_NO_THROW(ai.onCreatureEvent(AIEventType::TARGET_CHANGED, *other));
}

} // namespace
} // namespace aion::gameserver::ai::testing
