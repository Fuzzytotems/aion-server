// P5-06c, M5d T-01a, T-02 and T-04 (m5d-plan.md §7): the report_to_many kind - ReportToManyData.register_ building ReportToMany
// (ReportToMany.java), driven through QuestEngine on in-world players (QuestTemplateTestSupport.h):
// - 1115 "The Elim's Message" (poeta.xml:58-61): started at namus, one step at feira, the last at asteros, rewarded 680 kinah and 2673 exp;
// - 1118 "Polinia's Ointment" (poeta.xml:62-65): its work item 182200224 is given at the first step and taken at the report;
// - 29601 "Instruction on Instructors" (fatebound_abbey.xml:40-45), data-driven with four steps: the pages 4762, 1011 + step * 341 and 10002;
// - 4914 "Lifeform Remodeling Report" (pandaemonium.xml:348-351), started by using its start item 182207127 (registerQuestItem, no start npc);
//   the plan's other start-item route, 2274 (altgard.xml:154; its item 182203249 carries <queststart questid="2274"/>, so CM_USE_ITEM starts it
//   through QuestStartAction, E-10, still AION_UNPORTED), is owed to E-10 (P5-06c.md);
// - 13809 "Tree is Company" (kaldor.xml:59-64): three work items, one per step.
// Pages, statuses and vars as `oracle.py m5d-quest --quest 1115` (and 1118, 29601, 4914, 13809) prints them.

#include "QuestTemplateTestSupport.h"

#include <array>
#include <cstdint>
#include <vector>

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/questEngine/handlers/HandlerResult.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test::templates {
namespace {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::gameobjects::Npc;

class ReportToManyTemplateTest : public QuestTemplateTest {};

// ReportToManyData.register (ReportToManyData.java:36-38) and ReportToMany.register (ReportToMany.java:50-67): without a start item the start
// npcs start and talk; every npc of every step is a talk npc; with a start item the item is registered instead of the start npcs
TEST_F(ReportToManyTemplateTest, RegisterMakesTheStepNpcsTalkAndTheStartItemAQuestItem) {
	registerXml(1115);
	registerXml(4914);
	EXPECT_EQ(startQuests(NAMUS), (std::vector<int32_t>{1115}));
	EXPECT_EQ(talkQuests(NAMUS), (std::vector<int32_t>{1115}));
	EXPECT_EQ(talkQuests(FEIRA), (std::vector<int32_t>{1115}));
	EXPECT_EQ(talkQuests(ASTEROS), (std::vector<int32_t>{1115}));
	EXPECT_EQ(startQuests(FEIRA), (std::vector<int32_t>{}));
	EXPECT_EQ(talkQuests(204182), (std::vector<int32_t>{4914}));
	EXPECT_EQ(talkQuests(203385), (std::vector<int32_t>{4914}));
	EXPECT_EQ(startQuests(204182), (std::vector<int32_t>{}));

	Quester* asmodian = makeQuester(810401, "Remodeler", gameserver::model::Race::ASMODIANS, 10);
	gameserver::model::gameobjects::Item& report = holdItem(*asmodian, 820501, REMODELING_REPORT, 1);
	asmodian->clearSent();
	EXPECT_EQ(QuestEngine::getInstance().onItemUseEvent(*envOf(*asmodian, 0, 0), report), HandlerResult::SUCCESS);
	EXPECT_EQ(asmodian->sent(), cp::exactly({dialogWindow(0, 4, 4914)})) << "the accept window of 4914 (ReportToMany.java:172-182)";
}

// 1115 (ReportToMany.java:70-147): namus offers and starts it; in START only the npc of the current step answers - feira at step 0 with page
// 1352, SETPRO1 moving to step 1 and closing the window, a report action showing the selection page before the last step; asteros at the
// last step with 2375 and SELECT_QUEST_REWARD, which (no collect items, no work items) sets REWARD and shows page 5; in REWARD only the last
// step's npc reports, and finishes it (680 kinah, 2673 exp)
TEST_F(ReportToManyTemplateTest, Quest1115FromNpcToNpcToTheReward) {
	registerXml(1115);
	Quester* q = makeQuester(810402, "Messenger", gameserver::model::Race::ELYOS, 4);
	holdItem(*q, 820502, items::KINAH, 1000);
	Npc& namus = npcOf(NAMUS);
	Npc& feira = npcOf(FEIRA);
	Npc& asteros = npcOf(ASTEROS);

	EXPECT_TRUE(talk(*q, 1115, DialogAction::QUEST_SELECT, namus));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(namus.getObjectId(), 1011, 1115)}));
	EXPECT_FALSE(talk(*q, 1115, DialogAction::QUEST_SELECT, feira)) << "no start npc";
	EXPECT_TRUE(talk(*q, 1115, DialogAction::QUEST_ACCEPT_1, namus));
	Ptr<QuestState> qs = q->player().getQuestStateList()->getQuestState(1115);
	ASSERT_TRUE(qs);
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_EQ(q->sent(), cp::exactly({questAction(1, 1115, START), noNearbyQuests(), dialogWindow(namus.getObjectId(), 1003, 1115)}));

	EXPECT_FALSE(talk(*q, 1115, DialogAction::QUEST_SELECT, asteros)) << "step 0 is feira's";
	EXPECT_TRUE(q->sent().empty());
	EXPECT_TRUE(talk(*q, 1115, DialogAction::QUEST_SELECT, feira));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(feira.getObjectId(), 1352, 1115)}));
	EXPECT_TRUE(talk(*q, 1115, DialogAction::QUEST_REFUSE_1, feira)) << "other actions: AbstractQuestHandler.onDialogEvent";
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(feira.getObjectId(), 1004, 1115)}));
	EXPECT_TRUE(talk(*q, 1115, DialogAction::SELECT_QUEST_REWARD, feira));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(feira.getObjectId(), 10, 0)})) << "before the last step: the selection page";
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_TRUE(talk(*q, 1115, DialogAction::SETPRO1, feira));
	EXPECT_EQ(qs->getQuestVarById(0), 1);
	EXPECT_EQ(q->sent(), cp::exactly({questUpdate(1115, START, 1), dialogWindow(feira.getObjectId(), 0, 0)}));

	EXPECT_FALSE(talk(*q, 1115, DialogAction::QUEST_SELECT, feira)) << "step 1 is asteros'";
	EXPECT_TRUE(talk(*q, 1115, DialogAction::QUEST_SELECT, asteros));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(asteros.getObjectId(), 2375, 1115)}));
	EXPECT_TRUE(talk(*q, 1115, DialogAction::SELECT_QUEST_REWARD, asteros));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(qs->getQuestVarById(0), 1);
	EXPECT_EQ(q->sent(), cp::exactly({questUpdate(1115, REWARD, 1), noNearbyQuests(), dialogWindow(asteros.getObjectId(), 5, 1115)}));

	EXPECT_FALSE(talk(*q, 1115, DialogAction::USE_OBJECT, feira)) << "REWARD: only the last step's npc";
	EXPECT_TRUE(talk(*q, 1115, DialogAction::USE_OBJECT, asteros));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(asteros.getObjectId(), 5, 1115)}));
	const int64_t exp = q->f.commonData->getExp();
	EXPECT_TRUE(talk(*q, 1115, DialogAction::SELECTED_QUEST_NOREWARD, asteros));
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(q->player().getInventory().getKinah(), 1000 + 680);
	EXPECT_EQ(q->f.commonData->getExp(), exp + 2673);
}

// SET_SUCCEED at a step's npc (ReportToMany.java:119-132) counts as the next step: at feira's step 0 of 1115 it reaches the last step, sets
// var 0 to 1 and REWARD and closes the window (sendQuestEndDialog); the handler then remembers that the reward npc did not set the state
// itself, so asteros answers a talk with the whole report page 2375 before the reward (:142-143)
TEST_F(ReportToManyTemplateTest, SetSucceedAtAStepNpcRewardsAndTheEndNpcShowsTheReportPage) {
	registerXml(1115);
	Quester* q = makeQuester(810403, "Messenger", gameserver::model::Race::ELYOS, 4);
	Npc& feira = npcOf(FEIRA);
	Npc& asteros = npcOf(ASTEROS);
	Ref<QuestState> qs = hold(*q, 1115, QuestStatus::START);
	EXPECT_FALSE(talk(*q, 1115, DialogAction::SET_SUCCEED, asteros)) << "step 0 is feira's";
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_TRUE(talk(*q, 1115, DialogAction::SET_SUCCEED, feira));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(qs->getQuestVarById(0), 1);
	EXPECT_EQ(q->sent(), cp::exactly({questUpdate(1115, REWARD, 1), noNearbyQuests(), dialogWindow(feira.getObjectId(), 0, 0)}));
	EXPECT_TRUE(talk(*q, 1115, DialogAction::USE_OBJECT, asteros));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(asteros.getObjectId(), 2375, 1115)}));
	EXPECT_TRUE(talk(*q, 1115, DialogAction::QUEST_SELECT, asteros)) << "another action shows the reward page";
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(asteros.getObjectId(), 5, 1115)}));
}

// 1118's work item (ReportToMany.java:116-117, 160-170): each SETPRO step gives the work item of its index; the report takes the work items
// (validateAndRemoveItems) before the reward
TEST_F(ReportToManyTemplateTest, TheWorkItemOfAStepIsGivenThereAndTakenAtTheReport) {
	registerXml(1118);
	Quester* q = makeQuester(810404, "Ointment", gameserver::model::Race::ELYOS, 6);
	Npc& kustanon = npcOf(KUSTANON);
	Npc& melponeh = npcOf(MELPONEH);
	Ref<QuestState> qs = hold(*q, 1118, QuestStatus::START);
	EXPECT_TRUE(talk(*q, 1118, DialogAction::SETPRO1, kustanon));
	EXPECT_EQ(qs->getQuestVarById(0), 1);
	EXPECT_EQ(held(*q, POLINIAS_OINTMENT), 1);
	EXPECT_EQ(q->sent().back(), dialogWindow(kustanon.getObjectId(), 0, 0));
	EXPECT_TRUE(talk(*q, 1118, DialogAction::SETPRO2, melponeh)) << "any SETPRO of the last step moves on";
	EXPECT_EQ(qs->getQuestVarById(0), 2);
	EXPECT_EQ(held(*q, POLINIAS_OINTMENT), 1) << "no second work item";

	qs->setQuestVarById(0, 1);
	EXPECT_TRUE(talk(*q, 1118, DialogAction::CHECK_USER_HAS_QUEST_ITEM, melponeh));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(held(*q, POLINIAS_OINTMENT), 0);
	EXPECT_EQ(q->sent().back(), dialogWindow(melponeh.getObjectId(), 5, 1118));
}

// A step past the last npc_infos (var 0 at 2 of 1115's two steps) is refused with a warning (ReportToMany.java:92-95)
TEST_F(ReportToManyTemplateTest, AStepPastTheLastNpcIsRefusedWithAWarning) {
	registerXml(1115);
	Quester* q = makeQuester(810405, "Messenger", gameserver::model::Race::ELYOS, 4);
	hold(*q, 1115, QuestStatus::START, 2);
	network::test::LogCapture log({"com.aionemu.gameserver.questEngine.handlers.template.ReportToMany"});
	EXPECT_FALSE(talk(*q, 1115, DialogAction::QUEST_SELECT, npcOf(ASTEROS)));
	EXPECT_EQ(log.count("Missing NpcInfo for quest 1115 step #3"), 1) << log.dump();
	EXPECT_TRUE(log.contains("warning|")) << log.dump();
	EXPECT_TRUE(q->sent().empty());
}

// A data-driven report_to_many (29601): 4762 to offer it, 1011 + step * 341 at the steps before the last, 10002 at the last
TEST_F(ReportToManyTemplateTest, ADataDrivenQuestUsesTheNewPages) {
	registerXml(29601);
	Quester* q = makeQuester(810406, "Novice", gameserver::model::Race::ASMODIANS, 10);
	Npc& melanka = npcOf(MELANKA);
	EXPECT_TRUE(talk(*q, 29601, DialogAction::QUEST_SELECT, melanka));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(melanka.getObjectId(), 4762, 29601)}));
	Ref<QuestState> qs = hold(*q, 29601, QuestStatus::START);
	Npc& alda = npcOf(ALDA);
	EXPECT_TRUE(talk(*q, 29601, DialogAction::QUEST_SELECT, alda));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(alda.getObjectId(), 1011, 29601)}));
	qs->setQuestVarById(0, 1);
	Npc& leopold = npcOf(LEOPOLD);
	EXPECT_TRUE(talk(*q, 29601, DialogAction::QUEST_SELECT, leopold));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(leopold.getObjectId(), 1352, 29601)}));
	qs->setQuestVarById(0, 3);
	Npc& sigurd = npcOf(SIGURD);
	EXPECT_TRUE(talk(*q, 29601, DialogAction::QUEST_SELECT, sigurd));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(sigurd.getObjectId(), 10002, 29601)}));
}

// The start item (ReportToMany.java:77-78, 172-182): the start page needs the item in the cube; using it answers the accept window only while
// the quest can start, and UNKNOWN once it is held, so the engine lets the item go on
TEST_F(ReportToManyTemplateTest, TheStartItemOpensTheAcceptWindowWhileTheQuestCanStart) {
	registerXml(4914);
	Quester* q = makeQuester(810407, "Remodeler", gameserver::model::Race::ASMODIANS, 10);
	Npc& first = npcOf(NAMUS); // any npc: 4914 has no start npc
	EXPECT_FALSE(talk(*q, 4914, DialogAction::QUEST_SELECT, first)) << "no start item in the cube";
	EXPECT_TRUE(q->sent().empty());
	gameserver::model::gameobjects::Item& report = holdItem(*q, 820601, REMODELING_REPORT, 1);
	EXPECT_TRUE(talk(*q, 4914, DialogAction::QUEST_SELECT, first));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(first.getObjectId(), 1011, 4914)}));

	q->clearSent();
	EXPECT_EQ(QuestEngine::getInstance().onItemUseEvent(*envOf(*q, 0, 0), report), HandlerResult::SUCCESS);
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(0, 4, 4914)}));
	hold(*q, 4914, QuestStatus::START);
	q->clearSent();
	EXPECT_EQ(QuestEngine::getInstance().onItemUseEvent(*envOf(*q, 0, 0), report), HandlerResult::UNKNOWN);
	EXPECT_TRUE(q->sent().empty());
}

// The report of a start-item quest takes the start item (ReportToMany.java:160-170): without it validateAndRemoveItems fails and the
// selection page stays; with it 4914 is rewarded at hnoss (its work item is the same item, already gone)
TEST_F(ReportToManyTemplateTest, TheReportTakesTheStartItem) {
	registerXml(4914);
	Quester* q = makeQuester(810408, "Remodeler", gameserver::model::Race::ASMODIANS, 10);
	Npc& hnoss = npcOf(HNOSS);
	Ref<QuestState> qs = hold(*q, 4914, QuestStatus::START, 1);
	EXPECT_TRUE(talk(*q, 4914, DialogAction::SELECT_QUEST_REWARD, hnoss));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(hnoss.getObjectId(), 10, 0)}));
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	holdItem(*q, 820602, REMODELING_REPORT, 1);
	EXPECT_TRUE(talk(*q, 4914, DialogAction::SELECT_QUEST_REWARD, hnoss));
	EXPECT_EQ(held(*q, REMODELING_REPORT), 0);
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(q->sent().back(), dialogWindow(hnoss.getObjectId(), 5, 4914));
}

// Three work items over four steps (13809, kaldor.xml:59-64; ReportToMany.java:115-118, 160-170): each SETPRO step gives the work item of its
// index - the Shell Ember at the scorched tree (step 0), the Brittle Outer Scale at the cindery tree (1), the Crimson Bloodstain at the burnt
// tree (2) - and the report at caetess (step 3) takes all three, sets REWARD and shows page 5 (oracle.py m5d-quest --quest 13809)
TEST_F(ReportToManyTemplateTest, EachStepGivesItsOwnWorkItemAndTheReportTakesThemAll) {
	registerXml(13809);
	Npc& caetess = npcOf(CAETESS);
	Ref<QuestState> qs = hold(*me, 13809, QuestStatus::START);
	const std::array<int32_t, 3> trees{SCORCHED_TREE, CINDERY_TREE, BURNT_TREE};
	const std::array<int32_t, 3> workItems{SHELL_EMBER, BRITTLE_OUTER_SCALE, CRIMSON_BLOODSTAIN};
	for (int32_t step = 0; step < 3; step++) {
		EXPECT_TRUE(talk(*me, 13809, DialogAction::SETPRO1, npcOf(trees[step]))) << step;
		EXPECT_EQ(qs->getQuestVarById(0), step + 1);
		for (int32_t i = 0; i < 3; i++)
			EXPECT_EQ(held(*me, workItems[i]), i <= step ? 1 : 0) << "after step " << step << ", work item " << i;
	}
	EXPECT_TRUE(talk(*me, 13809, DialogAction::SELECT_QUEST_REWARD, caetess));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	for (int32_t workItem : workItems)
		EXPECT_EQ(held(*me, workItem), 0) << workItem;
	EXPECT_EQ(sentOf(*me, SM_DIALOG_WINDOW_OPCODE), cp::exactly({dialogWindow(caetess.getObjectId(), 5, 13809)}));
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::test::templates
