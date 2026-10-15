// M5d's quest-npc-ais lane (m5d-plan.md A-04): the AI smoke of handlers-and-porting-plan.md (the P5-05 row of its acceptance table: for each
// registered AI name, spawn an npc with it, fire SPAWNED -> CREATURE_SEE -> ATTACKED -> DIED -> DESPAWNED, run all timers for 10 minutes of
// virtual time, 0 exceptions and 0 unported hits) for the three AI names M5d pulls forward (m5d-plan.md D5): "simple_abyssguard"
// (AbyssGuardSimpleAI, P5-05), "useitem" (ActionItemNpcAI, P5-05) and "quest_use_item" (QuestItemNpcAI, chunk A1). Each runs on an npc of a
// shipped row with that AI (QuestNpcAiTestSupport.h: Jucleas 203752, the defensive artillery 218610, the kerub grain sack 700105), next to a
// player in the fixture's active map region, who also clicks it (DIALOG_START, between the see and the attack). On the artillery the click
// starts the use bar, and the death aborts it (ActionItemNpcAI.java:87-97); both are checked before the timers run, which would end the bar by
// its own task. On the grain sack the click starts no bar: no quest is registered here, so QuestEngine.onCanAct refuses it
// (QuestItemNpcAI.java:33-40, QuestEngine.java:587-600) - QuestItemNpcAiTest.cpp runs the quest object's bar with 1103 registered. ASan, which
// the row also names, is not run by day (the workflow's resource rules).
//
// Each AI comes from the factory its AION_AI marker defines (this executable links the empty registry, see RootAiHandlersTest.cpp); the
// QuestItemNpcAI factory is defined by the leased source that QuestItemNpcAiTest.cpp compiles into this executable. A task's exception ends
// in ExecuteWrapper's log ("Exception in a Runnable execution", runtime/sched/Future.cpp), which the cases capture.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/handlers/ai/AbyssGuardSimpleAI.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"
#include "aion/gameserver/handlers/ai/quests/QuestItemNpcAI.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"

#include "QuestNpcAiTestSupport.h"

// The factory functions the AION_AI markers define, declared exactly as Registry.ai.gen.cpp declares them (handlers::AIFactory)
namespace aion::gameserver::handlers::ai {
::aion::gameserver::handlers::AIFactory AbyssGuardSimpleAI_aiFactory;
::aion::gameserver::handlers::AIFactory ActionItemNpcAI_aiFactory;
namespace quests {
::aion::gameserver::handlers::AIFactory QuestItemNpcAI_aiFactory;
} // namespace quests
} // namespace aion::gameserver::handlers::ai

namespace aion::gameserver::ai::testing {
namespace {

namespace roots = gameserver::handlers::ai;

using event::AIEventType;
using model::gameobjects::Npc;
using model::gameobjects::player::Player;

class QuestNpcAiSmokeTest : public QuestNpcAiWorldTest {
protected:
	/**
	 * The smoke of one AI name, which must end DESPAWNED; the case fails on an exception of an event, a new unported or partial site, an error
	 * of a task, or a bar left in the player's ObserveController. For the two item AIs, `clickRunsABar` says whether the click starts the use
	 * bar, and no bar may outlive the death.
	 */
	template <class AI>
	void smoke(gameserver::handlers::AIFactory& factory, int32_t npcId, int32_t playerId, bool clickRunsABar = false) {
		AI_TEST_SCOPE;
		const uint64_t unportedBefore = runtime::unportedHitCount();
		const uint64_t partialBefore = runtime::partialHitCount();
		LogCapture wrapperLog("com.aionemu.commons.utils.concurrent.ExecuteWrapper");
		runtime::Ref<Npc> npc = makeWorldNpc(npcId, 505, 500, 100);
		runtime::Ref<Player> player = makeWorldPlayer(playerId, 503, 500, 100); // 2 m away: in talk range of all three rows
		player->setQuestStateList(model::gameobjects::player::QuestStateList::create()); // a loaded player always has one
		know(*npc, *player);
		std::unique_ptr<AbstractAI> created = factory(*npc);
		ASSERT_NE(dynamic_cast<AI*>(created.get()), nullptr) << "the marker's factory builds its class";
		AbstractAI& ai = *created;
		npc->replaceAi(std::move(created));

		constexpr bool itemAi = std::is_base_of_v<roots::ActionItemNpcAI, AI>;
		EXPECT_NO_THROW({
			ai.onGeneralEvent(AIEventType::BEFORE_SPAWNED);
			ai.onGeneralEvent(AIEventType::SPAWNED);
			ai.onCreatureEvent(AIEventType::CREATURE_SEE, *player);
			ai.onCreatureEvent(AIEventType::DIALOG_START, *player);
		});
		if constexpr (itemAi)
			EXPECT_EQ(player->getObserveController()->hasObservers(), clickRunsABar) << "the click's use bar";
		EXPECT_NO_THROW({
			ai.onCreatureEvent(AIEventType::ATTACK, *player); // the row's ATTACKED: the event an attack fires (AbstractAI.java:335)
			ai.onGeneralEvent(AIEventType::DIED);
		});
		if constexpr (itemAi)
			EXPECT_FALSE(player->getObserveController()->hasObservers()) << "the death aborted the bar before any timer ran";
		EXPECT_NO_THROW({
			ai.onGeneralEvent(AIEventType::DESPAWNED);
			executor->advance(std::chrono::minutes(10));
		});

		EXPECT_TRUE(ai.isInState(AIState::DESPAWNED));
		EXPECT_EQ(runtime::unportedHitCount(), unportedBefore) << "an unported site was reached";
		EXPECT_EQ(runtime::partialHitCount(), partialBefore) << "a partial site was reached";
		EXPECT_EQ(wrapperLog.text(), "") << "a task threw";
		EXPECT_FALSE(player->getObserveController()->hasObservers()) << "no bar outlives the death";
	}
};

TEST_F(QuestNpcAiSmokeTest, SimpleAbyssguard) {
	smoke<roots::AbyssGuardSimpleAI>(roots::AbyssGuardSimpleAI_aiFactory, JUCLEAS, 880001);
}

TEST_F(QuestNpcAiSmokeTest, Useitem) {
	smoke<roots::ActionItemNpcAI>(roots::ActionItemNpcAI_aiFactory, DEFENSIVE_ARTILLERY, 880002, true);
}

TEST_F(QuestNpcAiSmokeTest, QuestUseItem) {
	smoke<roots::quests::QuestItemNpcAI>(roots::quests::QuestItemNpcAI_aiFactory, KERUB_GRAIN_SACK_OBJECT, 880003, false); // no quest: no bar
}

} // namespace
} // namespace aion::gameserver::ai::testing
