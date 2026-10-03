// P5-06c, M5d T-01a, T-02 and T-04 (m5d-plan.md §7): the item_collecting kind - ItemCollectingData.register_ building ItemCollecting
// (ItemCollecting.java), driven through QuestEngine on in-world players (QuestTemplateTestSupport.h):
// - 1103 "Grain Thieves" (poeta.xml:114; quest_data.xml:905-914): started and reported at mires, 3 Kerub Grain Sacks collected from the
//   quest object 700105 (its <quest_drop>, which makes 700105 an action item of the quest), rewarded 290 kinah and 590 exp;
// - 15002 "Cold Hands, Warm Luciferin" (cygnea.xml:151), data-driven: the pages 4762, 1011, 10000 and 10001 in place of 1011, 2375, 5 and
//   2716.
// Pages, statuses and vars as `oracle.py m5d-quest --quest 1103` (and 15002) prints them.

#include "QuestTemplateTestSupport.h"

#include <array>
#include <cstdint>
#include <vector>

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/questEngine/model/QuestActionType.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test::templates {
namespace {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::gameobjects::Npc;

class ItemCollectingTemplateTest : public QuestTemplateTest {
protected:
	/** 1103 held in START with `sacks` Kerub Grain Sacks in the cube (one item each: max_stack_count 20, but loaded rows) */
	Ref<QuestState> startedWithSacks(int32_t sacks) {
		player().setQuestStateList(gameserver::model::gameobjects::player::QuestStateList::create());
		while (held(*me, KERUB_GRAIN_SACK) > 0)
			player().getInventory().decreaseByItemId(KERUB_GRAIN_SACK, held(*me, KERUB_GRAIN_SACK));
		if (sacks > 0)
			holdItem(*me, nextItemObjId++, KERUB_GRAIN_SACK, sacks);
		return hold(*me, 1103, QuestStatus::START);
	}

	int32_t nextItemObjId = 820201;
};

// ItemCollectingData.register (ItemCollectingData.java:47-50) and ItemCollecting.register (ItemCollecting.java:65-86): mires starts and talks,
// and 700105, whose <quest_drop> the handler's action items hold (AbstractQuestHandler.loadActionItems: npc ids 7xxxxx), is a talk npc; its
// quest_use_item AI lets QuestEngine.onCanAct ask 1103 about it (registerCanAct) - true in START while the sacks are missing
TEST_F(ItemCollectingTemplateTest, RegisterMakesTheQuestObjectsTalkNpcsAndCanActTargets) {
	registerXml(1103);
	EXPECT_EQ(startQuests(MIRES), (std::vector<int32_t>{1103}));
	EXPECT_EQ(talkQuests(MIRES), (std::vector<int32_t>{1103}));
	EXPECT_EQ(talkQuests(GRAIN_SACK), (std::vector<int32_t>{1103}));
	EXPECT_EQ(startQuests(GRAIN_SACK), (std::vector<int32_t>{}));

	Npc& sack = npcOf(GRAIN_SACK);
	// (without the quest AbstractQuestHandler.onCanAct answers false, AbstractQuestHandler.java:227-228: the handler base's, not asserted
	// here)
	startedWithSacks(0);
	EXPECT_TRUE(QuestEngine::getInstance().onCanAct(*envOf(*me, 0, 0, at(sack)), GRAIN_SACK, model::QuestActionType::ACTION_ITEM_USE, {}));
}

// 1103 (ItemCollecting.java:88-165): mires offers it (1011) and starts it; in START mires asks for the sacks (2375); CHECK_USER_HAS_QUEST_ITEM
// with 2 of 3 shows 2716 and keeps START, with 3 takes them, sets REWARD and shows page 5; the quest object answers true in START (looting)
// and nothing else; in REWARD mires reports and finishes it (290 kinah, 590 exp; a level-5 quester, so the exp does not level him up -
// the fixture's players have no npc factions for PlayerController.onLevelChange)
TEST_F(ItemCollectingTemplateTest, Quest1103FromTheAcceptThroughTheSacksToTheReward) {
	registerXml(1103);
	Quester* q = makeQuester(810501, "Harvester", gameserver::model::Race::ELYOS, 5);
	holdItem(*q, 820300, items::KINAH, 1000);
	Npc& mires = npcOf(MIRES);
	Npc& sack = npcOf(GRAIN_SACK);
	const int32_t atMires = mires.getObjectId();
	hold(*q, 1102, QuestStatus::COMPLETE); // 1103's <finished quest_id="1102"/>

	EXPECT_FALSE(talk(*q, 1103, DialogAction::QUEST_SELECT, sack)) << "no quest: the object is no start npc";
	EXPECT_TRUE(talk(*q, 1103, DialogAction::QUEST_SELECT, mires));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atMires, 1011, 1103)}));
	EXPECT_TRUE(talk(*q, 1103, DialogAction::QUEST_ACCEPT_1, mires));
	Ptr<QuestState> qs = q->player().getQuestStateList()->getQuestState(1103);
	ASSERT_TRUE(qs);
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_EQ(q->sent(), cp::exactly({questAction(1, 1103, START), noNearbyQuests(), dialogWindow(atMires, 1003, 1103)}));

	EXPECT_TRUE(talk(*q, 1103, DialogAction::USE_OBJECT, sack)) << "looting";
	EXPECT_TRUE(q->sent().empty());
	EXPECT_TRUE(talk(*q, 1103, DialogAction::QUEST_SELECT, mires));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atMires, 2375, 1103)}));

	holdItem(*q, 820301, KERUB_GRAIN_SACK, 2);
	EXPECT_TRUE(talk(*q, 1103, DialogAction::CHECK_USER_HAS_QUEST_ITEM, mires));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atMires, 2716, 1103)}));
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	// (the sacks stay: collectItemCheck removes nothing when it fails - QuestService's, not asserted here)

	holdItem(*q, 820302, KERUB_GRAIN_SACK, 1);
	EXPECT_TRUE(talk(*q, 1103, DialogAction::CHECK_USER_HAS_QUEST_ITEM, mires));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(held(*q, KERUB_GRAIN_SACK), 0);
	EXPECT_EQ(q->sent().back(), dialogWindow(atMires, 5, 1103));
	EXPECT_FALSE(talk(*q, 1103, DialogAction::USE_OBJECT, sack)) << "REWARD: the object is no end npc";

	EXPECT_TRUE(talk(*q, 1103, DialogAction::USE_OBJECT, mires));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atMires, 5, 1103)}));
	const int64_t exp = q->f.commonData->getExp();
	EXPECT_TRUE(talk(*q, 1103, DialogAction::SELECTED_QUEST_NOREWARD, mires));
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(q->player().getInventory().getKinah(), 1000 + 290);
	EXPECT_EQ(q->f.commonData->getExp(), exp + 590);
}

// The other START actions at the end npc (ItemCollecting.java:134-149): CHECK_USER_HAS_QUEST_ITEM_SIMPLE and SETPRO1-4 check the items the
// simple way - the pages 5, 5, 6, 7 and 8 with them, the closed window without; FINISH_DIALOG shows the selection page; SET_SUCCEED sets
// REWARD and closes the window, whatever is held
TEST_F(ItemCollectingTemplateTest, TheSimpleChecksAndTheOtherReportActions) {
	registerXml(1103);
	Npc& mires = npcOf(MIRES);
	const int32_t atMires = mires.getObjectId();
	struct Row {
		int32_t action;
		int32_t page;
	};
	const std::array<Row, 5> rows{{{DialogAction::CHECK_USER_HAS_QUEST_ITEM_SIMPLE, 5},
		{DialogAction::SETPRO1, 5},
		{DialogAction::SETPRO2, 6},
		{DialogAction::SETPRO3, 7},
		{DialogAction::SETPRO4, 8}}};
	for (const Row& row : rows) {
		Ref<QuestState> qs = startedWithSacks(3);
		EXPECT_TRUE(talk(*me, 1103, row.action, mires)) << row.action;
		EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD) << row.action;
		EXPECT_EQ(held(*me, KERUB_GRAIN_SACK), 0) << row.action;
		EXPECT_EQ(me->sent().back(), dialogWindow(atMires, row.page, 1103)) << row.action;

		qs = startedWithSacks(1);
		EXPECT_TRUE(talk(*me, 1103, row.action, mires)) << row.action;
		EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(atMires, 0, 0)})) << row.action;
		// (still START: the handler base's checkQuestItemsSimple, not asserted here)
	}

	Ref<QuestState> qs = startedWithSacks(0);
	EXPECT_TRUE(talk(*me, 1103, DialogAction::FINISH_DIALOG, mires));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(atMires, 10, 0)}));
	EXPECT_TRUE(talk(*me, 1103, DialogAction::SET_SUCCEED, mires));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1103, REWARD), noNearbyQuests(), dialogWindow(atMires, 0, 0)}));
	EXPECT_FALSE(talk(*me, 1103, DialogAction::SETPRO5, mires)) << "REWARD answers through sendQuestEndDialog only";
}

// The start actions (ItemCollecting.java:98-114): SETPRO1 starts the quest and closes the window; SELECT1_1 shows page 1012 (1103 has no
// movie); an action outside the switch goes to AbstractQuestHandler.onDialogEvent (QUEST_REFUSE_1: page 1004)
TEST_F(ItemCollectingTemplateTest, TheStartActionsAtTheStartNpc) {
	registerXml(1103);
	Npc& mires = npcOf(MIRES);
	const int32_t atMires = mires.getObjectId();
	hold(*me, 1102, QuestStatus::COMPLETE);
	EXPECT_TRUE(talk(*me, 1103, DialogAction::SELECT1_1, mires));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(atMires, 1012, 1103)}));
	EXPECT_TRUE(talk(*me, 1103, DialogAction::QUEST_REFUSE_1, mires));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(atMires, 1004, 1103)}));
	EXPECT_FALSE(player().getQuestStateList()->hasQuest(1103));
	EXPECT_TRUE(talk(*me, 1103, DialogAction::SETPRO1, mires));
	ASSERT_TRUE(player().getQuestStateList()->hasQuest(1103));
	EXPECT_EQ(player().getQuestStateList()->getQuestState(1103)->getStatus(), QuestStatus::START);
	EXPECT_EQ(me->sent(), cp::exactly({questAction(1, 1103, START), noNearbyQuests(), dialogWindow(atMires, 0, 0)}));
}

// A data-driven item_collecting quest (15002; ItemCollecting.java:100, 129-132): 4762 to offer it, 1011 to ask for the items, 10001 without
// them and 10000 with the 7 Vespine's Luciferin
TEST_F(ItemCollectingTemplateTest, ADataDrivenQuestUsesTheNewPages) {
	registerXml(15002);
	Npc& nubes = npcOf(NUBES);
	const int32_t atNubes = nubes.getObjectId();
	EXPECT_TRUE(talk(*me, 15002, DialogAction::QUEST_SELECT, nubes));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(atNubes, 4762, 15002)}));
	Ref<QuestState> qs = hold(*me, 15002, QuestStatus::START);
	EXPECT_TRUE(talk(*me, 15002, DialogAction::QUEST_SELECT, nubes));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(atNubes, 1011, 15002)}));
	EXPECT_TRUE(talk(*me, 15002, DialogAction::CHECK_USER_HAS_QUEST_ITEM, nubes));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(atNubes, 10001, 15002)}));
	holdItem(*me, 820401, VESPINE_LUCIFERIN, 7);
	EXPECT_TRUE(talk(*me, 15002, DialogAction::CHECK_USER_HAS_QUEST_ITEM, nubes));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(me->sent().back(), dialogWindow(atNubes, 10000, 15002));
}

// The script's dialog ids (ItemCollecting.java:100, 129): 3018 is offered with page 4762 at its wanted poster and asks for the item at crios
// with 1011, in place of 1011 and 2375
TEST_F(ItemCollectingTemplateTest, TheScriptsDialogIdsReplaceTheDefaultPages) {
	registerXml(3018);
	Npc& poster = npcOf(WANTED_POSTER);
	EXPECT_TRUE(talk(*me, 3018, DialogAction::QUEST_SELECT, poster));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(poster.getObjectId(), 4762, 3018)}));
	Npc& crios = npcOf(CRIOS);
	hold(*me, 3018, QuestStatus::START);
	EXPECT_TRUE(talk(*me, 3018, DialogAction::QUEST_SELECT, crios));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(crios.getObjectId(), 1011, 3018)}));
}

// next_npc_id (ItemCollecting.java:118-125): 3340's next npc laigas, also its end npc, takes step 0 - 1352, and SETPRO1 moves var 0 to 1
// and closes the window (defaultCloseDialog); at var 1 laigas is the end npc and asks for the item (2375). The report runs at var 1, where
// 3340 collects (its <quest_drop> has collecting_step="1"): the item checks take var 0 as their step (:133, 143), so CHECK_USER_HAS_QUEST_ITEM
// shows 2716 without the Destruction Orders and, with them, takes them, sets REWARD (var 0 kept at 1) and shows page 5; SETPRO1 does the same
// through the simple check (oracle.py m5d-quest --quest 3340)
TEST_F(ItemCollectingTemplateTest, TheNextNpcTakesStepZeroBeforeTheReport) {
	registerXml(3340);
	Npc& laigas = npcOf(LAIGAS);
	const int32_t atLaigas = laigas.getObjectId();
	Ref<QuestState> qs = hold(*me, 3340, QuestStatus::START);
	EXPECT_TRUE(talk(*me, 3340, DialogAction::QUEST_SELECT, laigas));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(atLaigas, 1352, 3340)}));
	EXPECT_TRUE(talk(*me, 3340, DialogAction::SETPRO1, laigas));
	EXPECT_EQ(qs->getQuestVarById(0), 1);
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(3340, START, 1), dialogWindow(atLaigas, 0, 0)}));
	EXPECT_TRUE(talk(*me, 3340, DialogAction::QUEST_SELECT, laigas));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(atLaigas, 2375, 3340)}));

	EXPECT_TRUE(talk(*me, 3340, DialogAction::CHECK_USER_HAS_QUEST_ITEM, laigas));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(atLaigas, 2716, 3340)})) << "no Destruction Orders";
	holdItem(*me, 820304, DESTRUCTION_ORDERS, 1);
	EXPECT_TRUE(talk(*me, 3340, DialogAction::CHECK_USER_HAS_QUEST_ITEM, laigas));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(qs->getQuestVarById(0), 1);
	EXPECT_EQ(held(*me, DESTRUCTION_ORDERS), 0);
	EXPECT_EQ(sentOf(*me, SM_DIALOG_WINDOW_OPCODE), cp::exactly({dialogWindow(atLaigas, 5, 3340)}));

	qs->setStatus(QuestStatus::START);
	holdItem(*me, 820305, DESTRUCTION_ORDERS, 1);
	EXPECT_TRUE(talk(*me, 3340, DialogAction::SETPRO1, laigas));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(held(*me, DESTRUCTION_ORDERS), 0);
	EXPECT_EQ(sentOf(*me, SM_DIALOG_WINDOW_OPCODE), cp::exactly({dialogWindow(atLaigas, 5, 3340)}));
}

// The work item (ItemCollecting.java:58-62, 109-112, 154-161): 1137's accept gives it; in REWARD the end npc takes every one held, on any
// action, before sendQuestEndDialog
TEST_F(ItemCollectingTemplateTest, TheWorkItemIsGivenOnTheAcceptAndTakenInReward) {
	registerXml(1137);
	Quester* q = makeQuester(810503, "Fossil", gameserver::model::Race::ELYOS, 10);
	Npc& spiros = npcOf(SPIROS);
	EXPECT_TRUE(talk(*q, 1137, DialogAction::QUEST_ACCEPT_1, spiros));
	Ptr<QuestState> qs = q->player().getQuestStateList()->getQuestState(1137);
	ASSERT_TRUE(qs);
	EXPECT_EQ(held(*q, BROKEN_FOSSIL), 1);
	qs->setStatus(QuestStatus::REWARD);
	holdItem(*q, 820303, BROKEN_FOSSIL, 1);
	EXPECT_TRUE(talk(*q, 1137, DialogAction::USE_OBJECT, spiros));
	EXPECT_EQ(held(*q, BROKEN_FOSSIL), 0) << "both taken";
	EXPECT_EQ(q->sent().back(), dialogWindow(spiros.getObjectId(), 5, 1137));
}

// start_zone (ItemCollecting.java:84-85, 167-178): entering the zone starts 18739 for an Elyos of level 60; entering again with the quest
// held changes nothing
TEST_F(ItemCollectingTemplateTest, EnteringTheStartZoneStartsTheQuest) {
	const world::zone::ZoneName* zone = world::zone::ZoneName::createOrGet(IDRAKSHA_ZONE);
	registerXml(18739);
	Quester* q = makeQuester(810504, "Raksang", gameserver::model::Race::ELYOS, 60);
	EXPECT_TRUE(enterZone(*q, zone));
	Ptr<QuestState> qs = q->player().getQuestStateList()->getQuestState(18739);
	ASSERT_TRUE(qs);
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_EQ(q->sent(), cp::exactly({questAction(1, 18739, START), noNearbyQuests()}));
	enterZone(*q, zone); // fails the case on an error line: the not-startable path must answer, not throw
	EXPECT_TRUE(q->sent().empty()) << "START is not startable: no second start and no refusal message";
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::test::templates
