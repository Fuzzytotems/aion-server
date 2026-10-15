// P5-06c, M5d T-01a, T-02 and T-04 (m5d-plan.md §7): the monster_hunt kind - MonsterHuntData.register_ building the Monster list from the
// quest's <quest_kill> rows and the MonsterHunt handler (MonsterHunt.java), driven through QuestEngine on in-world players
// (QuestTemplateTestSupport.h):
// - 1102 "Kerubar Hunt" (poeta.xml:130; quest_data.xml:898-904): 3 kills of 210133 or 210134 counted in var 0, reported at mires for 400
//   kinah and 180 exp;
// - 24230 "A Grave Situation" (altgard.xml:229, end_reward="true"): the 9th kill of 210504/210505 sets REWARD itself;
// - 35052 "[Daily] Alabaster Orders" (alabaster_order.xml:159), data-driven: 5 kills of 702760 counted in var 1 at step 0; the step's last
//   kill moves the quest to step 1 (setQuestVar, which clears var 1) and, as the last step, to REWARD; its REWARD pages are 10002;
// - 1839 "A Deal with Silvius" (reshanta.xml:618): 67 kills, a count over two 6-bit vars; 1666 "Taking it to the Indratu" (heiron.xml:488):
//   two kill rows, both checked by the report; 15001 "Lending Both Hands" (cygnea.xml:193), data-driven with two kill rows at step 0;
//   39005 "Artillery Strike" (brusthonin.xml:222), an invasion_world quest asked about every enter world.
// Pages, statuses and vars as `oracle.py m5d-quest --quest 1102` (and 24230, 35052, 1839, 1666, 15001) prints them.

#include "QuestTemplateTestSupport.h"

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test::templates {
namespace {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::gameobjects::Npc;

class MonsterHuntTemplateTest : public QuestTemplateTest {};

// MonsterHuntData.register (MonsterHuntData.java:60-86) and MonsterHunt.register (MonsterHunt.java:77-103): the start npcs start and talk,
// each npc id of each <quest_kill> row is a kill npc, and without end_npc_ids the start npcs are the end npcs
TEST_F(MonsterHuntTemplateTest, RegisterMakesEveryKillRowNpcAKillNpc) {
	registerXml(1102);
	registerXml(35052);
	EXPECT_EQ(startQuests(MIRES), (std::vector<int32_t>{1102}));
	EXPECT_EQ(talkQuests(MIRES), (std::vector<int32_t>{1102}));
	EXPECT_EQ(killQuests(STRIPED_KERUB), (std::vector<int32_t>{1102}));
	EXPECT_EQ(killQuests(KERUB_2), (std::vector<int32_t>{1102}));
	EXPECT_EQ(killQuests(FIELD_STONE), (std::vector<int32_t>{35052}));
	EXPECT_EQ(talkQuests(DONAND), (std::vector<int32_t>{35052}));
	EXPECT_EQ(startQuests(DONAND), (std::vector<int32_t>{})) << "35052 has no start npc";
	EXPECT_EQ(killQuests(MIRES), (std::vector<int32_t>{}));
}

// 1102 (MonsterHunt.java:105-171, 173-239): accepted at mires; each kill of either kerub adds one to var 0 and sends the update, a kill past
// the count changes nothing, a kill of another npc or by a player without the quest changes nothing; mires asks for the kills (1352) and,
// with fewer than 3, SELECT_QUEST_REWARD answers false without a packet; with 3 it sets REWARD and shows page 5; in REWARD mires reports
// and finishes it (400 kinah, 180 exp)
TEST_F(MonsterHuntTemplateTest, Quest1102FromTheAcceptThroughTheKillsToTheReward) {
	registerXml(1102);
	Npc& mires = npcOf(MIRES);
	Npc& kerub = npcOf(STRIPED_KERUB);
	Npc& kerub2 = npcOf(KERUB_2);
	const int32_t atMires = mires.getObjectId();

	kill(*me, kerub); // no quest: onKillEvent answers false at qs == null (MonsterHunt.java:177)
	// (not asserted: no mutant of the template can count a kill for a null quest state)
	// (the kill helper still fails the case on an error line)

	hold(*me, 1101, QuestStatus::COMPLETE); // 1102's <finished quest_id="1101"/>
	EXPECT_TRUE(talk(*me, 1102, DialogAction::QUEST_SELECT, mires));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(atMires, 1011, 1102)}));
	EXPECT_TRUE(talk(*me, 1102, DialogAction::QUEST_ACCEPT_1, mires));
	Ptr<QuestState> qs = player().getQuestStateList()->getQuestState(1102);
	ASSERT_TRUE(qs);
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);

	EXPECT_TRUE(talk(*me, 1102, DialogAction::QUEST_SELECT, mires));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(atMires, 1352, 1102)}));
	EXPECT_FALSE(talk(*me, 1102, DialogAction::SELECT_QUEST_REWARD, mires)) << "0 of 3";
	EXPECT_TRUE(me->sent().empty());

	kill(*me, kerub);
	EXPECT_EQ(qs->getQuestVarById(0), 1);
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1102, START, 1)}));
	kill(*me, kerub2);
	EXPECT_EQ(qs->getQuestVarById(0), 2);
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1102, START, 2)}));
	EXPECT_FALSE(talk(*me, 1102, DialogAction::SELECT_QUEST_REWARD, mires)) << "2 of 3";
	EXPECT_TRUE(me->sent().empty());
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	kill(*me, npcOf(ELPAS)); // not a kill npc: the engine asks only the quests registered at the npc (QuestEngine.java:180-186)
	// (not asserted: the engine's filter, not the template's)
	kill(*me, kerub);
	EXPECT_EQ(qs->getQuestVarById(0), 3);
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1102, START, 3)}));
	EXPECT_EQ(qs->getStatus(), QuestStatus::START) << "no end_reward: the count alone does not reward";
	kill(*me, kerub);
	EXPECT_EQ(qs->getQuestVarById(0), 3) << "a 4th kill is past the count";
	EXPECT_TRUE(me->sent().empty());

	EXPECT_TRUE(talk(*me, 1102, DialogAction::SELECT_QUEST_REWARD, mires));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(qs->getQuestVarById(0), 3);
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1102, REWARD, 3), noNearbyQuests(), dialogWindow(atMires, 5, 1102)}));
	kill(*me, kerub);
	EXPECT_EQ(qs->getQuestVarById(0), 3) << "in REWARD a kill counts nothing";
	EXPECT_TRUE(me->sent().empty());

	EXPECT_TRUE(talk(*me, 1102, DialogAction::QUEST_SELECT, mires));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(atMires, 5, 1102)})) << "not data-driven: the reward page";
	EXPECT_FALSE(talk(*me, 1102, DialogAction::QUEST_SELECT, kerub)) << "not an end npc";
	EXPECT_TRUE(talk(*me, 1102, DialogAction::SELECTED_QUEST_NOREWARD, mires));
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(player().getInventory().getKinah(), 1000 + 400);
	EXPECT_EQ(me->f.commonData->getExp(), 180);
}

// The kill total of a Monster reads as many vars as end_var has 6-bit groups (MonsterHunt.java:131-140): 1102's count of 3 fits var 0, so
// var 1 does not add to it - with var 1 at 5 and var 0 at 2 the report still finds 2 of 3
TEST_F(MonsterHuntTemplateTest, TheReportReadsOnlyTheVarsOfTheKillCount) {
	registerXml(1102);
	Npc& mires = npcOf(MIRES);
	Ref<QuestState> qs = hold(*me, 1102, QuestStatus::START, 2 | 5 << 6);
	EXPECT_FALSE(talk(*me, 1102, DialogAction::SELECT_QUEST_REWARD, mires));
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	qs->setQuestVarById(0, 3);
	EXPECT_TRUE(talk(*me, 1102, DialogAction::SELECT_QUEST_REWARD, mires));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(qs->getQuestVarById(1), 5) << "the report keeps the vars";
}

// A player with AdminConfig.DIALOG_INFO's access level is told why the report is refused (MonsterHunt.java:141-145): the var after the last
// one read, the kill count and the total
TEST_F(MonsterHuntTemplateTest, AStaffPlayerIsToldTheKillTotalOfARefusedReport) {
	registerXml(1102);
	Npc& mires = npcOf(MIRES);
	hold(*me, 1102, QuestStatus::START, 2);
	configs::administration::AdminConfig::DIALOG_INFO.store(0); // every access level
	EXPECT_FALSE(talk(*me, 1102, DialogAction::SELECT_QUEST_REWARD, mires));
	EXPECT_EQ(me->sent(), cp::exactly({me->serializedFor(network::aion::serverpackets::SM_MESSAGE(0, "", "varId: 1; req endVar: 3; curr total: 2",
							   gameserver::model::ChatType::GOLDEN_YELLOW))}));
}

// A <quest_kill> row's seq above 0 is its var (MonsterHuntData.java:76-77): 1666's second row (npc_ids 213964..., no var, seq="1") counts
// in var 1, its first row (seq="0") in var 0
TEST_F(MonsterHuntTemplateTest, TheSeqOfAKillRowIsItsVar) {
	registerXml(1666);
	Ref<QuestState> qs = hold(*me, 1666, QuestStatus::START);
	kill(*me, npcOf(INDRATU_DEFENDER));
	EXPECT_EQ(qs->getQuestVarById(1), 1);
	EXPECT_EQ(qs->getQuestVarById(0), 0);
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1666, START, 1 << 6)}));
}

// end_reward="true" (24230, MonsterHunt.java:211-218): the kill that reaches the count also sets REWARD, with a second update
TEST_F(MonsterHuntTemplateTest, AnEndRewardQuestIsRewardedByItsLastKill) {
	registerXml(24230);
	Quester* asmodian = makeQuester(810301, "Graveguard", gameserver::model::Race::ASMODIANS, 14);
	Npc& sentry = npcOf(GRAVE_SENTRY);
	Ref<QuestState> qs = hold(*asmodian, 24230, QuestStatus::START, 7);
	kill(*asmodian, sentry);
	EXPECT_EQ(qs->getQuestVarById(0), 8);
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_EQ(asmodian->sent(), cp::exactly({questUpdate(24230, START, 8)}));
	kill(*asmodian, sentry);
	EXPECT_EQ(qs->getQuestVarById(0), 9);
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(asmodian->sent(), cp::exactly({questUpdate(24230, START, 9), questUpdate(24230, REWARD, 9), noNearbyQuests()}));
	Npc& brodir = npcOf(BRODIR);
	EXPECT_TRUE(talk(*asmodian, 24230, DialogAction::USE_OBJECT, brodir));
	EXPECT_EQ(asmodian->sent(), cp::exactly({dialogWindow(brodir.getObjectId(), 5, 24230)}));
}

// A data-driven monster_hunt (35052; MonsterHunt.java:183-236): a kill of the current step's npc counts in its var and sends the update; the
// step's last kill moves var 0 to the next step with setQuestVar (var 1 cleared) and, the step being the last, REWARD; the start page is
// 4762 and the REWARD pages at the end npc 10002, while SELECT_QUEST_REWARD there shows the reward page itself
TEST_F(MonsterHuntTemplateTest, ADataDrivenQuestCountsTheStepAndMovesToTheNext) {
	registerXml(35052);
	Npc& donand = npcOf(DONAND);
	Npc& stone = npcOf(FIELD_STONE);
	const int32_t atDonand = donand.getObjectId();
	EXPECT_TRUE(talk(*me, 35052, DialogAction::QUEST_SELECT, donand));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(atDonand, 4762, 35052)})) << "no start npc: any npc offers it";

	Ref<QuestState> qs = hold(*me, 35052, QuestStatus::START, 3 << 6);
	kill(*me, stone);
	EXPECT_EQ(qs->getQuestVarById(1), 4);
	EXPECT_EQ(qs->getQuestVarById(0), 0);
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(35052, START, 4 << 6)}));
	EXPECT_TRUE(talk(*me, 35052, DialogAction::QUEST_SELECT, donand));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(atDonand, 1352, 35052)}));

	kill(*me, stone);
	EXPECT_EQ(qs->getQuestVarById(0), 1) << "step 0 done: step 1";
	EXPECT_EQ(qs->getQuestVarById(1), 0) << "setQuestVar replaces all vars";
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD) << "step 0 was the last";
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(35052, START, 5 << 6), questUpdate(35052, REWARD, 1), noNearbyQuests()}));
	kill(*me, stone);
	EXPECT_TRUE(me->sent().empty()) << "REWARD";

	for (int32_t action : {DialogAction::QUEST_SELECT, DialogAction::USE_OBJECT}) {
		EXPECT_TRUE(talk(*me, 35052, action, donand)) << action;
		EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(atDonand, 10002, 35052)})) << action;
	}
	EXPECT_TRUE(talk(*me, 35052, DialogAction::SELECT_QUEST_REWARD, donand));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(atDonand, 5, 35052)}));
}

// A kill of a data-driven quest's npc for a step other than the current one counts nothing but still ends the step when its totals are met
// (MonsterHunt.java:185-186, 228-236): with var 0 at 1 (step 1, past 35052's only step) the kill skips the Monster, and the empty totals of
// step 1 (0 >= 0) move the quest to step 2 and REWARD
TEST_F(MonsterHuntTemplateTest, AKillForAnotherStepOfADataDrivenQuestOnlyChecksTheTotals) {
	registerXml(35052);
	Npc& stone = npcOf(FIELD_STONE);
	Ref<QuestState> qs = hold(*me, 35052, QuestStatus::START, 1 | 2 << 6);
	kill(*me, stone);
	EXPECT_EQ(qs->getQuestVarById(1), 0) << "setQuestVar(2) replaced var 1 without counting the kill";
	EXPECT_EQ(qs->getQuestVarById(0), 2);
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(35052, REWARD, 2), noNearbyQuests()}));
}

// A startable quest answers only at its start npcs (MonsterHunt.java:112-114): elpas does not offer 1102
TEST_F(MonsterHuntTemplateTest, OnlyTheStartNpcOffersTheQuest) {
	registerXml(1102);
	hold(*me, 1101, QuestStatus::COMPLETE);
	EXPECT_FALSE(talk(*me, 1102, DialogAction::QUEST_SELECT, npcOf(ELPAS)));
	EXPECT_TRUE(me->sent().empty());
}

// The script's dialog ids (MonsterHunt.java:117, 129): 11250 (start_dialog_id 4762, not data-driven) is offered with page 4762 at
// laestrygos, and 16900 (end_dialog_id 2375) asks for its kill at castor with 2375 in place of 1352
TEST_F(MonsterHuntTemplateTest, TheScriptsDialogIdsReplaceTheDefaultPages) {
	registerXml(11250);
	registerXml(16900);
	Npc& laestrygos = npcOf(LAESTRYGOS);
	EXPECT_TRUE(talk(*me, 11250, DialogAction::QUEST_SELECT, laestrygos));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(laestrygos.getObjectId(), 4762, 11250)}));
	Npc& castor = npcOf(CASTOR);
	hold(*me, 16900, QuestStatus::START);
	EXPECT_TRUE(talk(*me, 16900, DialogAction::QUEST_SELECT, castor));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(castor.getObjectId(), 2375, 16900)}));
}

// start_zone (MonsterHunt.java:98-99, 269-274, 281-288): with the zone known, register_ registers 18741 for it, and entering it starts the
// quest for an Elyos of level 60 (QuestService.startQuest: ADD in START); entering again with the quest held changes nothing
TEST_F(MonsterHuntTemplateTest, EnteringTheStartZoneStartsTheQuest) {
	const world::zone::ZoneName* zone = world::zone::ZoneName::createOrGet(IDRAKSHA_ZONE);
	registerXml(18741);
	Quester* q = makeQuester(810303, "Raksang", gameserver::model::Race::ELYOS, 60);
	EXPECT_TRUE(enterZone(*q, zone));
	Ptr<QuestState> qs = q->player().getQuestStateList()->getQuestState(18741);
	ASSERT_TRUE(qs);
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_EQ(q->sent(), cp::exactly({questAction(1, 18741, START), noNearbyQuests()}));
	enterZone(*q, zone); // fails the case on an error line: the not-startable path must answer, not throw
	EXPECT_TRUE(q->sent().empty()) << "START is not startable: no second start and no refusal message";
}

// A kill count above 63 spans two 6-bit vars (MonsterHunt.java:131-140, 188-209): 1839 counts its 67 kills in var 0 and var 1. The 64th kill
// carries var 0's 63 into var 1 (var 0 back to 0, the update showing 64); the report reads both vars, so silvius refuses it at 64 without a
// packet; the 67th kill (var 0 at 3, var 1 at 1) reaches the count and the report sets REWARD. As `oracle.py m5d-quest --quest 1839`'s kill
// run prints it (kill 64: vars 0 and 1; kill 67: vars 3 and 1)
TEST_F(MonsterHuntTemplateTest, AKillCountAbove63CarriesIntoTheNextVar) {
	registerXml(1839);
	Npc& silvius = npcOf(SILVIUS);
	Npc& swordlord = npcOf(ASHIKAR_SWORDLORD);
	Ref<QuestState> qs = hold(*me, 1839, QuestStatus::START, 63);
	kill(*me, swordlord);
	EXPECT_EQ(qs->getQuestVarById(0), 0);
	EXPECT_EQ(qs->getQuestVarById(1), 1);
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1839, START, 64)}));
	EXPECT_FALSE(talk(*me, 1839, DialogAction::SELECT_QUEST_REWARD, silvius)) << "64 of 67";
	EXPECT_TRUE(me->sent().empty());
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);

	qs->setQuestVarById(0, 2); // 66
	kill(*me, swordlord);
	EXPECT_EQ(qs->getQuestVarById(0), 3);
	EXPECT_EQ(qs->getQuestVarById(1), 1);
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1839, START, 67)}));
	EXPECT_TRUE(talk(*me, 1839, DialogAction::SELECT_QUEST_REWARD, silvius));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1839, REWARD, 67), noNearbyQuests(), dialogWindow(silvius.getObjectId(), 5, 1839)}));
}

// The report checks every <quest_kill> row (MonsterHunt.java:131-146): 1666's first row (var 0) done and its second (seq 1, var 1) at 2 of 5
// is refused without a packet; with var 1 at 5 javlantia sets REWARD and shows page 5 (oracle.py m5d-quest --quest 1666: vars 5 and 5)
TEST_F(MonsterHuntTemplateTest, TheReportChecksEveryKillRow) {
	registerXml(1666);
	Npc& javlantia = npcOf(JAVLANTIA);
	Ref<QuestState> qs = hold(*me, 1666, QuestStatus::START, 5 | 2 << 6);
	EXPECT_FALSE(talk(*me, 1666, DialogAction::SELECT_QUEST_REWARD, javlantia)) << "the second row at 2 of 5";
	EXPECT_TRUE(me->sent().empty());
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	qs->setQuestVarById(1, 5);
	EXPECT_TRUE(talk(*me, 1666, DialogAction::SELECT_QUEST_REWARD, javlantia));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1666, REWARD, 5 | 5 << 6), noNearbyQuests(), dialogWindow(javlantia.getObjectId(), 5, 1666)}));
}

// A data-driven step with two <quest_kill> rows (15001, both rows at step 0: 5 kills in var 1 and 5 in var 2; MonsterHunt.java:223-236)
// ends only when the sum of both rows' vars reaches the sum of their counts. As the kill run of `oracle.py m5d-quest --quest 15001` prints
// it: with var 1 full (5) the first kill of the second row counts 1 in var 2 and ends nothing (6 of 10; kill 7); with var 2 at 4 the next one
// makes 10 of 10, and the step moves to 1 (vars cleared) and, as the last step, to REWARD (kill 11)
TEST_F(MonsterHuntTemplateTest, ADataDrivenStepEndsWhenAllItsRowsAreDone) {
	registerXml(15001);
	Npc& ksellid = npcOf(BLUESHROOM_KSELLID);
	Ref<QuestState> qs = hold(*me, 15001, QuestStatus::START, 5 << 6);
	kill(*me, ksellid);
	EXPECT_EQ(qs->getQuestVarById(2), 1);
	EXPECT_EQ(qs->getQuestVarById(1), 5);
	EXPECT_EQ(qs->getQuestVarById(0), 0);
	EXPECT_EQ(qs->getStatus(), QuestStatus::START) << "6 of 10";
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(15001, START, 5 << 6 | 1 << 12)}));

	qs->setQuestVarById(2, 4);
	kill(*me, ksellid);
	EXPECT_EQ(qs->getQuestVarById(0), 1);
	EXPECT_EQ(qs->getQuestVarById(1), 0);
	EXPECT_EQ(qs->getQuestVarById(2), 0);
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD) << "10 of 10";
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(15001, START, 5 << 6 | 5 << 12), questUpdate(15001, REWARD, 1), noNearbyQuests()}));
}

// invasion_world (MonsterHunt.java:95-96, 246-258): register_ asks 39005 (invasion_world 220050000, Brusthonin) about every enter world, and
// its onEnterWorldEvent reads the world's vortex location before the world check (:250; VortexService.java:157-161). This fixture publishes
// no VORTEX_DATA, so the enter world of a Poeta player reaches the handler and fails there with Java's NullPointerException on the null
// DataManager.VORTEX_DATA, which the engine catches and logs (QuestEngine.java:347-348). That is the I-05 hazard P5-06c.md names: every
// process that registers the six invasion quests must publish VORTEX_DATA. The vortex and rift branches themselves are not reached here
TEST_F(MonsterHuntTemplateTest, AnInvasionQuestIsAskedAboutEveryEnterWorld) {
	dataholders::DataManager::VORTEX_DATA.resetForTests();
	registerXml(39005);
	network::test::LogCapture log({"com.aionemu.gameserver.questEngine"});
	QuestEngine::getInstance().onEnterWorld(player());
	EXPECT_EQ(log.count("QE: exception in onEnterWorld"), 1) << log.dump();
	EXPECT_EQ(log.count("VortexData is not published"), 1) << log.dump();
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::test::templates
