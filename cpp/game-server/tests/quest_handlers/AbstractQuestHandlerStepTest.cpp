// P5-06b, M5d H-02 and H-04 (m5d-plan.md §7): the step helpers of AbstractQuestHandler - updateQuestStatus, changeQuestStep (with the rollback
// message H-07 names), onCanAct and getActionItems - and the event defaults: the kill, ranked-kill, kill-in-zone, skill, get-item and follow-end
// steps, the level-change and quest-completed starts of the campaign quests (with hasAnyPreQuestFinished), the zone start, playQuestMovie and
// sendEmotion (AbstractQuestHandler.java:81-85, 224-255, 290-327, 611-615, 661-864, 979-1149).
//
// The quests are real (QuestHandlerTestSupport.h): 1102's kill of the striped kerub, 1103's grain sacks (a 7xxxxx quest object: an action item),
// the campaign 1001 that waits for 1100 (_1001TheKerubThreat.java:58-63 pass 1100 as the pre-quest), and the Asmodian campaign 2014 -> 2015 ->
// 2017, whose <finished> chain hasAnyPreQuestFinished follows; 2911, a QUEST with six alternative conditions, and 3905, whose quest objects
// drop three items each. The quest variables are six bits each (QuestVars.java): var1 = 1 is 64.
//
// Not here: defaultStartFollowEvent x2, which need questEngine/task (E-07, stage 3; docs/deviations/P5-06b.md).

#include "QuestHandlerTestSupport.h"

#include <any>
#include <array>
#include <cstdint>
#include <span>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/EmotionId.h"
#include "aion/gameserver/model/EmotionIdInfo.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/model/QuestActionType.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/world/zone/ZoneName.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test {
namespace {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::gameobjects::Npc;
using gameserver::model::gameobjects::VisibleObject;
using model::QuestActionType;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

Ptr<VisibleObject> at(VisibleObject& object) {
	return Ptr<VisibleObject>(object);
}

class AbstractQuestHandlerStepTest : public QuestHandlerTest {
protected:
	std::vector<uint8_t> giveUp(int32_t questId) {
		return me->serializedFor(SM_SYSTEM_MESSAGE::STR_QUEST_SYSTEMMSG_GIVEUP(dataholders::DataManager::QUEST_DATA->getQuestById(questId)->getL10n()));
	}

	static int32_t vars(const Ref<QuestState>& qs) { return qs->getQuestVars()->getQuestVars(); }
};

// updateQuestStatus (AbstractQuestHandler.java:290-296): SM_QUEST_ACTION(UPDATE) of the handler's quest, and the nearby quests only for REWARD
// and COMPLETE; a player without the quest state is Java's NullPointerException in SM_QUEST_ACTION's constructor
TEST_F(AbstractQuestHandlerStepTest, UpdateQuestStatusSendsTheStateAndTheNearbyQuestsForRewardAndComplete) {
	PlainHandler handler(1111);
	Ref<QuestEnv> env = envOf(*me, 0, DialogAction::USE_OBJECT);
	EXPECT_THROW(handler.updateQuestStatus(*env), runtime::NullPointerException);
	Ref<QuestState> qs = hold(*me, 1111, QuestStatus::START, 2);
	for (auto [status, value] : std::array<std::pair<QuestStatus, int32_t>, 4>{
			 {{QuestStatus::START, START}, {QuestStatus::LOCKED, LOCKED}, {QuestStatus::REWARD, REWARD}, {QuestStatus::COMPLETE, COMPLETE}}}) {
		qs->setStatus(status);
		me->clearSent();
		handler.updateQuestStatus(*env);
		if (status == QuestStatus::REWARD || status == QuestStatus::COMPLETE)
			EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1111, value, 2), noNearbyQuests()})) << value;
		else
			EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1111, value, 2)})) << value;
	}
}

// changeQuestStep (AbstractQuestHandler.java:298-327): a matching step moves to the next (UPDATE sent); another step answers false; the same
// step answers true and sends nothing unless it rewards (REWARD, UPDATE and the nearby quests); a step back in START tells the player the quest
// was given up (STR_QUEST_SYSTEMMSG_GIVEUP with the name of the handler's quest, :313, whatever the env's) before the UPDATE - not in REWARD;
// varNum picks a variable, -1 the whole vars, which the three-argument overload chooses for a step above 0x3F; no quest state answers false
TEST_F(AbstractQuestHandlerStepTest, ChangeQuestStepMovesAMatchingStepAndWarnsOfARollback) {
	PlainHandler handler(1111);
	Ref<QuestEnv> env = envOf(*me, 1111, DialogAction::SETPRO1);
	EXPECT_FALSE(handler.changeQuestStep(*env, 0, 1, false)) << "no quest state";
	Ref<QuestState> qs = hold(*me, 1111, QuestStatus::START, 2);

	EXPECT_TRUE(handler.changeQuestStep(*env, 2, 3, false));
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1111, START, 3)}));
	me->clearSent();
	EXPECT_FALSE(handler.changeQuestStep(*env, 2, 4, false)) << "the step is 3";
	EXPECT_TRUE(handler.changeQuestStep(*env, 3, 3, false));
	EXPECT_TRUE(me->sent().empty()) << "no change, no reward: nothing sent";
	EXPECT_EQ(vars(qs), 3);

	EXPECT_TRUE(handler.changeQuestStep(*envOf(*me, 1101, DialogAction::SETPRO1), 3, 1, false)) << "an env of another quest: the handler's decides";
	EXPECT_EQ(me->sent(), cp::exactly({giveUp(1111), questUpdate(1111, START, 1)})) << "a rollback in START: the message names the handler's quest";

	me->clearSent();
	EXPECT_TRUE(handler.changeQuestStep(*env, 1, 1, true));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1111, REWARD, 1), noNearbyQuests()}));
	me->clearSent();
	EXPECT_TRUE(handler.changeQuestStep(*env, 1, 0, false));
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1111, REWARD, 0), noNearbyQuests()})) << "a rollback in REWARD: no message";

	qs->setStatus(QuestStatus::START);
	qs->setQuestVar(1); // var0 1, var1 0
	me->clearSent();
	EXPECT_TRUE(handler.changeQuestStep(*env, 0, 2, false, 1));
	EXPECT_EQ(vars(qs), 1 + (2 << 6));
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1111, START, 1 + (2 << 6))}));

	qs->setQuestVar(0x40); // var0 0, var1 1
	me->clearSent();
	EXPECT_TRUE(handler.changeQuestStep(*env, 0x40, 0x41)) << "a step above 0x3F: the whole vars";
	EXPECT_EQ(vars(qs), 0x41);
	EXPECT_EQ(qs->getQuestVarById(0), 1) << "setQuestVar splits the value: var0 1";
	EXPECT_EQ(qs->getQuestVarById(1), 1) << "var1 1";
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1111, START, 0x41)}));
	qs->setQuestVar(0x3F);
	EXPECT_TRUE(handler.changeQuestStep(*env, 0x3F, 0x40)) << "the new step decides too";
	EXPECT_EQ(vars(qs), 0x40);
	EXPECT_EQ(qs->getQuestVarById(0), 0) << "the whole vars: var0 0";
	EXPECT_EQ(qs->getQuestVarById(1), 1) << "var1 1 (var0 would hold 64 if the step went to var0 alone)";
	qs->setQuestVar(0x3F + 0x40); // var0 63, var1 1
	me->clearSent();
	EXPECT_TRUE(handler.changeQuestStep(*env, 0x3F, 0x3E)) << "both steps up to 0x3F: var0";
	EXPECT_EQ(vars(qs), 0x3E + 0x40);
	EXPECT_EQ(me->sent(), cp::exactly({giveUp(1111), questUpdate(1111, START, 0x3E + 0x40)}));
}

// onCanAct (AbstractQuestHandler.java:224-255): only a quest in START (the env's quest) may act; for an action item (ACTION_ITEM_USE with
// action items) the player must not already hold the collect count of the item the target npc drops - 1103's grain sack drops 182200201, of
// which 3 are collected. Another target, another action type or a quest without action items may act. The action items are the handler's,
// the state and the drops the env's quest's (:226-230).
TEST_F(AbstractQuestHandlerStepTest, OnCanActStopsAnActionItemOnceTheCollectCountIsHeld) {
	PlainHandler handler(1103);
	Npc& sack = npcOf(GRAIN_SACK);
	Npc& mires = npcOf(MIRES);
	auto canAct = [&](int32_t questId, QuestActionType type, Ptr<VisibleObject> target) {
		return handler.onCanAct(*envOf(*me, questId, DialogAction::USE_OBJECT, target), type, std::span<const std::any>());
	};
	EXPECT_FALSE(canAct(1103, QuestActionType::ACTION_ITEM_USE, at(sack))) << "no quest state";
	Ref<QuestState> qs = hold(*me, 1103, QuestStatus::REWARD);
	EXPECT_FALSE(canAct(1103, QuestActionType::ACTION_ITEM_USE, at(sack))) << "REWARD";
	qs->setStatus(QuestStatus::COMPLETE);
	EXPECT_FALSE(canAct(1103, QuestActionType::ITEM_USE, at(sack))) << "COMPLETE";
	qs->setStatus(QuestStatus::START);
	EXPECT_TRUE(canAct(1103, QuestActionType::ACTION_ITEM_USE, at(sack))) << "none held";
	holdItem(*me, 820031, KERUB_GRAIN_SACK, 2);
	EXPECT_TRUE(canAct(1103, QuestActionType::ACTION_ITEM_USE, at(sack))) << "2 of 3";
	holdItem(*me, 820032, KERUB_GRAIN_SACK, 1);
	EXPECT_FALSE(canAct(1103, QuestActionType::ACTION_ITEM_USE, at(sack))) << "3 of 3";
	holdItem(*me, 820033, KERUB_GRAIN_SACK, 1);
	EXPECT_FALSE(canAct(1103, QuestActionType::ACTION_ITEM_USE, at(sack))) << "4 of 3";
	EXPECT_TRUE(canAct(1103, QuestActionType::ITEM_USE, at(sack))) << "not an action item use";
	EXPECT_TRUE(canAct(1103, QuestActionType::ACTION_ITEM_USE, at(mires))) << "mires drops nothing for 1103";
	EXPECT_TRUE(canAct(1103, QuestActionType::ACTION_ITEM_USE, nullptr)) << "no target (id 0)";

	player().getQuestStateList()->deleteQuest(1103);
	hold(*me, 1101, QuestStatus::START);
	EXPECT_TRUE(canAct(1101, QuestActionType::ACTION_ITEM_USE, at(sack))) << "the env's quest: 1101 in START, without quest drops";
	EXPECT_TRUE(PlainHandler(1101).onCanAct(*envOf(*me, 1101, DialogAction::USE_OBJECT, at(sack)), QuestActionType::ACTION_ITEM_USE,
		std::span<const std::any>())) << "a handler without action items";
	hold(*me, 1103, QuestStatus::START);
	EXPECT_TRUE(PlainHandler(1101).onCanAct(*envOf(*me, 1103, DialogAction::USE_OBJECT, at(sack)), QuestActionType::ACTION_ITEM_USE,
		std::span<const std::any>())) << "the handler's action items decide (1101 has none), although 4 of the env's 1103's 3 sacks are held";
}

// onCanAct takes the item of the first quest drop of the target (the break, AbstractQuestHandler.java:236): 3905's treasure box 700462 drops
// 186000067, 186000068 and 186000069 in that order (quest_data.xml:21650-21652), one of each collected - holding the first stops the use, the
// other two are not looked at
TEST_F(AbstractQuestHandlerStepTest, OnCanActReadsTheFirstDropOfTheTarget) {
	PlainHandler dragon(3905);
	Npc& box = npcOf(TREASURE_BOX);
	auto canAct = [&] {
		return dragon.onCanAct(*envOf(*me, 3905, DialogAction::USE_OBJECT, at(box)), QuestActionType::ACTION_ITEM_USE, std::span<const std::any>());
	};
	hold(*me, 3905, QuestStatus::START);
	EXPECT_TRUE(canAct()) << "none held";
	holdItem(*me, 820035, LIGHT_BLADE_FRAGMENT, 1);
	EXPECT_FALSE(canAct()) << "186000067, the first drop's item, is held; 186000069, the last, is not";
}

// getActionItems (AbstractQuestHandler.java:71-85): the 7xxxxx npcs of the quest drops - quest objects - or an empty set
TEST_F(AbstractQuestHandlerStepTest, GetActionItemsNamesTheQuestObjectsOfTheQuestDrops) {
	EXPECT_EQ(PlainHandler(1103).getActionItems(), (std::unordered_set<int32_t>{GRAIN_SACK}));
	EXPECT_TRUE(PlainHandler(1111).getActionItems().empty()) << "210261 and 210674 are monsters";
	EXPECT_TRUE(PlainHandler(1101).getActionItems().empty()) << "no quest drop";
	EXPECT_TRUE(PlainHandler(4242).getActionItems().empty()) << "no template";
}

// defaultOnKillEvent (AbstractQuestHandler.java:671-747): a kill of a listed npc counts a variable from startVar up to endVar (exclusive), or,
// with a reward flag, ends the step at exactly startVar (REWARD, or the variable + 1); another npc, another step or a quest not in START is
// false
TEST_F(AbstractQuestHandlerStepTest, DefaultOnKillEventCountsAListedKill) {
	PlainHandler handler(1102);
	Npc& kerub = npcOf(STRIPED_KERUB);
	Npc& mires = npcOf(MIRES);
	Ref<QuestEnv> kill = envOf(*me, 1102, DialogAction::NULL_, at(kerub));
	EXPECT_FALSE(handler.defaultOnKillEvent(*kill, STRIPED_KERUB, 0, 3)) << "no quest state";
	Ref<QuestState> qs = hold(*me, 1102, QuestStatus::START);
	EXPECT_TRUE(handler.defaultOnKillEvent(*kill, STRIPED_KERUB, 0, 3));
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1102, START, 1)}));
	EXPECT_TRUE(handler.defaultOnKillEvent(*kill, STRIPED_KERUB, 0, 3));
	EXPECT_TRUE(handler.defaultOnKillEvent(*kill, STRIPED_KERUB, 0, 3));
	EXPECT_EQ(vars(qs), 3);
	me->clearSent();
	EXPECT_FALSE(handler.defaultOnKillEvent(*kill, STRIPED_KERUB, 0, 3)) << "3 is endVar";
	EXPECT_FALSE(handler.defaultOnKillEvent(*kill, STRIPED_KERUB, 4, 6)) << "3 is below startVar";
	EXPECT_FALSE(handler.defaultOnKillEvent(*envOf(*me, 1102, DialogAction::NULL_, at(mires)), STRIPED_KERUB, 0, 5)) << "another npc";
	EXPECT_TRUE(me->sent().empty());

	const std::array<int32_t, 2> kerubs{KERUB_2, STRIPED_KERUB};
	EXPECT_TRUE(handler.defaultOnKillEvent(*kill, kerubs, 3, 5)) << "one of several npcs";
	EXPECT_EQ(vars(qs), 4);
	EXPECT_TRUE(handler.defaultOnKillEvent(*kill, STRIPED_KERUB, 0, 1, 1)) << "var1";
	EXPECT_EQ(vars(qs), 4 + (1 << 6));
	EXPECT_TRUE(handler.defaultOnKillEvent(*kill, kerubs, 1, 2, 1));
	EXPECT_EQ(vars(qs), 4 + (2 << 6));

	qs->setQuestVar(1);
	EXPECT_FALSE(handler.defaultOnKillEvent(*kill, STRIPED_KERUB, 0, false)) << "the reward form needs exactly startVar";
	EXPECT_TRUE(handler.defaultOnKillEvent(*kill, STRIPED_KERUB, 1, false));
	EXPECT_EQ(vars(qs), 2);
	EXPECT_TRUE(handler.defaultOnKillEvent(*kill, kerubs, 2, false));
	EXPECT_EQ(vars(qs), 3);
	EXPECT_TRUE(handler.defaultOnKillEvent(*kill, STRIPED_KERUB, 0, false, 1));
	EXPECT_EQ(vars(qs), 3 + (1 << 6));
	me->clearSent();
	EXPECT_TRUE(handler.defaultOnKillEvent(*kill, kerubs, 3, true, 0));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(vars(qs), 3 + (1 << 6)) << "REWARD keeps the variable";
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1102, REWARD, 3 + (1 << 6)), noNearbyQuests()}));
	EXPECT_FALSE(handler.defaultOnKillEvent(*kill, STRIPED_KERUB, 3, 5)) << "not START";
	EXPECT_FALSE(handler.defaultOnKillEvent(*kill, STRIPED_KERUB, 3, true)) << "not START";
}

// defaultOnKillRankedEvent (AbstractQuestHandler.java:749-783), and defaultOnKillInZoneEvent, which is the same body: below endVar - 1 the step
// moves on (changeQuestStep); at endVar - 1 it rewards, or counts one more; beyond it only the state is sent again - true in START in every case.
// Data-driven, var1 counts and the end sets the whole vars to var0 + 1.
TEST_F(AbstractQuestHandlerStepTest, DefaultOnKillRankedEventCountsUpToTheEndStep) {
	PlainHandler handler(1102);
	Ref<QuestEnv> env = envOf(*me, 1102, DialogAction::NULL_);
	EXPECT_FALSE(handler.defaultOnKillRankedEvent(*env, 0, 3, false)) << "no quest state";
	Ref<QuestState> qs = hold(*me, 1102, QuestStatus::START);
	EXPECT_TRUE(handler.defaultOnKillRankedEvent(*env, 0, 3, false));
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1102, START, 1)}));
	EXPECT_TRUE(handler.defaultOnKillRankedEvent(*env, 0, 3, false));
	me->clearSent();
	EXPECT_TRUE(handler.defaultOnKillRankedEvent(*env, 0, 3, false)) << "var 2 is endVar - 1: counted";
	EXPECT_EQ(vars(qs), 3);
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1102, START, 3)}));
	me->clearSent();
	EXPECT_TRUE(handler.defaultOnKillRankedEvent(*env, 0, 3, false)) << "beyond: the state again";
	EXPECT_EQ(vars(qs), 3);
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1102, START, 3)}));
	qs->setQuestVar(0);
	EXPECT_TRUE(handler.defaultOnKillRankedEvent(*env, 1, 3, false)) << "below startVar: the state again";
	EXPECT_EQ(vars(qs), 0);

	qs->setQuestVar(4);
	me->clearSent();
	EXPECT_TRUE(handler.defaultOnKillInZoneEvent(*env, 2, 5, true)) << "var 4 is endVar - 1: rewarded";
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(vars(qs), 4);
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1102, REWARD, 4), noNearbyQuests()}));
	EXPECT_FALSE(handler.defaultOnKillInZoneEvent(*env, 2, 5, true)) << "not START";

	qs->setStatus(QuestStatus::START);
	qs->setQuestVar(1); // var0 1, var1 0
	me->clearSent();
	EXPECT_TRUE(handler.defaultOnKillRankedEvent(*env, 0, 3, false, true));
	EXPECT_EQ(vars(qs), 1 + (1 << 6)) << "data-driven: var1";
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1102, START, 1 + (1 << 6))}));
	EXPECT_TRUE(handler.defaultOnKillInZoneEvent(*env, 0, 3, false, true));
	EXPECT_EQ(vars(qs), 1 + (2 << 6));
	me->clearSent();
	EXPECT_TRUE(handler.defaultOnKillRankedEvent(*env, 0, 3, true, true)) << "var1 2 is endVar - 1";
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(vars(qs), 2) << "setQuestVar(var0 + 1): the whole vars, var1 cleared";
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1102, REWARD, 2), noNearbyQuests()}));
	qs->setStatus(QuestStatus::START);
	qs->setQuestVar(5 + (2 << 6));
	EXPECT_TRUE(handler.defaultOnKillRankedEvent(*env, 0, 3, false, true));
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_EQ(vars(qs), 6);
}

// defaultOnUseSkillEvent, defaultOnGetItemEvent and defaultFollowEndEvent (AbstractQuestHandler.java:793-804, 835-864): a step inside the range
// (the skill) or at the step (the item, the follow end) moves on in START; the follow end plays its movie after the step
TEST_F(AbstractQuestHandlerStepTest, TheSkillItemAndFollowEndDefaultsMoveTheStepInStart) {
	PlainHandler handler(1111);
	Npc& mires = npcOf(MIRES);
	Ref<QuestEnv> env = envOf(*me, 1111, DialogAction::NULL_, at(mires));
	EXPECT_FALSE(handler.defaultOnUseSkillEvent(*env, 0, 2, 1));
	EXPECT_FALSE(handler.defaultOnGetItemEvent(*env, 0, 1, false));
	EXPECT_FALSE(handler.defaultFollowEndEvent(*env, 0, 1, false));
	Ref<QuestState> qs = hold(*me, 1111, QuestStatus::START);

	EXPECT_TRUE(handler.defaultOnUseSkillEvent(*env, 0, 2, 1));
	EXPECT_TRUE(handler.defaultOnUseSkillEvent(*env, 0, 2, 1));
	EXPECT_EQ(vars(qs), 2 << 6);
	EXPECT_FALSE(handler.defaultOnUseSkillEvent(*env, 0, 2, 1)) << "var1 2 is endVar";
	EXPECT_FALSE(handler.defaultOnUseSkillEvent(*env, 3, 5, 1)) << "var1 2 is below startVar";
	EXPECT_TRUE(handler.defaultOnUseSkillEvent(*env, 0, 1, 0));
	EXPECT_EQ(vars(qs), 1 + (2 << 6));

	me->clearSent();
	EXPECT_FALSE(handler.defaultOnGetItemEvent(*env, 0, 2, false)) << "the step is 1";
	EXPECT_TRUE(handler.defaultOnGetItemEvent(*env, 1, 2, false));
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1111, START, 2 + (2 << 6))}));

	me->clearSent();
	EXPECT_TRUE(handler.defaultFollowEndEvent(*env, 2, 3, false, 77));
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1111, START, 3 + (2 << 6)), playMovie(false, mires.getObjectId(), 1111, 77)}));
	me->clearSent();
	EXPECT_TRUE(handler.defaultFollowEndEvent(*env, 3, 4, false));
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1111, START, 4 + (2 << 6))})) << "no movie";
	EXPECT_FALSE(handler.defaultFollowEndEvent(*env, 3, 4, false)) << "the step is 4";

	EXPECT_TRUE(handler.defaultOnGetItemEvent(*env, 4, 4, true));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_FALSE(handler.defaultOnGetItemEvent(*env, 4, 5, false)) << "not START";
	EXPECT_FALSE(handler.defaultOnUseSkillEvent(*env, 0, 5, 0)) << "not START";
	EXPECT_FALSE(handler.defaultFollowEndEvent(*env, 4, 5, false)) << "not START";
}

// playQuestMovie (AbstractQuestHandler.java:661-669): SM_PLAY_MOVIE of the env's quest and target (0 without one), skippable, cutscene or not;
// the answer is always false
TEST_F(AbstractQuestHandlerStepTest, PlayQuestMovieSendsTheMovieAndAnswersFalse) {
	PlainHandler handler(1101);
	Npc& mires = npcOf(MIRES);
	EXPECT_FALSE(handler.playQuestMovie(*envOf(*me, 1111, DialogAction::NULL_, at(mires)), 55));
	EXPECT_FALSE(handler.playQuestMovie(*envOf(*me, 1111, DialogAction::NULL_, at(mires)), 3, true));
	EXPECT_FALSE(handler.playQuestMovie(*envOf(*me, 0, DialogAction::NULL_), 55, false));
	EXPECT_EQ(me->sent(), cp::exactly({playMovie(false, mires.getObjectId(), 1111, 55), playMovie(true, mires.getObjectId(), 1111, 3),
							  playMovie(false, 0, 0, 55)}));
}

// sendEmotion (AbstractQuestHandler.java:611-615): SM_EMOTION(EMOTE) of the emoting creature aimed at the player - or, when the player emotes,
// at the env's object; broadcast decides whether the player gets it too
TEST_F(AbstractQuestHandlerStepTest, SendEmotionAimsTheEmoteAtTheOtherSide) {
	PlainHandler handler(1101);
	Npc& mires = npcOf(MIRES);
	Ref<QuestEnv> env = envOf(*me, 1101, DialogAction::NULL_, at(mires));
	using gameserver::model::EmotionId;
	using gameserver::model::EmotionType;
	handler.sendEmotion(*env, mires, EmotionId::SAD, true);
	EXPECT_EQ(me->sent(), cp::exactly({me->serializedFor(network::aion::serverpackets::SM_EMOTION(mires, EmotionType::EMOTE,
							  gameserver::model::id(EmotionId::SAD), player().getObjectId()))}));
	me->clearSent();
	handler.sendEmotion(*env, player(), EmotionId::POINT, true);
	EXPECT_EQ(me->sent(), cp::exactly({me->serializedFor(network::aion::serverpackets::SM_EMOTION(player(), EmotionType::EMOTE,
							  gameserver::model::id(EmotionId::POINT), mires.getObjectId()))}));
	me->clearSent();
	handler.sendEmotion(*env, mires, EmotionId::SAD, false);
	EXPECT_TRUE(me->sent().empty()) << "not to the player himself";
}

// defaultOnLevelChangedEvent (AbstractQuestHandler.java:988-1036) on the campaign 1001 (level 2, pre-quest 1100): only a quest the player does
// not hold or holds LOCKED starts; a missing pre-quest stops it (for a campaign with no state yet: unless a later pre-quest is done, which
// locks it); a failed <start_conditions> locks a campaign; a player below the level (allowed 2 below for the check) gets it LOCKED; else START
TEST_F(AbstractQuestHandlerStepTest, DefaultOnLevelChangedEventStartsOrLocksTheCampaign) {
	PlainHandler campaign(1001);
	EXPECT_FALSE(campaign.defaultOnLevelChangedEvent(player(), {1100})) << "1100 missing";
	EXPECT_FALSE(player().getQuestStateList()->hasQuest(1001));
	EXPECT_FALSE(campaign.defaultOnLevelChangedEvent(player(), {1100, 1101})) << "both missing";
	EXPECT_FALSE(player().getQuestStateList()->hasQuest(1001));
	hold(*me, 1101, QuestStatus::COMPLETE);
	me->clearSent();
	EXPECT_FALSE(campaign.defaultOnLevelChangedEvent(player(), {1100, 1101})) << "1100 missing, 1101 done: locked";
	EXPECT_EQ(player().getQuestStateList()->getQuestState(1001)->getStatus(), QuestStatus::LOCKED);
	EXPECT_EQ(me->sent(), cp::exactly({questAction(1, 1001, LOCKED)}));

	player().setQuestStateList(gameserver::model::gameobjects::player::QuestStateList::create());
	hold(*me, 1100, QuestStatus::COMPLETE);
	EXPECT_FALSE(campaign.defaultOnLevelChangedEvent(player(), {1100})) << "level 1 of 2";
	EXPECT_EQ(player().getQuestStateList()->getQuestState(1001)->getStatus(), QuestStatus::LOCKED);
	me->clearSent();
	EXPECT_FALSE(campaign.defaultOnLevelChangedEvent(player(), {1100})) << "still level 1, already LOCKED";
	EXPECT_TRUE(me->sent().empty());
	EXPECT_FALSE(PlainHandler(1101).defaultOnLevelChangedEvent(player(), {1111, 1100})) << "not a campaign: the first missing pre-quest ends it";
	EXPECT_FALSE(player().getQuestStateList()->hasQuest(1101)) << "no lock, although 1100 is done";

	Quester& second = *makeQuester(810111, "Second", gameserver::model::Race::ELYOS, 2);
	hold(second, 1100, QuestStatus::COMPLETE);
	EXPECT_TRUE(campaign.defaultOnLevelChangedEvent(second.player(), {1100}));
	EXPECT_EQ(second.sent(), cp::exactly({questAction(1, 1001, START)}));
	EXPECT_FALSE(campaign.defaultOnLevelChangedEvent(second.player(), {1100})) << "in START";
	second.player().getQuestStateList()->getQuestState(1001)->setStatus(QuestStatus::LOCKED);
	second.clearSent();
	EXPECT_TRUE(campaign.defaultOnLevelChangedEvent(second.player(), {1100})) << "LOCKED starts";
	EXPECT_EQ(second.sent(), cp::exactly({questAction(2, 1001, START)}));

	// 2015 (level 12) needs 2014 by <start_conditions>
	PlainHandler scouting(2015);
	Quester& asmodian = *makeQuester(810112, "Rookie", gameserver::model::Race::ASMODIANS, 13);
	EXPECT_FALSE(scouting.defaultOnLevelChangedEvent(asmodian.player()));
	EXPECT_EQ(asmodian.player().getQuestStateList()->getQuestState(2015)->getStatus(), QuestStatus::LOCKED) << "2014 not done";
	asmodian.player().getQuestStateList()->deleteQuest(2015);
	hold(asmodian, 2014, QuestStatus::COMPLETE);
	EXPECT_TRUE(scouting.defaultOnLevelChangedEvent(asmodian.player()));
	EXPECT_EQ(asmodian.player().getQuestStateList()->getQuestState(2015)->getStatus(), QuestStatus::START);
	Quester& young = *makeQuester(810113, "Youngster", gameserver::model::Race::ASMODIANS, 10);
	hold(young, 2014, QuestStatus::COMPLETE);
	EXPECT_FALSE(scouting.defaultOnLevelChangedEvent(young.player())) << "level 10 of 12";
	EXPECT_EQ(young.player().getQuestStateList()->getQuestState(2015)->getStatus(), QuestStatus::LOCKED);
}

// defaultOnQuestCompletedEvent (AbstractQuestHandler.java:1047-1133): as the level change, with 15 levels allowed below; a campaign whose
// <start_conditions> fail is locked if any quest of its <finished> chain is done (hasAnyPreQuestFinished follows 2017 -> 2015 -> 2014); a pre-quest
// just finished (the env's quest) locks a campaign still too high for the player
TEST_F(AbstractQuestHandlerStepTest, DefaultOnQuestCompletedEventFollowsThePreQuestChain) {
	PlainHandler observatory(2017);
	Quester& asmodian = *makeQuester(810121, "Novice", gameserver::model::Race::ASMODIANS, 1);
	auto completed = [&](int32_t finishedQuestId, std::initializer_list<int32_t> preQuests) {
		return observatory.defaultOnQuestCompletedEvent(*envOf(asmodian, finishedQuestId, DialogAction::NULL_), preQuests);
	};
	EXPECT_FALSE(completed(2014, {})) << "no quest of the chain done";
	EXPECT_FALSE(asmodian.player().getQuestStateList()->hasQuest(2017));
	hold(asmodian, 2014, QuestStatus::COMPLETE);
	EXPECT_FALSE(completed(2014, {}));
	EXPECT_EQ(asmodian.player().getQuestStateList()->getQuestState(2017)->getStatus(), QuestStatus::LOCKED) << "2014 two links back is done";
	EXPECT_FALSE(completed(2014, {})) << "already LOCKED";
	EXPECT_EQ(asmodian.player().getQuestStateList()->getQuestState(2017)->getStatus(), QuestStatus::LOCKED);

	asmodian.player().getQuestStateList()->deleteQuest(2017);
	hold(asmodian, 2015, QuestStatus::COMPLETE);
	EXPECT_FALSE(completed(2014, {})) << "level 1 of 15, no pre-quest just finished";
	EXPECT_FALSE(asmodian.player().getQuestStateList()->hasQuest(2017));
	EXPECT_FALSE(completed(2015, {2015})) << "level 1 of 15, 2015 just finished";
	EXPECT_EQ(asmodian.player().getQuestStateList()->getQuestState(2017)->getStatus(), QuestStatus::LOCKED);

	asmodian.player().getQuestStateList()->deleteQuest(2017);
	asmodian.player().getQuestStateList()->deleteQuest(2015);
	EXPECT_FALSE(completed(2014, {2015})) << "2015 missing";
	EXPECT_FALSE(asmodian.player().getQuestStateList()->hasQuest(2017));
	EXPECT_FALSE(completed(2014, {2014, 2015})) << "2015 missing, 2014 just finished: locked";
	EXPECT_EQ(asmodian.player().getQuestStateList()->getQuestState(2017)->getStatus(), QuestStatus::LOCKED);

	PlainHandler campaign(1001);
	Quester& elyos = *makeQuester(810122, "Kalio", gameserver::model::Race::ELYOS, 2);
	hold(elyos, 1100, QuestStatus::COMPLETE);
	EXPECT_TRUE(campaign.defaultOnQuestCompletedEvent(*envOf(elyos, 1100, DialogAction::NULL_), {1100}));
	EXPECT_EQ(elyos.sent(), cp::exactly({questAction(1, 1001, START)}));
	EXPECT_FALSE(campaign.defaultOnQuestCompletedEvent(*envOf(elyos, 1100, DialogAction::NULL_), {1100})) << "in START";
	hold(*me, 1100, QuestStatus::COMPLETE);
	EXPECT_FALSE(PlainHandler(1101).defaultOnQuestCompletedEvent(*envOf(*me, 1100, DialogAction::NULL_), {1111, 1100}))
		<< "not a campaign: the first missing pre-quest ends it";
	EXPECT_FALSE(player().getQuestStateList()->hasQuest(1101)) << "no lock, although 1100 was just finished";
}

// Both campaign starts ask checkStartConditions first and silently (warn false; AbstractQuestHandler.java:998, 1060): an Asmodian who holds 1100
// COMPLETE starts the Elyos campaign 1001 by neither (the race), and a level-9 Asmodian does not get 2015 (level 12, 2 below allowed) even
// LOCKED - no state, no packet, no refusal message
TEST_F(AbstractQuestHandlerStepTest, TheCampaignStartsRefuseSilentlyWhatCheckStartConditionsRefuses) {
	PlainHandler campaign(1001);
	Quester& asmodian = *makeQuester(810131, "Stranger", gameserver::model::Race::ASMODIANS, 2);
	hold(asmodian, 1100, QuestStatus::COMPLETE);
	EXPECT_FALSE(campaign.defaultOnLevelChangedEvent(asmodian.player(), {1100}));
	EXPECT_FALSE(asmodian.player().getQuestStateList()->hasQuest(1001)) << "the level change";
	EXPECT_FALSE(campaign.defaultOnQuestCompletedEvent(*envOf(asmodian, 1100, DialogAction::NULL_), {1100}));
	EXPECT_FALSE(asmodian.player().getQuestStateList()->hasQuest(1001)) << "the quest completion";
	EXPECT_TRUE(asmodian.sent().empty());

	Quester& young = *makeQuester(810132, "Toddler", gameserver::model::Race::ASMODIANS, 9);
	EXPECT_FALSE(PlainHandler(2015).defaultOnLevelChangedEvent(young.player()));
	EXPECT_FALSE(young.player().getQuestStateList()->hasQuest(2015)) << "level 9: 3 below 12";
	EXPECT_TRUE(young.sent().empty());
}

// defaultOnQuestCompletedEvent (AbstractQuestHandler.java:1083-1096): a campaign whose <start_conditions> fail is locked only if a quest of the
// failed condition's <finished> chain is COMPLETE; else it is left alone at any level - two level-15 Asmodians (2017's level) who have just
// finished 24112: one has done no quest of 2014 -> 2015, the other holds 2015 only LOCKED
TEST_F(AbstractQuestHandlerStepTest, DefaultOnQuestCompletedEventLeavesACampaignWithoutADoneQuestOfItsChain) {
	PlainHandler observatory(2017);
	auto completed = [&](Quester& quester) {
		return observatory.defaultOnQuestCompletedEvent(*envOf(quester, 24112, DialogAction::NULL_), {});
	};
	Quester& veteran = *makeQuester(810141, "Veteran", gameserver::model::Race::ASMODIANS, 15);
	hold(veteran, 24112, QuestStatus::COMPLETE);
	EXPECT_FALSE(completed(veteran));
	EXPECT_FALSE(veteran.player().getQuestStateList()->hasQuest(2017)) << "neither 2015 nor 2014 done";
	EXPECT_TRUE(veteran.sent().empty());

	Quester& watcher = *makeQuester(810142, "Watcher", gameserver::model::Race::ASMODIANS, 15);
	hold(watcher, 24112, QuestStatus::COMPLETE);
	hold(watcher, 2015, QuestStatus::LOCKED);
	EXPECT_FALSE(completed(watcher));
	EXPECT_FALSE(watcher.player().getQuestStateList()->hasQuest(2017)) << "2015 LOCKED is not done, 2014 not held";
	EXPECT_TRUE(watcher.sent().empty());
}

// For a quest that is not a MISSION both campaign starts end at the first failed <start_conditions> (AbstractQuestHandler.java:1018-1022,
// 1085-1087), without a lock. 2911 (QUEST) starts after 2009 whatever its reward: six conditions, <finished 2009 reward="0"> to "5", of which
// checkStartConditions needs one (conditions with <finished> count once together, QuestTemplate.getRequiredConditionCount). 2009 finished with
// reward 0 passes that check and fails the second condition - where a MISSION would be locked, 2009 being done
TEST_F(AbstractQuestHandlerStepTest, TheCampaignStartsNeverLockAQuestThatIsNotAMission) {
	PlainHandler blessing(2911);
	Quester& asmodian = *makeQuester(810151, "Singer", gameserver::model::Race::ASMODIANS, 10);
	hold(asmodian, 2009, QuestStatus::COMPLETE)->setRewardGroup(0);
	EXPECT_FALSE(blessing.defaultOnLevelChangedEvent(asmodian.player()));
	EXPECT_FALSE(asmodian.player().getQuestStateList()->hasQuest(2911)) << "the level change";
	EXPECT_FALSE(blessing.defaultOnQuestCompletedEvent(*envOf(asmodian, 2009, DialogAction::NULL_), {2009}));
	EXPECT_FALSE(asmodian.player().getQuestStateList()->hasQuest(2911)) << "the quest completion";
	EXPECT_TRUE(asmodian.sent().empty());
}

// defaultOnEnterZoneEvent (AbstractQuestHandler.java:1136-1149): entering the quest's zone starts a quest the player does not hold, with the env
// set to the handler's quest; another zone, a held quest or no player answer false
TEST_F(AbstractQuestHandlerStepTest, DefaultOnEnterZoneEventStartsTheQuestInItsZone) {
	PlainHandler handler(1101);
	const world::zone::ZoneName* questZone = world::zone::ZoneName::createOrGet("AGERS_FARM_210010000");
	const world::zone::ZoneName* otherZone = world::zone::ZoneName::createOrGet("AKARIOS_PLAINS_210010000");
	Ref<QuestEnv> env = QuestEnv::create(nullptr, player(), 0);
	EXPECT_FALSE(handler.defaultOnEnterZoneEvent(*env, otherZone, questZone));
	EXPECT_EQ(env->getQuestId(), 0);
	EXPECT_TRUE(handler.defaultOnEnterZoneEvent(*env, questZone, questZone));
	EXPECT_EQ(env->getQuestId(), 1101);
	EXPECT_EQ(player().getQuestStateList()->getQuestState(1101)->getStatus(), QuestStatus::START);
	EXPECT_EQ(me->sent(), cp::exactly({questAction(1, 1101, START), noNearbyQuests()}));
	EXPECT_FALSE(handler.defaultOnEnterZoneEvent(*env, questZone, questZone)) << "held";

	Ref<QuestEnv> high = QuestEnv::create(nullptr, player(), 0);
	EXPECT_FALSE(PlainHandler(1111).defaultOnEnterZoneEvent(*high, questZone, questZone)) << "1111 needs level 3";
	EXPECT_EQ(high->getQuestId(), 1111) << "set before the start";
	EXPECT_FALSE(player().getQuestStateList()->hasQuest(1111));
	high->setPlayer(nullptr);
	EXPECT_FALSE(PlainHandler(1111).defaultOnEnterZoneEvent(*high, questZone, questZone)) << "no player";
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::test
