// P5-06c, M5d T-01a, T-02 and T-04 (m5d-plan.md §7): the report_to and report_on_levelup kinds - ReportToData/ReportOnLevelUpData.register_
// building ReportTo/ReportOnLevelUp (ReportTo.java, ReportOnLevelUp.java) and the quests they make, driven through QuestEngine on in-world
// players (QuestTemplateTestSupport.h):
// - 1101 "Sleeping on the Job" (poeta.xml:54): accepted at elpas, reported at mires, rewarded 120 kinah and 130 exp; finishing it opens the
//   start page of the follow-up 1102 (a monster_hunt that starts at mires and names 1101 as <finished>, quest_data.xml:898-904);
// - 1106 "Helping Kales" (poeta.xml:56), whose work item 182200203 is given on the accept and taken at the report (quest_data.xml:928-936);
// - 18970 "The Corridor Lore" (cygnea.xml:147), a data-driven report_to: the pages 4762 and 10002 in place of 1011 and 2375;
// - 13830 "Stigma 101" (stigma.xml:19), the report_on_levelup quest started in REWARD by the first level change or enter world of an Elyos
//   of level 30, reported at persephone for 46544 exp and a Stigma Support Bundle.
// Pages, statuses and vars as `oracle.py m5d-quest --quest 1101` (and 1106, 18970, 13830) prints them.

#include "QuestTemplateTestSupport.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/Npc.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test::templates {
namespace {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::gameobjects::Npc;

class ReportToTemplateTest : public QuestTemplateTest {};

// ReportToData.register (ReportToData.java:32-34) adds a ReportTo, whose register (ReportTo.java:50-60) makes each start npc a start and talk
// npc and, as the end npcs are another set, each end npc a talk npc
TEST_F(ReportToTemplateTest, RegisterMakesTheStartNpcsStartAndTalkAndTheEndNpcsTalk) {
	registerXml(1101);
	registerXml(18970);
	EXPECT_TRUE(QuestEngine::getInstance().isHaveHandler(1101));
	EXPECT_EQ(startQuests(ELPAS), (std::vector<int32_t>{1101}));
	EXPECT_EQ(talkQuests(ELPAS), (std::vector<int32_t>{1101}));
	EXPECT_EQ(startQuests(MIRES), (std::vector<int32_t>{})) << "an end npc starts nothing";
	EXPECT_EQ(talkQuests(MIRES), (std::vector<int32_t>{1101}));
	for (int32_t endNpc : {805213, 805214, 805215})
		EXPECT_EQ(talkQuests(endNpc), (std::vector<int32_t>{18970})) << endNpc;
	EXPECT_EQ(startQuests(BRUNTE), (std::vector<int32_t>{18970}));
}

// 1101 from its start page to the reward (ReportTo.java:63-107): elpas offers it (1011) and starts it; in START only mires answers, with the
// report page 2375 and SELECT_QUEST_REWARD, which sets var 1 and REWARD and shows the reward page; in REWARD mires shows the reward page and
// SELECTED_QUEST_NOREWARD finishes the quest (120 kinah, 130 exp), after which sendQuestEndDialog opens 1102's start page at mires
TEST_F(ReportToTemplateTest, Quest1101FromItsStartNpcToTheRewardAndTheFollowUp) {
	registerXml(1101);
	registerXml(1102);
	Npc& elpas = npcOf(ELPAS);
	Npc& mires = npcOf(MIRES);
	const int32_t atElpas = elpas.getObjectId();
	const int32_t atMires = mires.getObjectId();

	EXPECT_TRUE(talk(*me, 1101, DialogAction::QUEST_SELECT, elpas));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(atElpas, 1011, 1101)}));
	EXPECT_FALSE(talk(*me, 1101, DialogAction::QUEST_SELECT, mires)) << "mires does not start 1101";
	EXPECT_TRUE(me->sent().empty());
	EXPECT_FALSE(talk(*me, 1101, DialogAction::QUEST_ACCEPT_1, mires));
	EXPECT_FALSE(player().getQuestStateList()->hasQuest(1101));

	EXPECT_TRUE(talk(*me, 1101, DialogAction::QUEST_ACCEPT_1, elpas));
	Ptr<QuestState> qs = player().getQuestStateList()->getQuestState(1101);
	ASSERT_TRUE(qs);
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_EQ(me->sent(), cp::exactly({questAction(1, 1101, START), noNearbyQuests(), dialogWindow(atElpas, 1003, 1101)}));

	EXPECT_FALSE(talk(*me, 1101, DialogAction::QUEST_SELECT, elpas)) << "in START elpas is no end npc";
	EXPECT_TRUE(me->sent().empty());
	EXPECT_FALSE(talk(*me, 1101, DialogAction::USE_OBJECT, mires)) << "an action outside the START switch";
	EXPECT_TRUE(me->sent().empty());
	EXPECT_TRUE(talk(*me, 1101, DialogAction::QUEST_SELECT, mires));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(atMires, 2375, 1101)}));
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);

	EXPECT_TRUE(talk(*me, 1101, DialogAction::SELECT_QUEST_REWARD, mires));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(qs->getQuestVarById(0), 1);
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1101, REWARD, 1), noNearbyQuests(), dialogWindow(atMires, 5, 1101)}));

	EXPECT_FALSE(talk(*me, 1101, DialogAction::USE_OBJECT, elpas)) << "in REWARD only the end npc answers";
	EXPECT_TRUE(me->sent().empty());
	EXPECT_TRUE(talk(*me, 1101, DialogAction::USE_OBJECT, mires));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(atMires, 5, 1101)}));

	EXPECT_TRUE(talk(*me, 1101, DialogAction::SELECTED_QUEST_NOREWARD, mires));
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(player().getInventory().getKinah(), 1000 + 120);
	EXPECT_EQ(me->f.commonData->getExp(), 130);
	std::vector<std::vector<uint8_t>> sent = me->sent();
	ASSERT_FALSE(sent.empty());
	EXPECT_EQ(sentOf(*me, SM_QUEST_ACTION_OPCODE), cp::exactly({questUpdate(1101, COMPLETE)}));
	EXPECT_EQ(sent.back(), dialogWindow(atMires, 1011, 1102)) << "the follow-up: 1102's start page at mires";
	EXPECT_FALSE(player().getQuestStateList()->hasQuest(1102));

	EXPECT_FALSE(talk(*me, 1101, DialogAction::QUEST_SELECT, elpas)) << "a completed quest that cannot repeat is not offered again";
	EXPECT_TRUE(me->sent().empty());
}

// 1106's work item (ReportTo.java:42-46, 74-77, 88-94): the accept gives it; the report asks for its count - the one held is enough, fewer
// shows the selection page and keeps START - and takes every one held (the held count, not the work item's) before setting var 1 and REWARD
TEST_F(ReportToTemplateTest, TheWorkItemIsGivenOnTheAcceptAndTakenAtTheReport) {
	registerXml(1106);
	Npc& kales = npcOf(KALES);
	Npc& uno = npcOf(UNO);
	hold(*me, 1105, QuestStatus::COMPLETE); // 1106's <finished quest_id="1105"/>

	EXPECT_TRUE(talk(*me, 1106, DialogAction::QUEST_ACCEPT_1, kales));
	Ptr<QuestState> qs = player().getQuestStateList()->getQuestState(1106);
	ASSERT_TRUE(qs);
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_EQ(held(*me, GRAIN_SACK_1106), 1);
	EXPECT_EQ(sentOf(*me, items::SM_INVENTORY_ADD_ITEM_OPCODE).size(), 1u);
	EXPECT_EQ(me->sent().back(), dialogWindow(kales.getObjectId(), 1003, 1106));

	EXPECT_TRUE(talk(*me, 1106, DialogAction::SELECT_QUEST_REWARD, uno)) << "the one work item held";
	EXPECT_EQ(held(*me, GRAIN_SACK_1106), 0);
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(qs->getQuestVarById(0), 1);
	EXPECT_EQ(me->sent().back(), dialogWindow(uno.getObjectId(), 5, 1106));

	qs->setStatus(QuestStatus::START);
	qs->setQuestVar(0);
	EXPECT_TRUE(talk(*me, 1106, DialogAction::SELECT_QUEST_REWARD, uno));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(uno.getObjectId(), 10, 0)})) << "no work item: the selection page";
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_EQ(qs->getQuestVarById(0), 0);

	holdItem(*me, 820101, GRAIN_SACK_1106, 1);
	holdItem(*me, 820102, GRAIN_SACK_1106, 1);
	EXPECT_TRUE(talk(*me, 1106, DialogAction::SELECT_QUEST_REWARD, uno));
	EXPECT_EQ(held(*me, GRAIN_SACK_1106), 0) << "both taken";
	EXPECT_EQ(sentOf(*me, items::SM_DELETE_ITEM_OPCODE).size(), 2u);
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(me->sent().back(), dialogWindow(uno.getObjectId(), 5, 1106));
}

// Without end_npc_ids the start npcs are the end npcs (ReportTo.java:38-39): 80545 starts and reports at florarinerk, which register (the
// sets being equal) makes a start and talk npc once
TEST_F(ReportToTemplateTest, WithoutEndNpcsTheStartNpcReports) {
	registerXml(80545);
	EXPECT_EQ(startQuests(FLORARINERK), (std::vector<int32_t>{80545}));
	EXPECT_EQ(talkQuests(FLORARINERK), (std::vector<int32_t>{80545}));
	Npc& florarinerk = npcOf(FLORARINERK);
	hold(*me, 80545, QuestStatus::START);
	EXPECT_TRUE(talk(*me, 80545, DialogAction::QUEST_SELECT, florarinerk));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(florarinerk.getObjectId(), 2375, 80545)}));
}

// A data-driven report_to (18970, data_driven="true") offers page 4762 and reports with 10002 (ReportTo.java:73, 86); any of its three end
// npcs reports, and its start npc does not
TEST_F(ReportToTemplateTest, ADataDrivenQuestUsesTheNewPages) {
	registerXml(18970);
	Npc& brunte = npcOf(BRUNTE);
	Npc& finderyux = npcOf(FINDERYUX);
	EXPECT_TRUE(talk(*me, 18970, DialogAction::QUEST_SELECT, brunte));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(brunte.getObjectId(), 4762, 18970)}));
	EXPECT_FALSE(talk(*me, 18970, DialogAction::QUEST_SELECT, finderyux)) << "no start npc";

	Ref<QuestState> qs = hold(*me, 18970, QuestStatus::START);
	EXPECT_TRUE(talk(*me, 18970, DialogAction::QUEST_SELECT, finderyux));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(finderyux.getObjectId(), 10002, 18970)}));
	EXPECT_FALSE(talk(*me, 18970, DialogAction::QUEST_SELECT, brunte));

	qs->setStatus(QuestStatus::REWARD);
	EXPECT_TRUE(talk(*me, 18970, DialogAction::USE_OBJECT, finderyux));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(finderyux.getObjectId(), 5, 18970)}));
}

// A talk without a quest id (TalkEventHandler: QuestEngine.onDialog walks the npc's onTalkEvent list) reaches the registered ReportTo: a
// QUEST_SELECT at elpas opens 1101, and at an npc no quest registered nothing answers
TEST_F(ReportToTemplateTest, ATalkWithoutAQuestIdReachesTheQuestsOfTheNpc) {
	registerXml(1101);
	Npc& elpas = npcOf(ELPAS);
	EXPECT_TRUE(talk(*me, 0, DialogAction::QUEST_SELECT, elpas));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(elpas.getObjectId(), 1011, 1101)}));
	// (an npc nothing registered at is the engine's case: its onTalkEvent list is empty, QuestEngine.java:165-173)
	// (not asserted here)
}

class ReportOnLevelUpTemplateTest : public QuestTemplateTest {};

// ReportOnLevelUpData.register (ReportOnLevelUpData.java:26-28) adds a ReportOnLevelUp, whose register (ReportOnLevelUp.java:28-35) makes
// each end npc a talk npc and asks for every enter world and level change. An Elyos of level 30 gets 13830 started in REWARD by a level change
// (QuestService.startQuest(env, REWARD, false), :61-65) and only once; one of level 29 does not, and no message tells him. The level
// changes after the first go through levelChanged, which fails the case on an error line: the held-already path must answer, not throw
TEST_F(ReportOnLevelUpTemplateTest, ALevelChangeStartsTheQuestInRewardOnce) {
	registerXml(13830);
	EXPECT_EQ(talkQuests(PERSEPHONE), (std::vector<int32_t>{13830}));
	EXPECT_EQ(startQuests(PERSEPHONE), (std::vector<int32_t>{}));

	Quester* low = makeQuester(810201, "Twentynine", gameserver::model::Race::ELYOS, 29);
	levelChanged(*low);
	EXPECT_FALSE(low->player().getQuestStateList()->hasQuest(13830));
	EXPECT_TRUE(low->sent().empty()) << "warn false: no refusal message";

	Quester* stigmatic = makeQuester(810202, "Thirty", gameserver::model::Race::ELYOS, 30);
	levelChanged(*stigmatic);
	Ptr<QuestState> qs = stigmatic->player().getQuestStateList()->getQuestState(13830);
	ASSERT_TRUE(qs);
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(stigmatic->sent(), cp::exactly({questAction(1, 13830, REWARD), noNearbyQuests()}));

	levelChanged(*stigmatic);
	EXPECT_TRUE(stigmatic->sent().empty()) << "held already";
	qs->setStatus(QuestStatus::START);
	levelChanged(*stigmatic);
	EXPECT_EQ(qs->getStatus(), QuestStatus::START) << "hasQuest, not the status, stops a second start";
	EXPECT_TRUE(stigmatic->sent().empty());
}

// The enter world does the same (ReportOnLevelUp.java:52-54): QuestEngine.onEnterWorld hands every registered handler an env of its quest
TEST_F(ReportOnLevelUpTemplateTest, AnEnterWorldStartsTheQuestInReward) {
	registerXml(13830);
	Quester* stigmatic = makeQuester(810203, "Thirty", gameserver::model::Race::ELYOS, 30);
	enterWorld(*stigmatic);
	Ptr<QuestState> qs = stigmatic->player().getQuestStateList()->getQuestState(13830);
	ASSERT_TRUE(qs);
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(stigmatic->sent(), cp::exactly({questAction(1, 13830, REWARD), noNearbyQuests()}));
	enterWorld(*stigmatic); // fails the case on an error line: the held-already path must answer, not throw
	EXPECT_TRUE(stigmatic->sent().empty());
	EXPECT_FALSE(QuestEngine::getInstance().getQuestNpc(PERSEPHONE)->getOnQuestStart().contains(13830));
}

// Only REWARD at an end npc answers (ReportOnLevelUp.java:38-49): without the quest, in START, or at another npc it is false and silent; in
// REWARD persephone shows the reward page and finishes the quest (46544 exp and the Stigma Support Bundle)
TEST_F(ReportOnLevelUpTemplateTest, TheEndNpcReportsTheQuestInReward) {
	registerXml(13830);
	Quester* stigmatic = makeQuester(810204, "Thirty", gameserver::model::Race::ELYOS, 30);
	Npc& persephone = npcOf(PERSEPHONE);
	Npc& mires = npcOf(MIRES);
	EXPECT_FALSE(talk(*stigmatic, 13830, DialogAction::USE_OBJECT, persephone)) << "no quest state";
	EXPECT_TRUE(stigmatic->sent().empty());
	Ref<QuestState> qs = hold(*stigmatic, 13830, QuestStatus::START);
	// (START: false as well, but sendQuestEndDialog's own guard answers the same, AbstractQuestHandler.java:416-417 - not this template's)
	// (not asserted here)
	qs->setStatus(QuestStatus::REWARD);
	EXPECT_FALSE(talk(*stigmatic, 13830, DialogAction::USE_OBJECT, mires)) << "not an end npc";
	EXPECT_TRUE(stigmatic->sent().empty());

	EXPECT_TRUE(talk(*stigmatic, 13830, DialogAction::USE_OBJECT, persephone));
	EXPECT_EQ(stigmatic->sent(), cp::exactly({dialogWindow(persephone.getObjectId(), 5, 13830)}));
	const int64_t exp = stigmatic->f.commonData->getExp();
	EXPECT_TRUE(talk(*stigmatic, 13830, DialogAction::SELECTED_QUEST_NOREWARD, persephone));
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(stigmatic->f.commonData->getExp(), exp + 46544);
	EXPECT_EQ(held(*stigmatic, STIGMA_SUPPORT_BUNDLE), 1);
}

// The start branch hands the actions it does not handle to AbstractQuestHandler.onDialogEvent (ReportTo.java:78-79): QUEST_REFUSE_1 at elpas
// shows page 1004
TEST_F(ReportToTemplateTest, OtherStartActionsGoToTheHandlerBase) {
	registerXml(1101);
	Npc& elpas = npcOf(ELPAS);
	EXPECT_TRUE(talk(*me, 1101, DialogAction::QUEST_REFUSE_1, elpas));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(elpas.getObjectId(), 1004, 1101)}));
	EXPECT_FALSE(player().getQuestStateList()->hasQuest(1101));
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::test::templates
