// P5-06b, M5d H-01 and H-07 (m5d-plan.md §7): the dialog helpers of AbstractQuestHandler (AbstractQuestHandler.java:93-114, 329-529,
// 1151-1236) on real quest templates - the page each dialog action answers, the two reward-page exploit guards, and sendQuestEndDialog's
// continuation after a finished quest: another quest in REWARD at the npc, else the follow-up quest (the first startable quest of the npc's
// QuestNpc.getOnQuestStart() - Java's `new HashSet<>(0)`, so in its table order, which the handler replays - whose <finished> precondition is
// the quest just completed and that pays something), else the quest-selection page or a closed window.
//
// The follow-up cases register ProbeHandlers the way the templates register (a start npc gets onQuestStart and onTalkEvent, an end npc
// onTalkEvent): QuestEngine.onDialog hands the follow-up to the probe, which records what it got. The quests and npcs are the shipped ones
// (QuestHandlerTestSupport.h): the Poeta chain 1101 -> 1102 at mires, 1194 -> 1195 at spatalos, 80677 -> 80673-80676 at sonatine, and
// 24112's three follow-ups 2236, 2237 and 2292, which the data starts at anmurnerk 832822 (altgard.xml:182-183, 201) - 24112 itself ends at
// Brodir 832821 (_24112NoLaissezFaireForLepharists.java:18), so its fabricated handler here reports at anmurnerk to have the three candidates
// at one npc.
//
// Not asserted: the AI event DIALOG_FINISH defaultCloseDialog hands a npc (AbstractQuestHandler.java:524-525) - the npcs here run AIEngine's
// substitute AI (the warn mode of AIConfig.MISSING_AI_HANDLERS), which ignores it.

#include "QuestHandlerTestSupport.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestItems.h"
#include "aion/gameserver/model/templates/quest/QuestWorkItems.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/ChatUtil.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test {
namespace {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::gameobjects::Npc;
using gameserver::model::gameobjects::VisibleObject;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

Ptr<VisibleObject> at(VisibleObject& object) {
	return Ptr<VisibleObject>(object);
}

class AbstractQuestHandlerDialogTest : public QuestHandlerTest {};

// onDialogEvent (AbstractQuestHandler.java:93-114) at an npc: the accept window of the handler's quest (DialogPage.ASK_QUEST_ACCEPT_WINDOW, 4) -
// the handler's id, not the env's - and the pages 1003-1007 through sendQuestDialog, which sends the env's quest id; FINISH_DIALOG answers
// true without a packet, every other action false
TEST_F(AbstractQuestHandlerDialogTest, OnDialogEventAnswersEachActionWithItsPage) {
	PlainHandler handler(1101);
	Npc& mires = npcOf(MIRES);
	const int32_t npc = mires.getObjectId();
	struct Row {
		int32_t action;
		int32_t page; // -1: no packet
		bool result;
	};
	const std::array<Row, 11> rows{{
		{DialogAction::QUEST_ACCEPT_1, 1003, true},
		{DialogAction::QUEST_REFUSE, 1004, true},
		{DialogAction::QUEST_REFUSE_SIMPLE, 1004, true},
		{DialogAction::QUEST_REFUSE_1, 1004, true},
		{DialogAction::QUEST_REFUSE_2, 1005, true},
		{DialogAction::QUEST_REFUSE_3, 1006, true},
		{DialogAction::QUEST_REFUSE_4, 1007, true},
		{DialogAction::FINISH_DIALOG, -1, true},
		{DialogAction::QUEST_SELECT, -1, false},
		{DialogAction::QUEST_ACCEPT, -1, false},
		{DialogAction::SELECT_QUEST_REWARD, -1, false},
	}};
	for (const Row& row : rows) {
		me->clearSent();
		EXPECT_EQ(handler.onDialogEvent(*envOf(*me, 0, row.action, at(mires))), row.result) << row.action;
		if (row.page < 0)
			EXPECT_TRUE(me->sent().empty()) << row.action;
		else
			EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(npc, row.page, 0)})) << row.action << ": the env's quest id (0)";
	}

	me->clearSent();
	EXPECT_TRUE(handler.onDialogEvent(*envOf(*me, 0, DialogAction::ASK_QUEST_ACCEPT, at(mires))));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(npc, 4, 1101)})) << "the handler's quest id (:96)";
}

// Without an npc - no visible object, or the player himself (a quest item or object is not an Npc either) - the accept and refuse actions close
// the window (AbstractQuestHandler.java:99-109, 359-362): SM_DIALOG_WINDOW(object or 0, 0, 0)
TEST_F(AbstractQuestHandlerDialogTest, OnDialogEventWithoutAnNpcClosesTheWindow) {
	PlainHandler handler(1101);
	for (int32_t action : {DialogAction::QUEST_ACCEPT_1, DialogAction::QUEST_REFUSE, DialogAction::QUEST_REFUSE_SIMPLE, DialogAction::QUEST_REFUSE_1,
			 DialogAction::QUEST_REFUSE_2, DialogAction::QUEST_REFUSE_3, DialogAction::QUEST_REFUSE_4}) {
		me->clearSent();
		EXPECT_TRUE(handler.onDialogEvent(*envOf(*me, 1101, action)));
		EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(0, 0, 0)})) << action;
		me->clearSent();
		EXPECT_TRUE(handler.onDialogEvent(*envOf(*me, 1101, action, at(player()))));
		EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(player().getObjectId(), 0, 0)})) << action << ": not an Npc";
	}
	me->clearSent();
	EXPECT_TRUE(handler.onDialogEvent(*envOf(*me, 1101, DialogAction::ASK_QUEST_ACCEPT)));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(0, 4, 1101)}));
}

// sendQuestDialog (AbstractQuestHandler.java:330-344): the ten reward pages (5-8, 45-50) are sent only while the handler's quest is in REWARD -
// the reward packet exploitation fix - every other page always; the packet carries the env's quest id (0 here, as for Kromede's entry)
TEST_F(AbstractQuestHandlerDialogTest, SendQuestDialogSendsARewardPageOnlyInReward) {
	PlainHandler handler(1111);
	Npc& mires = npcOf(MIRES);
	const int32_t npc = mires.getObjectId();
	Ref<QuestEnv> env = envOf(*me, 0, DialogAction::QUEST_SELECT, at(mires));
	const std::array<int32_t, 10> rewardPages{5, 6, 7, 8, 45, 46, 47, 48, 49, 50};
	for (int32_t page : rewardPages) {
		me->clearSent();
		EXPECT_FALSE(handler.sendQuestDialog(*env, page)) << page << ": no quest state";
		EXPECT_TRUE(me->sent().empty()) << page;
	}
	Ref<QuestState> qs = hold(*me, 1111, QuestStatus::START);
	for (QuestStatus status : {QuestStatus::START, QuestStatus::COMPLETE, QuestStatus::LOCKED}) {
		qs->setStatus(status);
		for (int32_t page : rewardPages) {
			me->clearSent();
			EXPECT_FALSE(handler.sendQuestDialog(*env, page)) << page << " in " << static_cast<int>(status);
			EXPECT_TRUE(me->sent().empty()) << page;
		}
		for (int32_t page : {4, 9, 44, 51, 1011, 1352}) {
			me->clearSent();
			EXPECT_TRUE(handler.sendQuestDialog(*env, page)) << page << " in " << static_cast<int>(status);
			EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(npc, page, 0)})) << page;
		}
	}
	qs->setStatus(QuestStatus::REWARD);
	for (int32_t page : rewardPages) {
		me->clearSent();
		EXPECT_TRUE(handler.sendQuestDialog(*env, page)) << page;
		EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(npc, page, 0)})) << page;
	}
}

// sendQuestSelectionDialog and closeDialogWindow (AbstractQuestHandler.java:354-362): page 10 and page 0, both with quest 0
TEST_F(AbstractQuestHandlerDialogTest, TheSelectionPageAndTheClosedWindowCarryNoQuest) {
	PlainHandler handler(1101);
	Npc& mires = npcOf(MIRES);
	Ref<QuestEnv> env = envOf(*me, 1101, DialogAction::QUEST_SELECT, at(mires));
	EXPECT_TRUE(handler.sendQuestSelectionDialog(*env));
	EXPECT_TRUE(handler.closeDialogWindow(*env));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(mires.getObjectId(), 10, 0), dialogWindow(mires.getObjectId(), 0, 0)}));
}

// sendQuestStartDialog (AbstractQuestHandler.java:373-398): ASK_QUEST_ACCEPT shows page 4; the three accept actions start the quest
// (QuestService.startQuest: SM_QUEST_ACTION(ADD) and the nearby quests) and answer 1003 at an npc, or close the window for QUEST_ACCEPT_SIMPLE and
// without an npc; a refused start answers false with no dialog; QUEST_REFUSE_1/2 page 1004, QUEST_REFUSE_SIMPLE closes, FINISH_DIALOG goes
// back to the selection page, every other action is false
TEST_F(AbstractQuestHandlerDialogTest, SendQuestStartDialogStartsTheQuestAndAnswersEachAction) {
	PlainHandler handler(1101);
	Npc& mires = npcOf(MIRES);
	const int32_t npc = mires.getObjectId();
	auto run = [&](int32_t action, Ptr<VisibleObject> target) {
		me->clearSent();
		return handler.sendQuestStartDialog(*envOf(*me, 1101, action, target));
	};

	EXPECT_TRUE(run(DialogAction::ASK_QUEST_ACCEPT, at(mires)));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(npc, 4, 1101)}));
	EXPECT_FALSE(player().getQuestStateList()->hasQuest(1101));

	for (int32_t action : {DialogAction::QUEST_ACCEPT, DialogAction::QUEST_ACCEPT_1}) {
		player().setQuestStateList(gameserver::model::gameobjects::player::QuestStateList::create());
		EXPECT_TRUE(run(action, at(mires))) << action;
		ASSERT_TRUE(player().getQuestStateList()->hasQuest(1101)) << action;
		EXPECT_EQ(player().getQuestStateList()->getQuestState(1101)->getStatus(), QuestStatus::START);
		EXPECT_EQ(me->sent(), cp::exactly({questAction(1, 1101, START), noNearbyQuests(), dialogWindow(npc, 1003, 1101)})) << action;
	}

	EXPECT_FALSE(run(DialogAction::QUEST_ACCEPT, at(mires))) << "startQuest refuses a quest in START (checkStartConditions)";
	EXPECT_EQ(me->sent(), cp::exactly({me->serializedFor(SM_SYSTEM_MESSAGE::STR_QUEST_ACQUIRE_ERROR_WORKING_QUEST())}))
		<< "the refusal's warning, no dialog";

	player().setQuestStateList(gameserver::model::gameobjects::player::QuestStateList::create());
	EXPECT_TRUE(run(DialogAction::QUEST_ACCEPT_SIMPLE, at(mires)));
	EXPECT_EQ(me->sent(), cp::exactly({questAction(1, 1101, START), noNearbyQuests(), dialogWindow(npc, 0, 0)}));

	player().setQuestStateList(gameserver::model::gameobjects::player::QuestStateList::create());
	EXPECT_TRUE(run(DialogAction::QUEST_ACCEPT_1, nullptr));
	EXPECT_EQ(me->sent(), cp::exactly({questAction(1, 1101, START), noNearbyQuests(), dialogWindow(0, 0, 0)})) << "no npc: closed";

	EXPECT_TRUE(run(DialogAction::QUEST_REFUSE_1, at(mires)));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(npc, 1004, 1101)}));
	EXPECT_TRUE(run(DialogAction::QUEST_REFUSE_2, at(mires)));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(npc, 1004, 1101)}));
	EXPECT_TRUE(run(DialogAction::QUEST_REFUSE_SIMPLE, at(mires)));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(npc, 0, 0)}));
	EXPECT_TRUE(run(DialogAction::FINISH_DIALOG, at(mires)));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(npc, 10, 0)}));
	for (int32_t action : {DialogAction::QUEST_REFUSE, DialogAction::QUEST_REFUSE_3, DialogAction::QUEST_SELECT, DialogAction::USE_OBJECT}) {
		EXPECT_FALSE(run(action, at(mires))) << action;
		EXPECT_TRUE(me->sent().empty()) << action;
	}
}

// sendQuestStartDialog(env, itemId, itemCount) gives the item after the start (giveQuestItem: QUEST_WORK_ITEM), not with a count or id of 0
// (AbstractQuestHandler.java:380-382); the QuestItems overload (:368-370) passes a work item's id and count, and none for null
TEST_F(AbstractQuestHandlerDialogTest, SendQuestStartDialogGivesTheStartItemOnAccept) {
	PlainHandler handler(1101);
	Npc& mires = npcOf(MIRES);
	auto accept = [&](auto&&... item) {
		player().setQuestStateList(gameserver::model::gameobjects::player::QuestStateList::create());
		me->clearSent();
		return handler.sendQuestStartDialog(*envOf(*me, 1101, DialogAction::QUEST_ACCEPT_1, at(mires)), item...);
	};
	EXPECT_TRUE(accept(NYMPHS_DRESS, int64_t{0}));
	EXPECT_EQ(held(*me, NYMPHS_DRESS), 0) << "count 0";
	EXPECT_TRUE(accept(0, int64_t{1}));
	EXPECT_EQ(held(*me, NYMPHS_DRESS), 0) << "id 0";
	EXPECT_TRUE(accept(static_cast<const gameserver::model::templates::quest::QuestItems*>(nullptr)));
	EXPECT_EQ(held(*me, NYMPHS_DRESS), 0) << "no work item";

	// quest 1114's work item (quest_data.xml:993): item 182200217, count 1 by default
	const gameserver::model::templates::quest::QuestItems& workItem =
		dataholders::DataManager::QUEST_DATA->getQuestById(1114)->getQuestWorkItems()->getQuestWorkItem().at(0);
	EXPECT_TRUE(accept(&workItem));
	EXPECT_EQ(held(*me, NYMPHS_DRESS), 1);
	std::vector<std::vector<uint8_t>> sent = me->sent();
	ASSERT_FALSE(sent.empty());
	EXPECT_EQ(sent.front(), questAction(1, 1101, START)) << "the item comes after the start";
	EXPECT_EQ(sent.back(), dialogWindow(mires.getObjectId(), 1003, 1101));
	EXPECT_EQ(items::packetsOf(sent, items::SM_INVENTORY_ADD_ITEM_OPCODE).size(), 1u);

	player().getInventory().decreaseByItemId(NYMPHS_DRESS, 1);
	EXPECT_TRUE(accept(NYMPHS_DRESS, int64_t{2}));
	EXPECT_EQ(held(*me, NYMPHS_DRESS), 2);
}

// The first exploit guard of sendQuestEndDialog (AbstractQuestHandler.java:416-417): a quest that is not in REWARD - here START, reported with
// SELECT_QUEST_REWARD (1009) - answers false and sends nothing. Without the guard it would send SM_DIALOG_WINDOW(npc, DialogPage.NULL, quest)
// and answer true (the reward group is null outside REWARD, DialogPage.java:85-111). Nothing is paid for a SELECTED_QUEST_REWARD either.
TEST_F(AbstractQuestHandlerDialogTest, SendQuestEndDialogOutsideRewardAnswersFalseAndSendsNothing) {
	PlainHandler handler(1101);
	Npc& mires = npcOf(MIRES);
	EXPECT_FALSE(handler.sendQuestEndDialog(*envOf(*me, 1101, DialogAction::SELECT_QUEST_REWARD, at(mires)))) << "no quest state";
	EXPECT_TRUE(me->sent().empty());
	for (QuestStatus status : {QuestStatus::START, QuestStatus::COMPLETE, QuestStatus::LOCKED}) {
		player().setQuestStateList(gameserver::model::gameobjects::player::QuestStateList::create());
		Ref<QuestState> qs = hold(*me, 1101, status);
		for (int32_t action : {DialogAction::SELECT_QUEST_REWARD, DialogAction::USE_OBJECT, DialogAction::SELECTED_QUEST_NOREWARD,
				 DialogAction::SET_SUCCEED}) {
			me->clearSent();
			EXPECT_FALSE(handler.sendQuestEndDialog(*envOf(*me, 1101, action, at(mires)))) << static_cast<int>(status) << " " << action;
			EXPECT_TRUE(me->sent().empty()) << static_cast<int>(status) << " " << action;
		}
		EXPECT_EQ(qs->getStatus(), status);
		EXPECT_EQ(qs->getRewardGroup(), std::nullopt);
	}
	EXPECT_EQ(player().getInventory().getKinah(), 1000);
}

// In REWARD, the report actions (USE_OBJECT, QUEST_SELECT, SELECT_QUEST_REWARD, CHECK_USER_HAS_QUEST_ITEM(_SIMPLE)) show the reward page of the
// group validateAndFixRewardGroup settles (AbstractQuestHandler.java:463-470): unset -> group 0 -> page 5; group 1 -> page 6; a group past the
// end -> the last one. SET_SUCCEED closes the window; an action outside the switch and the finish range (7 and 24 lie next to 8-23) is false.
TEST_F(AbstractQuestHandlerDialogTest, SendQuestEndDialogShowsTheRewardPageOfTheSettledGroup) {
	PlainHandler handler(1111);
	Npc& mires = npcOf(MIRES);
	const int32_t npc = mires.getObjectId();
	Ref<QuestState> qs = hold(*me, 1111, QuestStatus::REWARD);
	EXPECT_TRUE(handler.sendQuestEndDialog(*envOf(*me, 1111, DialogAction::SELECT_QUEST_REWARD, at(mires))));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(npc, 5, 1111)})) << "no group set: validateAndFixRewardGroup takes 0";
	EXPECT_EQ(qs->getRewardGroup(), std::optional<int32_t>(0));

	qs->setRewardGroup(1);
	for (int32_t action : {DialogAction::USE_OBJECT, DialogAction::QUEST_SELECT, DialogAction::SELECT_QUEST_REWARD,
			 DialogAction::CHECK_USER_HAS_QUEST_ITEM, DialogAction::CHECK_USER_HAS_QUEST_ITEM_SIMPLE}) {
		me->clearSent();
		EXPECT_TRUE(handler.sendQuestEndDialog(*envOf(*me, 1111, action, at(mires)))) << action;
		EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(npc, 6, 1111)})) << action;
	}

	qs->setRewardGroup(5);
	me->clearSent();
	EXPECT_TRUE(handler.sendQuestEndDialog(*envOf(*me, 1111, DialogAction::USE_OBJECT, at(mires))));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(npc, 6, 1111)})) << "group 5 of 2: the last";
	EXPECT_EQ(qs->getRewardGroup(), std::optional<int32_t>(1));

	me->clearSent();
	EXPECT_TRUE(handler.sendQuestEndDialog(*envOf(*me, 1111, DialogAction::SET_SUCCEED, at(mires))));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(npc, 0, 0)}));
	for (int32_t action : {DialogAction::QUEST_ACCEPT_1, DialogAction::SELECTED_QUEST_REWARD1 - 1, DialogAction::SELECTED_QUEST_NOREWARD + 1}) {
		me->clearSent();
		EXPECT_FALSE(handler.sendQuestEndDialog(*envOf(*me, 1111, action, at(mires)))) << action;
		EXPECT_TRUE(me->sent().empty()) << action;
	}
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(player().getInventory().getKinah(), 1000) << "nothing finished";
}

// A SELECTED_QUEST_REWARD1..NOREWARD action (8-23) finishes the quest (QuestService.finishQuest) and continues at the end npc
// (AbstractQuestHandler.java:420-457): 1101 completed at mires, where 1102, 1103 and 1104 start (the start set iterates 1104, 1102, 1103);
// 1104 and 1103 cannot start yet, 1102 can and names 1101 as <finished> - the engine hands the same env to 1102's handler with QUEST_SELECT and
// the pre-quest continuation flag, and its answer is the answer
TEST_F(AbstractQuestHandlerDialogTest, FinishingAtTheEndNpcOpensTheFollowUpQuest) {
	PlainHandler handler(1101);
	Npc& mires = npcOf(MIRES);
	probe(1102, {MIRES}, {MIRES});
	probe(1103, {MIRES}, {MIRES});
	probe(1104, {MIRES}, {MIRES});
	ASSERT_EQ(QuestEngine::getInstance().getQuestNpc(MIRES)->getOnQuestStart().snapshot(), (std::vector<int32_t>{1102, 1103, 1104}))
		<< "the runtime set keeps insertion order; Java's HashSet(0) of four buckets iterates 1104 (bucket 0), 1102 (2), 1103 (3)";

	for (int32_t action : {DialogAction::SELECTED_QUEST_NOREWARD, DialogAction::SELECTED_QUEST_REWARD1}) {
		player().setQuestStateList(gameserver::model::gameobjects::player::QuestStateList::create());
		me->f.commonData->setExp(0); // 1101 pays 130 exp: stay below level 2 (400)
		Ref<QuestState> qs = hold(*me, 1101, QuestStatus::REWARD);
		calls.clear();
		me->clearSent();
		Ref<QuestEnv> env = envOf(*me, 1101, action, at(mires));
		EXPECT_TRUE(handler.sendQuestEndDialog(*env)) << action;
		EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE) << action;
		ASSERT_EQ(calls.size(), 1u) << action;
		const DialogCall& call = calls[0];
		EXPECT_EQ(call.handlerQuestId, 1102);
		EXPECT_EQ(call.envQuestId, 1102);
		EXPECT_EQ(call.dialogActionId, DialogAction::QUEST_SELECT);
		EXPECT_TRUE(call.continuation);
		EXPECT_EQ(call.env, env.get()) << "the same env (:447-450)";
		EXPECT_EQ(call.targetObjectId, mires.getObjectId());
		EXPECT_TRUE(items::packetsOf(me->sent(), SM_DIALOG_WINDOW_OPCODE).empty()) << "the follow-up's handler answers, not a fallback page";
		EXPECT_EQ(items::packetsOf(me->sent(), SM_QUEST_ACTION_OPCODE), cp::exactly({questUpdate(1101, COMPLETE)}));
	}
}

// The follow-up's <finished> precondition is compared with the env's quest (AbstractQuestHandler.java:446), the one finishQuest just completed,
// not with the handler's: a handler of 1111 whose env finishes 1101 at mires opens 1102 (<finished 1101>)
TEST_F(AbstractQuestHandlerDialogTest, TheFollowUpFollowsTheEnvsQuest) {
	PlainHandler handler(1111);
	Npc& mires = npcOf(MIRES);
	probe(1102, {MIRES}, {MIRES});
	me->f.commonData->setExp(0); // 1101 pays 130 exp: stay below level 2 (400)
	hold(*me, 1101, QuestStatus::REWARD);
	hold(*me, 1111, QuestStatus::REWARD); // the handler's own guard (:416)
	EXPECT_TRUE(handler.sendQuestEndDialog(*envOf(*me, 1101, DialogAction::SELECTED_QUEST_NOREWARD, at(mires)))) << "the probe's answer";
	EXPECT_EQ(calls.size(), 1u) << "1102, the one quest registered at mires, opens";
}

// The follow-up is the first startable quest in the start set's iteration order (AbstractQuestHandler.java:438-456): QuestNpc's
// `new HashSet<>(0)` (QuestNpc.java:27), a table that grows from one bucket. 2236, 2237 and 2292 all need 24112 and pay something; registered in
// the order of their ids (the order of the XML registration), the set iterates 2236, 2292, 2237 in Java - four buckets: 2236 and 2292 share
// bucket 0 (tools/oracle m5d/javasrc.py hash_iteration_order(..., initial_capacity=0)). So 2236 opens, and once 2236 is done 2292, where the
// runtime set's insertion order would open 2237 (and a 16-bucket HashMap order 2292 first)
TEST_F(AbstractQuestHandlerDialogTest, TheFollowUpIsTheFirstStartableQuestInJavaHashSetOrder) {
	Quester& asmodian = *makeQuester(810102, "Scout", gameserver::model::Race::ASMODIANS, 14);
	holdItem(asmodian, 820101, items::KINAH, 1000);
	PlainHandler handler(24112);
	Npc& anmurnerk = npcOf(ANMURNERK);
	probe(2236, {ANMURNERK}, {ANMURNERK});
	probe(2237, {ANMURNERK}, {ANMURNERK});
	probe(2292, {ANMURNERK}, {ANMURNERK});
	ASSERT_EQ(QuestEngine::getInstance().getQuestNpc(ANMURNERK)->getOnQuestStart().snapshot(), (std::vector<int32_t>{2236, 2237, 2292}))
		<< "the runtime set keeps insertion order: the handler must not follow it";

	hold(asmodian, 24112, QuestStatus::REWARD);
	EXPECT_TRUE(handler.sendQuestEndDialog(*envOf(asmodian, 24112, DialogAction::SELECTED_QUEST_NOREWARD, at(anmurnerk))));
	ASSERT_EQ(calls.size(), 1u);
	EXPECT_EQ(calls[0].handlerQuestId, 2236);
	EXPECT_EQ(calls[0].dialogActionId, DialogAction::QUEST_SELECT);

	asmodian.player().setQuestStateList(gameserver::model::gameobjects::player::QuestStateList::create());
	hold(asmodian, 2236, QuestStatus::COMPLETE);
	hold(asmodian, 24112, QuestStatus::REWARD);
	calls.clear();
	asmodian.clearSent();
	EXPECT_TRUE(handler.sendQuestEndDialog(*envOf(asmodian, 24112, DialogAction::SELECTED_QUEST_NOREWARD, at(anmurnerk))));
	ASSERT_EQ(calls.size(), 1u);
	EXPECT_EQ(calls[0].handlerQuestId, 2292) << "2236 is not repeatable: the next in Java's order";
	EXPECT_EQ(std::ranges::count(asmodian.sent(),
				  asmodian.serializedFor(SM_SYSTEM_MESSAGE::STR_QUEST_ACQUIRE_ERROR_NONE_REPEATABLE(utils::ChatUtil::quest(2236)))),
		0) << "checkStartConditions refuses 2236 silently (warn false, :439)";
}

// The ids' high bits count too: HashMap.hash spreads h ^ (h >>> 16) before the bucket mask. 80677 is followed by 80673-80676 at sonatine (their
// start npc, which ends 80677 too, event.xml:403-407); four ids make eight buckets, where the spread puts 80673 (0x13B21) in bucket 0, 80675 in 2,
// 80674 in 3 and 80676 in 5 (hash_iteration_order: 80673, 80675, 80674, 80676). They repeat (max_repeat_count 255), so a quest in START is the
// one that cannot start: with 80673 in START, 80675 opens - not 80674, the next without the spread and in insertion order
TEST_F(AbstractQuestHandlerDialogTest, TheFollowUpOrderSpreadsTheHighBitsOfTheIds) {
	Quester& eventer = *makeQuester(810104, "Listener", gameserver::model::Race::ELYOS, 10);
	holdItem(eventer, 820103, items::KINAH, 1000);
	PlainHandler handler(80677);
	Npc& sonatine = npcOf(SONATINE);
	for (int32_t questId : {80673, 80674, 80675, 80676})
		probe(questId, {SONATINE}, {SONATINE});
	hold(eventer, 80677, QuestStatus::REWARD);
	hold(eventer, 80673, QuestStatus::START);
	EXPECT_TRUE(handler.sendQuestEndDialog(*envOf(eventer, 80677, DialogAction::SELECTED_QUEST_NOREWARD, at(sonatine))));
	ASSERT_EQ(calls.size(), 1u);
	EXPECT_EQ(calls[0].handlerQuestId, 80675);
}

// isAcceptableQuest (AbstractQuestHandler.java:476-484): a follow-up without rewards, extended rewards, bonus, quest drops or class rewards is not
// opened - 1195 needs 1194 and pays nothing (quest_data.xml:1663-1667) - but it still makes the npc offer a new quest: the selection page
TEST_F(AbstractQuestHandlerDialogTest, AFollowUpThatPaysNothingIsNotOpened) {
	Quester& elyos = *makeQuester(810103, "Hunter", gameserver::model::Race::ELYOS, 19);
	holdItem(elyos, 820102, items::KINAH, 1000);
	PlainHandler handler(1194);
	Npc& spatalos = npcOf(SPATALOS);
	probe(1195, {SPATALOS}, {SPATALOS});
	hold(elyos, 1194, QuestStatus::REWARD);
	elyos.clearSent();
	EXPECT_TRUE(handler.sendQuestEndDialog(*envOf(elyos, 1194, DialogAction::SELECTED_QUEST_NOREWARD, at(spatalos))));
	EXPECT_EQ(elyos.player().getQuestStateList()->getQuestState(1194)->getStatus(), QuestStatus::COMPLETE);
	EXPECT_TRUE(calls.empty()) << "1195 is startable but not acceptable";
	ASSERT_FALSE(elyos.sent().empty());
	EXPECT_EQ(elyos.sent().back(), dialogWindow(spatalos.getObjectId(), 10, 0));
}

// Without a follow-up (AbstractQuestHandler.java:425-436, 457): the selection page if the npc has a new quest or an active one - a START quest
// with a talk event here that does not start here, or a MISSION starting here at step 0 - else the closed window
TEST_F(AbstractQuestHandlerDialogTest, WithoutAFollowUpTheSelectionPageOrTheClosedWindow) {
	PlainHandler handler(1101);
	Npc& mires = npcOf(MIRES);
	const int32_t npc = mires.getObjectId();
	auto finish1101 = [&] {
		me->f.commonData->setExp(0); // 1101 pays 130 exp: stay below level 2 (400)
		hold(*me, 1101, QuestStatus::REWARD);
		me->clearSent();
		bool result = handler.sendQuestEndDialog(*envOf(*me, 1101, DialogAction::SELECTED_QUEST_NOREWARD, at(mires)));
		std::vector<std::vector<uint8_t>> sent = me->sent();
		EXPECT_EQ(items::packetsOf(sent, SM_DIALOG_WINDOW_OPCODE).size(), 1u);
		EXPECT_TRUE(result);
		return sent.empty() ? std::vector<uint8_t>() : sent.back();
	};
	auto fresh = [&] {
		player().setQuestStateList(gameserver::model::gameobjects::player::QuestStateList::create());
		QuestEngine::getInstance().clear();
	};

	EXPECT_EQ(finish1101(), dialogWindow(npc, 0, 0)) << "nothing registered at mires";

	fresh();
	probe(1111, {}, {MIRES});
	hold(*me, 1111, QuestStatus::START);
	EXPECT_EQ(finish1101(), dialogWindow(npc, 10, 0)) << "1111 active and continued here (a talk event, not a start)";

	fresh();
	probe(1111, {MIRES}, {MIRES});
	hold(*me, 1111, QuestStatus::START);
	EXPECT_EQ(finish1101(), dialogWindow(npc, 0, 0)) << "1111 starts here: not counted as active (not a mission); not startable either";

	for (QuestStatus status : {QuestStatus::COMPLETE, QuestStatus::LOCKED}) {
		fresh();
		probe(1111, {}, {MIRES});
		hold(*me, 1111, status);
		EXPECT_EQ(finish1101(), dialogWindow(npc, 0, 0)) << static_cast<int>(status) << ": only a quest in START is active (:431)";
	}

	fresh();
	probe(1001, {MIRES}, {MIRES});
	hold(*me, 1001, QuestStatus::START);
	EXPECT_EQ(finish1101(), dialogWindow(npc, 10, 0)) << "a MISSION starting here, at step 0";

	fresh();
	probe(1001, {MIRES}, {MIRES});
	hold(*me, 1001, QuestStatus::START, 1);
	EXPECT_EQ(finish1101(), dialogWindow(npc, 0, 0)) << "the MISSION at step 1";

	fresh();
	probe(1101, {MIRES}, {MIRES});
	player().setQuestStateList(gameserver::model::gameobjects::player::QuestStateList::create());
	me->f.commonData->setExp(0);
	hold(*me, 1101, QuestStatus::REWARD);
	me->clearSent();
	// 1101 itself starts here: checkStartConditions answers false for the completed, non-repeatable quest, so no new quest
	EXPECT_TRUE(handler.sendQuestEndDialog(*envOf(*me, 1101, DialogAction::SELECTED_QUEST_NOREWARD, at(mires))));
	EXPECT_EQ(me->sent().back(), dialogWindow(npc, 0, 0));
	EXPECT_TRUE(calls.empty());
}

// Another quest in REWARD with a talk event at the npc comes first (AbstractQuestHandler.java:425-430): the env is set to it and USE_OBJECT, and
// a new env of the npc, the player, that quest and USE_OBJECT goes to its handler, whose answer is the answer - even with a follow-up waiting
TEST_F(AbstractQuestHandlerDialogTest, AnotherQuestInRewardAtTheNpcShowsItsRewardDialogFirst) {
	PlainHandler handler(1101);
	Npc& mires = npcOf(MIRES);
	probe(1102, {MIRES}, {MIRES});
	probe(1111, {}, {MIRES}, false);
	hold(*me, 1101, QuestStatus::REWARD);
	hold(*me, 1111, QuestStatus::REWARD);
	Ref<QuestEnv> env = envOf(*me, 1101, DialogAction::SELECTED_QUEST_NOREWARD, at(mires));
	EXPECT_FALSE(handler.sendQuestEndDialog(*env)) << "1111's handler answered false";
	EXPECT_EQ(player().getQuestStateList()->getQuestState(1101)->getStatus(), QuestStatus::COMPLETE);
	ASSERT_EQ(calls.size(), 1u);
	const DialogCall& call = calls[0];
	EXPECT_EQ(call.handlerQuestId, 1111);
	EXPECT_EQ(call.envQuestId, 1111);
	EXPECT_EQ(call.dialogActionId, DialogAction::USE_OBJECT);
	EXPECT_FALSE(call.continuation);
	EXPECT_NE(call.env, env.get()) << "a new QuestEnv (:430)";
	EXPECT_EQ(call.targetObjectId, mires.getObjectId());
	EXPECT_EQ(env->getQuestId(), 1111) << ":428";
	EXPECT_EQ(env->getDialogActionId(), DialogAction::USE_OBJECT) << ":429";
}

// sendQuestEndDialog(env, int[]) (AbstractQuestHandler.java:401-408) takes every listed item first - with the delete type of the quest's status
// (ItemDeleteType.fromQuestStatus: REWARD is DEFAULT 0, START QUEST_START 0x34) - then answers as sendQuestEndDialog(env), false outside REWARD;
// without a quest state the status is null, so the delete type comes from DEC_ITEM_USE (USE, 0x17; Storage.java:141), and the answer is false
TEST_F(AbstractQuestHandlerDialogTest, SendQuestEndDialogWithItemsTakesThemAll) {
	PlainHandler handler(1111);
	Npc& mires = npcOf(MIRES);
	holdItem(*me, 820011, SYLPHEN_WINGS, 3);
	holdItem(*me, 820012, NYMPHS_DRESS, 1);
	hold(*me, 1111, QuestStatus::REWARD);
	me->clearSent();
	const std::array<int32_t, 2> questItems{SYLPHEN_WINGS, NYMPHS_DRESS};
	EXPECT_TRUE(handler.sendQuestEndDialog(*envOf(*me, 1111, DialogAction::SELECT_QUEST_REWARD, at(mires)), questItems));
	EXPECT_EQ(held(*me, SYLPHEN_WINGS), 0);
	EXPECT_EQ(held(*me, NYMPHS_DRESS), 0);
	std::vector<std::vector<uint8_t>> sent = me->sent();
	EXPECT_EQ(items::packetsOf(sent, items::SM_DELETE_ITEM_OPCODE), cp::exactly({items::deleteItem(820011, 0), items::deleteItem(820012, 0)}));
	ASSERT_FALSE(sent.empty());
	EXPECT_EQ(sent.back(), dialogWindow(mires.getObjectId(), 5, 1111));

	player().setQuestStateList(gameserver::model::gameobjects::player::QuestStateList::create());
	holdItem(*me, 820013, SYLPHEN_WINGS, 2);
	me->clearSent();
	EXPECT_FALSE(handler.sendQuestEndDialog(*envOf(*me, 1111, DialogAction::SELECT_QUEST_REWARD, at(mires)), questItems));
	EXPECT_EQ(held(*me, SYLPHEN_WINGS), 0);
	EXPECT_EQ(items::packetsOf(me->sent(), items::SM_DELETE_ITEM_OPCODE), cp::exactly({items::deleteItem(820013, 0x17)}));
	EXPECT_TRUE(items::packetsOf(me->sent(), SM_DIALOG_WINDOW_OPCODE).empty());

	hold(*me, 1111, QuestStatus::START);
	holdItem(*me, 820014, SYLPHEN_WINGS, 1);
	me->clearSent();
	EXPECT_FALSE(handler.sendQuestEndDialog(*envOf(*me, 1111, DialogAction::SELECT_QUEST_REWARD, at(mires)), questItems)) << "START: the guard";
	EXPECT_EQ(held(*me, SYLPHEN_WINGS), 0) << "the items go before the guard";
	EXPECT_EQ(items::packetsOf(me->sent(), items::SM_DELETE_ITEM_OPCODE), cp::exactly({items::deleteItem(820014, 0x34)})) << "QUEST_START";
	EXPECT_TRUE(items::packetsOf(me->sent(), SM_DIALOG_WINDOW_OPCODE).empty());
}

// sendQuestRewardDialog (AbstractQuestHandler.java:1151-1164): in REWARD at the reward npc, USE_OBJECT shows the report page if one is given,
// every other action (and USE_OBJECT without a page) goes to sendQuestEndDialog; another npc or another status answers false; no quest state is
// Java's NullPointerException at qs.getStatus()
TEST_F(AbstractQuestHandlerDialogTest, SendQuestRewardDialogReportsAtTheRewardNpcOnly) {
	PlainHandler handler(1111);
	Npc& mires = npcOf(MIRES);
	Npc& sack = npcOf(GRAIN_SACK);
	const int32_t npc = mires.getObjectId();
	EXPECT_THROW(handler.sendQuestRewardDialog(*envOf(*me, 1111, DialogAction::USE_OBJECT, at(mires)), MIRES, 2375), runtime::NullPointerException);

	Ref<QuestState> qs = hold(*me, 1111, QuestStatus::REWARD);
	me->clearSent();
	EXPECT_TRUE(handler.sendQuestRewardDialog(*envOf(*me, 1111, DialogAction::USE_OBJECT, at(mires)), MIRES, 2375));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(npc, 2375, 1111)}));
	me->clearSent();
	EXPECT_TRUE(handler.sendQuestRewardDialog(*envOf(*me, 1111, DialogAction::USE_OBJECT, at(mires)), MIRES, 0));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(npc, 5, 1111)})) << "no report page: the reward page";
	me->clearSent();
	EXPECT_TRUE(handler.sendQuestRewardDialog(*envOf(*me, 1111, DialogAction::QUEST_SELECT, at(mires)), MIRES, 2375));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(npc, 5, 1111)}));
	me->clearSent();
	EXPECT_FALSE(handler.sendQuestRewardDialog(*envOf(*me, 1111, DialogAction::USE_OBJECT, at(sack)), MIRES, 2375)) << "another npc";
	EXPECT_TRUE(me->sent().empty());
	qs->setStatus(QuestStatus::START);
	EXPECT_FALSE(handler.sendQuestRewardDialog(*envOf(*me, 1111, DialogAction::USE_OBJECT, at(mires)), MIRES, 2375));
	EXPECT_TRUE(me->sent().empty());
}

// sendQuestNoneDialog (AbstractQuestHandler.java:1166-1225): for a quest that can start (no state, or isStartable) at its start npc, QUEST_SELECT
// shows the page (1011 by default), every other action is sendQuestStartDialog; with an item, only QUEST_ACCEPT_1 gives it before the start
// dialog. A quest in progress or another npc answers false.
TEST_F(AbstractQuestHandlerDialogTest, SendQuestNoneDialogOffersTheQuestAtItsStartNpc) {
	PlainHandler handler(1101);
	Npc& mires = npcOf(MIRES);
	Npc& sack = npcOf(GRAIN_SACK);
	const int32_t npc = mires.getObjectId();
	auto reset = [&] {
		player().setQuestStateList(gameserver::model::gameobjects::player::QuestStateList::create());
		me->clearSent();
	};

	reset();
	EXPECT_TRUE(handler.sendQuestNoneDialog(*envOf(*me, 1101, DialogAction::QUEST_SELECT, at(mires)), MIRES));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(npc, 1011, 1101)}));
	reset();
	EXPECT_TRUE(handler.sendQuestNoneDialog(*envOf(*me, 1101, DialogAction::QUEST_SELECT, at(mires)), MIRES, 4762));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(npc, 4762, 1101)}));
	reset();
	EXPECT_TRUE(handler.sendQuestNoneDialog(*envOf(*me, 1101, DialogAction::QUEST_SELECT, at(mires)), MIRES, NYMPHS_DRESS, 1));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(npc, 1011, 1101)})) << "the item overload's default page";
	EXPECT_EQ(held(*me, NYMPHS_DRESS), 0);
	reset();
	EXPECT_TRUE(handler.sendQuestNoneDialog(*envOf(*me, 1101, DialogAction::QUEST_ACCEPT_1, at(mires)), MIRES));
	EXPECT_EQ(me->sent(), cp::exactly({questAction(1, 1101, START), noNearbyQuests(), dialogWindow(npc, 1003, 1101)}));

	EXPECT_FALSE(handler.sendQuestNoneDialog(*envOf(*me, 1101, DialogAction::QUEST_SELECT, at(mires)), MIRES)) << "in START: not startable";
	reset();
	EXPECT_FALSE(handler.sendQuestNoneDialog(*envOf(*me, 1101, DialogAction::QUEST_SELECT, at(sack)), MIRES)) << "another npc";
	EXPECT_TRUE(me->sent().empty());

	reset();
	EXPECT_TRUE(handler.sendQuestNoneDialog(*envOf(*me, 1101, DialogAction::QUEST_ACCEPT, at(mires)), MIRES, 1011, NYMPHS_DRESS, 1));
	EXPECT_EQ(held(*me, NYMPHS_DRESS), 0) << "QUEST_ACCEPT starts without the item";
	EXPECT_EQ(me->sent().back(), dialogWindow(npc, 1003, 1101));
	reset();
	EXPECT_TRUE(handler.sendQuestNoneDialog(*envOf(*me, 1101, DialogAction::QUEST_ACCEPT_1, at(mires)), MIRES, NYMPHS_DRESS, 1));
	EXPECT_EQ(held(*me, NYMPHS_DRESS), 1) << "QUEST_ACCEPT_1 gives it first";
	std::vector<std::vector<uint8_t>> sent = me->sent();
	ASSERT_GE(sent.size(), 4u);
	EXPECT_EQ(items::javaOpcodeOf(sent.front()), items::SM_INVENTORY_ADD_ITEM_OPCODE) << "before the start";
	EXPECT_EQ(sent.back(), dialogWindow(npc, 1003, 1101));
	reset();
	EXPECT_TRUE(handler.sendQuestNoneDialog(*envOf(*me, 1101, DialogAction::QUEST_ACCEPT_1, at(mires)), MIRES, 0, 1));
	EXPECT_EQ(me->sent(), cp::exactly({questAction(1, 1101, START), noNearbyQuests(), dialogWindow(npc, 1003, 1101)})) << "no item id";

	// a held state that can start again (QuestState.isStartable): 80673 repeats (max_repeat_count 255) - by both overloads; 1101 done cannot
	PlainHandler repeatable(80673);
	Npc& sonatine = npcOf(SONATINE);
	reset();
	hold(*me, 80673, QuestStatus::COMPLETE);
	EXPECT_TRUE(repeatable.sendQuestNoneDialog(*envOf(*me, 80673, DialogAction::QUEST_SELECT, at(sonatine)), SONATINE));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(sonatine.getObjectId(), 1011, 80673)})) << "80673 done once, 255 allowed";
	me->clearSent();
	EXPECT_TRUE(repeatable.sendQuestNoneDialog(*envOf(*me, 80673, DialogAction::QUEST_SELECT, at(sonatine)), SONATINE, NYMPHS_DRESS, 1));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(sonatine.getObjectId(), 1011, 80673)})) << "the item overload";
	reset();
	hold(*me, 1101, QuestStatus::COMPLETE);
	EXPECT_FALSE(handler.sendQuestNoneDialog(*envOf(*me, 1101, DialogAction::QUEST_SELECT, at(mires)), MIRES)) << "1101 done, 1 allowed";
	EXPECT_FALSE(handler.sendQuestNoneDialog(*envOf(*me, 1101, DialogAction::QUEST_SELECT, at(mires)), MIRES, NYMPHS_DRESS, 1));
	EXPECT_TRUE(me->sent().empty());
}

// sendItemCollectingStartDialog (AbstractQuestHandler.java:1227-1236): QUEST_ACCEPT_1 starts the quest and shows the selection page whatever the
// start answered; QUEST_REFUSE_1 only shows it; every other action is false
TEST_F(AbstractQuestHandlerDialogTest, SendItemCollectingStartDialogReturnsToTheSelectionPage) {
	PlainHandler handler(1101);
	Npc& mires = npcOf(MIRES);
	const int32_t npc = mires.getObjectId();
	EXPECT_TRUE(handler.sendItemCollectingStartDialog(*envOf(*me, 1101, DialogAction::QUEST_REFUSE_1, at(mires))));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(npc, 10, 0)}));
	EXPECT_FALSE(player().getQuestStateList()->hasQuest(1101));
	me->clearSent();
	EXPECT_TRUE(handler.sendItemCollectingStartDialog(*envOf(*me, 1101, DialogAction::QUEST_ACCEPT_1, at(mires))));
	EXPECT_EQ(me->sent(), cp::exactly({questAction(1, 1101, START), noNearbyQuests(), dialogWindow(npc, 10, 0)}));
	me->clearSent();
	EXPECT_TRUE(handler.sendItemCollectingStartDialog(*envOf(*me, 1101, DialogAction::QUEST_ACCEPT_1, at(mires))));
	EXPECT_EQ(me->sent(), cp::exactly({me->serializedFor(SM_SYSTEM_MESSAGE::STR_QUEST_ACQUIRE_ERROR_WORKING_QUEST()), dialogWindow(npc, 10, 0)}))
		<< "a refused start still returns to the selection page";
	me->clearSent();
	for (int32_t action : {DialogAction::QUEST_ACCEPT, DialogAction::QUEST_REFUSE_2, DialogAction::QUEST_SELECT}) {
		EXPECT_FALSE(handler.sendItemCollectingStartDialog(*envOf(*me, 1101, action, at(mires)))) << action;
		EXPECT_TRUE(me->sent().empty()) << action;
	}
}

// defaultCloseDialog (AbstractQuestHandler.java:486-529): at the step, the item is given, the other taken (with the quest's status), the step
// changed, and the window closed - or, for the same npc, sendQuestEndDialog; another step answers false. The QuestItems overload passes the
// item's id and count; a null one, and a player without the quest state, are Java's NullPointerException.
TEST_F(AbstractQuestHandlerDialogTest, DefaultCloseDialogMovesTheStepAndClosesTheWindow) {
	PlainHandler handler(1111);
	Npc& mires = npcOf(MIRES);
	const int32_t npc = mires.getObjectId();
	auto env = [&] { return envOf(*me, 1111, DialogAction::CHECK_USER_HAS_QUEST_ITEM, at(mires)); };
	EXPECT_THROW(handler.defaultCloseDialog(*env(), 0, 1), runtime::NullPointerException);

	holdItem(*me, 820021, SYLPHEN_WINGS, 3);
	Ref<QuestState> qs = hold(*me, 1111, QuestStatus::START);
	me->clearSent();
	EXPECT_TRUE(handler.defaultCloseDialog(*env(), 0, 1));
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1111, START, 1), dialogWindow(npc, 0, 0)}));
	me->clearSent();
	EXPECT_FALSE(handler.defaultCloseDialog(*env(), 0, 2)) << "the step is 1";
	EXPECT_TRUE(me->sent().empty());

	EXPECT_TRUE(handler.defaultCloseDialog(*env(), 1, 2, NYMPHS_DRESS, 1));
	EXPECT_EQ(held(*me, NYMPHS_DRESS), 1);
	EXPECT_EQ(qs->getQuestVarById(0), 2);

	me->clearSent();
	EXPECT_TRUE(handler.defaultCloseDialog(*env(), 2, 3, 0, 0, SYLPHEN_WINGS, 3));
	EXPECT_EQ(held(*me, SYLPHEN_WINGS), 0);
	EXPECT_EQ(items::packetsOf(me->sent(), items::SM_DELETE_ITEM_OPCODE), cp::exactly({items::deleteItem(820021, 0x34)})) << "QUEST_START";
	EXPECT_EQ(qs->getQuestVarById(0), 3);

	const gameserver::model::templates::quest::QuestItems& workItem =
		dataholders::DataManager::QUEST_DATA->getQuestById(1114)->getQuestWorkItems()->getQuestWorkItem().at(0);
	player().getInventory().decreaseByItemId(NYMPHS_DRESS, 1);
	EXPECT_TRUE(handler.defaultCloseDialog(*env(), 3, 4, false, false, &workItem));
	EXPECT_EQ(held(*me, NYMPHS_DRESS), 1);
	EXPECT_THROW(handler.defaultCloseDialog(*env(), 4, 5, false, false, nullptr), runtime::NullPointerException);

	me->clearSent();
	EXPECT_TRUE(handler.defaultCloseDialog(*env(), 4, 4, true, true)) << "same npc: the end dialog";
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1111, REWARD, 4), noNearbyQuests(), dialogWindow(npc, 5, 1111)}));

	holdItem(*me, 820022, SYLPHEN_WINGS, 2);
	me->clearSent();
	EXPECT_TRUE(handler.defaultCloseDialog(*env(), 4, 5, 0, 0, SYLPHEN_WINGS, 2)) << "in REWARD";
	EXPECT_EQ(items::packetsOf(me->sent(), items::SM_DELETE_ITEM_OPCODE), cp::exactly({items::deleteItem(820022, 0)})) << "REWARD: DEFAULT";
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::test
