// P5-06c, M5d T-01b, T-02 and T-04 (m5d-plan.md §7, §18.3): the kill_in_world, kill_in_zone, kill_spawned and mentor_monster_hunt kinds -
// their *Data.register_ building KillInWorld, KillInZone, KillSpawned and MentorMonsterHunt (the four template .java files), driven through
// QuestEngine on in-world players (QuestTemplate1bTestSupport.h):
// - 13818 "[Weekly] Indirect Warfare" (kaldor.xml:122): a data-driven kill_in_world counting 6 player kills in Kaldor in var 1, the sixth
//   setting REWARD and var 1 (setQuestVar(var + 1)); reported and finished at leton (two medals of each reward row).
// - 15204 (cygnea.xml:190): its level_diff 5 refuses a kill of a player more than 5 levels below; 39006 (brusthonin.xml:227): invasion_world
//   registers it for every enter world. 18214 and 18224 (empyrean_crucible.xml:83, 85): not data-driven, the default pages 1011 and 2375, and
//   the script's start_dialog_id and end_dialog_id over them. 13801 (kaldor.xml:121): no amount, so the first kill rewards.
// - 15220 "[Daily] Help in Henor" (cygnea.xml:182): a data-driven kill_in_zone over two zones and level_diff 5, to its reward page, and its
//   finish: the exp, then the GP through E-09's GloryPointsService.addGp (QuestService.giveReward's order: kinah, exp, title, AP, DP, GP).
// - 36504 "[Daily] The Avian Flew" (fortuneers.xml:104-106): a kill_spawned without start npcs, its monster's one kill setting REWARD,
//   finished at an end npc. Its accept needs npc faction 4 (QuestService.checkStartConditions), so the cases hold the quest in START; the
//   spawner's USE_OBJECT needs the spawner's spawn in SPAWNS_DATA and a map instance of its world, which this fixture does not have.
// - mentor_monster_hunt: no quest of the data uses it; two fabricated rows name the mentor quests 37101 (MENTOR) and 37000 (MENTE).
// Registrations and rewards as `oracle.py m5d-quest --no-profile --quest 13818` (and 15220, 36504) print them.

#include "QuestTemplate1bTestSupport.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANK.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/handlers/models/Monster.h"
#include "aion/gameserver/questEngine/handlers/template/MentorMonsterHunt.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test::templates {
namespace {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::gameobjects::Npc;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

class KillTemplatesTest : public QuestTemplate1bTest {};

// KillInWorldData.register (KillInWorldData.java:43-46) and KillInWorld.register (KillInWorld.java:74-90): the start npc starts and talks (the
// end npcs are the start npcs); the listed world asks the quest about player kills there, and no other world does
TEST_F(KillTemplatesTest, KillInWorldRegistersItsNpcAndItsWorlds) {
	registerXml(13818);
	EXPECT_EQ(startQuests(LETON), (std::vector<int32_t>{13818}));
	EXPECT_EQ(talkQuests(LETON), (std::vector<int32_t>{13818}));
	Quester* q = makePlayer(810901, "Warfare", gameserver::model::Race::ELYOS, 65);
	Quester* victim = makePlayer(810902, "Victim", gameserver::model::Race::ASMODIANS, 65);
	Ref<QuestState> qs = hold(*q, 13818, QuestStatus::START);
	killInWorld(*q, *victim, 210010000);
	EXPECT_EQ(qs->getQuestVarById(1), 0) << "Poeta is not 13818's world";
	EXPECT_TRUE(q->sent().empty());
	killInWorld(*q, *victim, KALDOR);
	EXPECT_EQ(qs->getQuestVarById(1), 1);
}

// 13818 (KillInWorld.java:92-150): offered at leton with the data-driven 4762 and started; each kill in Kaldor counts in var 1
// (defaultOnKillRankedEvent(env, 0, 6, true, true)); the sixth sets REWARD and the vars to var 0 + 1; in REWARD leton shows 10002 for
// QUEST_SELECT and the reward page for another action, and finishes it
TEST_F(KillTemplatesTest, Quest13818CountsSixKillsInKaldorToItsReward) {
	registerXml(13818);
	Quester* q = makePlayer(810903, "Warfare", gameserver::model::Race::ELYOS, 65);
	Quester* victim = makePlayer(810904, "Victim", gameserver::model::Race::ASMODIANS, 65);
	Npc& leton = npcOf(LETON);
	const int32_t atNpc = leton.getObjectId();

	killInWorld(*q, *victim, KALDOR);
	EXPECT_TRUE(q->sent().empty()) << "no quest state";
	EXPECT_TRUE(talk(*q, 13818, DialogAction::QUEST_SELECT, leton));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atNpc, 4762, 13818)}));
	EXPECT_TRUE(talk(*q, 13818, DialogAction::QUEST_ACCEPT_1, leton));
	Ptr<QuestState> qs = q->player().getQuestStateList()->getQuestState(13818);
	ASSERT_TRUE(qs);
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_FALSE(talk(*q, 13818, DialogAction::QUEST_SELECT, leton)) << "START has no dialog";
	EXPECT_TRUE(q->sent().empty());

	for (int32_t kill = 1; kill <= 5; ++kill) {
		killInWorld(*q, *victim, KALDOR);
		EXPECT_EQ(qs->getQuestVarById(1), kill);
		EXPECT_EQ(q->sent(), cp::exactly({questUpdate(13818, START, kill << 6)})) << kill;
	}
	killInWorld(*q, *victim, KALDOR);
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(qs->getQuestVarById(0), 1) << "setQuestVar(var + 1): var 1 cleared";
	EXPECT_EQ(qs->getQuestVarById(1), 0);
	EXPECT_EQ(q->sent(), cp::exactly({questUpdate(13818, REWARD, 1), noNearbyQuests()}));
	killInWorld(*q, *victim, KALDOR);
	EXPECT_TRUE(q->sent().empty()) << "REWARD counts nothing";

	EXPECT_TRUE(talk(*q, 13818, DialogAction::QUEST_SELECT, leton));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atNpc, 10002, 13818)}));
	EXPECT_TRUE(talk(*q, 13818, DialogAction::USE_OBJECT, leton));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atNpc, 5, 13818)}));
	EXPECT_TRUE(talk(*q, 13818, DialogAction::SELECTED_QUEST_NOREWARD, leton));
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(held(*q, CERAMIUM_MEDAL), 1);
	EXPECT_EQ(held(*q, BLOOD_MARK), 4);
}

// level_diff (KillInWorld.java:139-141): 15204 refuses the kill of a player more than 5 levels below the killer; exactly 5 counts
TEST_F(KillTemplatesTest, KillInWorldRefusesAVictimMoreLevelsBelowThanTheLevelDiff) {
	registerXml(15204);
	Quester* q = makePlayer(810905, "Messenger", gameserver::model::Race::ELYOS, 65);
	Quester* low = makePlayer(810906, "Low", gameserver::model::Race::ASMODIANS, 59);
	Quester* near = makePlayer(810907, "Near", gameserver::model::Race::ASMODIANS, 60);
	Ref<QuestState> qs = hold(*q, 15204, QuestStatus::START);
	killInWorld(*q, *low, CYGNEA);
	EXPECT_EQ(qs->getQuestVarById(1), 0) << "65 - 59 > 5";
	EXPECT_TRUE(q->sent().empty());
	killInWorld(*q, *near, CYGNEA);
	EXPECT_EQ(qs->getQuestVarById(1), 1) << "65 - 60 = 5";
	EXPECT_EQ(q->sent(), cp::exactly({questUpdate(15204, START, 1 << 6)}));
}

// invasion_world (KillInWorld.java:84-85, 118-129): register_ asks 39006 (invasion_world 220050000, Brusthonin) about every enter world, and
// its onEnterWorldEvent reads the world's vortex location first (:121), which this fixture's unpublished VORTEX_DATA makes Java's
// NullPointerException, caught and logged by the engine (the I-05 hazard P5-06c.md names for MonsterHunt holds for these ten as well)
TEST_F(KillTemplatesTest, AnInvasionKillInWorldQuestIsAskedAboutEveryEnterWorld) {
	dataholders::DataManager::VORTEX_DATA.resetForTests();
	registerXml(39006);
	EXPECT_EQ(talkQuests(800500), (std::vector<int32_t>{39006})) << "the end npc talks (no start npc)";
	network::test::LogCapture log({"com.aionemu.gameserver.questEngine"});
	QuestEngine::getInstance().onEnterWorld(player());
	EXPECT_EQ(log.count("QE: exception in onEnterWorld"), 1) << log.dump();
	EXPECT_EQ(log.count("VortexData is not published"), 1) << log.dump();
}

// KillInZoneData.register (KillInZoneData.java:37-39) and KillInZone.register (KillInZone.java:66-79): the start npc starts and talks, both
// zones ask the quest about player kills there
TEST_F(KillTemplatesTest, KillInZoneRegistersItsNpcAndItsZones) {
	registerXml(15220);
	EXPECT_EQ(startQuests(MONODIA), (std::vector<int32_t>{15220}));
	EXPECT_EQ(talkQuests(MONODIA), (std::vector<int32_t>{15220}));
	Quester* q = makePlayer(810908, "Helper", gameserver::model::Race::ELYOS, 65);
	Quester* victim = makePlayer(810909, "Victim", gameserver::model::Race::ASMODIANS, 65);
	Ref<QuestState> qs = hold(*q, 15220, QuestStatus::START);
	killInZone(*q, *victim, "DRAGON_LORDS_GARDENS_210070000");
	EXPECT_EQ(qs->getQuestVarById(1), 0) << "not one of 15220's zones";
	killInZone(*q, *victim, CORAL_RISE);
	EXPECT_EQ(qs->getQuestVarById(1), 1);
	killInZone(*q, *victim, CRIMSON_HILLS);
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
}

// 15220 up to its reward page (KillInZone.java:81-128): offered with 4762 and started; the kills of a player more than 5 levels below count
// nothing; two kills in its zones count var 1 to REWARD (defaultOnKillInZoneEvent(env, 0, 2, true, true)); in REWARD monodia's USE_OBJECT
// shows the data-driven 10002, another action the reward page
TEST_F(KillTemplatesTest, Quest15220CountsTwoZoneKillsToItsRewardPage) {
	registerXml(15220);
	Quester* q = makePlayer(810910, "Helper", gameserver::model::Race::ELYOS, 65);
	Quester* victim = makePlayer(810911, "Victim", gameserver::model::Race::ASMODIANS, 60);
	Quester* low = makePlayer(810912, "Low", gameserver::model::Race::ASMODIANS, 59);
	Npc& monodia = npcOf(MONODIA);
	const int32_t atNpc = monodia.getObjectId();

	EXPECT_TRUE(talk(*q, 15220, DialogAction::QUEST_SELECT, monodia));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atNpc, 4762, 15220)}));
	EXPECT_TRUE(talk(*q, 15220, DialogAction::QUEST_ACCEPT_1, monodia));
	Ptr<QuestState> qs = q->player().getQuestStateList()->getQuestState(15220);
	ASSERT_TRUE(qs);
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_FALSE(talk(*q, 15220, DialogAction::USE_OBJECT, monodia)) << "START has no dialog";

	killInZone(*q, *low, CRIMSON_HILLS);
	EXPECT_EQ(qs->getQuestVarById(1), 0) << "65 - 59 > 5";
	EXPECT_TRUE(q->sent().empty());
	killInZone(*q, *victim, CRIMSON_HILLS);
	EXPECT_EQ(q->sent(), cp::exactly({questUpdate(15220, START, 1 << 6)}));
	killInZone(*q, *victim, CORAL_RISE);
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(q->sent(), cp::exactly({questUpdate(15220, REWARD, 1), noNearbyQuests()}));

	EXPECT_TRUE(talk(*q, 15220, DialogAction::USE_OBJECT, monodia));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atNpc, 10002, 15220)}));
	EXPECT_TRUE(talk(*q, 15220, DialogAction::QUEST_SELECT, monodia));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atNpc, 5, 15220)}));
}

// 15220's finish (KillInZone.java:115-121, QuestService.finishQuest and giveReward, E-09): the reward selection at monodia finishes it - the
// five Blood Marks, then 3618881 exp (STR_GET_EXP with monodia's name), then 4 GP (GP rate 1.0; E-09's GloryPointsService.addGp: the gain and
// the rank), then the daily's next-start message, its UPDATE(COMPLETE) and the nearby quests
TEST_F(KillTemplatesTest, Quest15220TheRewardSelectionPaysItsExpThenItsGp) {
	registerXml(15220);
	Quester* q = makePlayer(810913, "Helper", gameserver::model::Race::ELYOS, 65);
	Npc& monodia = npcOf(MONODIA);
	const int64_t exp = q->player().getCommonData()->getExp();
	Ref<QuestState> qs = hold(*q, 15220, QuestStatus::REWARD, 1);
	EXPECT_TRUE(talk(*q, 15220, DialogAction::SELECTED_QUEST_NOREWARD, monodia));
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(held(*q, BLOOD_MARK), 5);
	EXPECT_EQ(q->player().getCommonData()->getExp(), exp + 3618881);
	EXPECT_EQ(q->player().getAbyssRank()->getCurrentGP(), 4);
	// the finish's packets that show the order, named, in the order they were sent
	const std::string npcName = dataholders::DataManager::NPC_DATA->getNpcTemplate(MONODIA)->getL10n();
	const std::vector<std::pair<std::vector<uint8_t>, std::string>> named{
		{q->serializedFor(SM_SYSTEM_MESSAGE::STR_GET_EXP(npcName, 3618881)), "the exp"},
		{q->serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_GLORY_POINT_GAIN(4)), "the GP gain"},
		{q->serializedFor(network::aion::serverpackets::SM_ABYSS_RANK(q->player())), "the rank"},
		{questUpdate(15220, COMPLETE, 0), "UPDATE(COMPLETE)"}};
	std::vector<std::string> order;
	for (const std::vector<uint8_t>& packet : q->sent()) {
		if (items::opcodesOf({packet}).front() == SM_STATUPDATE_EXP_OPCODE)
			order.push_back("SM_STATUPDATE_EXP");
		for (const auto& [bytes, name] : named) {
			if (packet == bytes)
				order.push_back(name);
		}
	}
	EXPECT_EQ(order, (std::vector<std::string>{"SM_STATUPDATE_EXP", "the exp", "the GP gain", "the rank", "UPDATE(COMPLETE)"}))
		<< "the exp before the GP (QuestService.java:224-241), both before the update";
}

// Not data-driven kill_in_world (KillInWorld.java:100-122): 18214 offers 1011 and reports 2375 at junos; 18224 at the same npc has the
// script's start_dialog_id 4762 and end_dialog_id 10002 instead; in REWARD another action is the end dialog (the reward page)
TEST_F(KillTemplatesTest, KillInWorldPagesAreTheScriptsOrTheDefaultsOfAQuestNotDataDriven) {
	registerXml(18214);
	registerXml(18224);
	EXPECT_EQ(startQuests(JUNOS), (std::vector<int32_t>{18214, 18224}));
	Quester* q = makePlayer(810914, "Crucible", gameserver::model::Race::ELYOS, 61);
	Npc& junos = npcOf(JUNOS);
	const int32_t atNpc = junos.getObjectId();

	EXPECT_TRUE(talk(*q, 18214, DialogAction::QUEST_SELECT, junos));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atNpc, 1011, 18214)}));
	EXPECT_TRUE(talk(*q, 18224, DialogAction::QUEST_SELECT, junos));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atNpc, 4762, 18224)}));

	hold(*q, 18214, QuestStatus::REWARD, 1);
	hold(*q, 18224, QuestStatus::REWARD, 1);
	EXPECT_TRUE(talk(*q, 18214, DialogAction::QUEST_SELECT, junos));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atNpc, 2375, 18214)}));
	EXPECT_TRUE(talk(*q, 18224, DialogAction::QUEST_SELECT, junos));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atNpc, 10002, 18224)}));
	EXPECT_TRUE(talk(*q, 18214, DialogAction::USE_OBJECT, junos));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atNpc, 5, 18214)}));
}

// kill_in_world without amount (KillInWorld.java:59-62): 13801's kill amount is 1, so its first kill in Kaldor sets REWARD (and, data-driven,
// the vars to var 0 + 1)
TEST_F(KillTemplatesTest, AKillInWorldWithoutAmountRewardsTheFirstKill) {
	registerXml(13801);
	Quester* q = makePlayer(810915, "Minus", gameserver::model::Race::ELYOS, 65);
	Quester* victim = makePlayer(810916, "Victim", gameserver::model::Race::ASMODIANS, 65);
	Ref<QuestState> qs = hold(*q, 13801, QuestStatus::START);
	killInWorld(*q, *victim, KALDOR);
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(qs->getQuestVarById(0), 1);
	EXPECT_EQ(q->sent(), cp::exactly({questUpdate(13801, REWARD, 1), noNearbyQuests()}));
}

// A completed kill_spawned daily (KillSpawned.java:112-113: only START counts): the kill of its monster changes nothing although the var is 0
// again (QuestService.finishQuest resets it)
TEST_F(KillTemplatesTest, ACompletedKillSpawnedQuestCountsNoKill) {
	registerXml(36504);
	Quester* q = makePlayer(811005, "Avian", gameserver::model::Race::ELYOS, 50);
	Ref<QuestState> qs = hold(*q, 36504, QuestStatus::COMPLETE);
	kill(*q, npcOf(LIGHTNINGBEAK_PABU));
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(qs->getQuestVarById(0), 0);
	EXPECT_TRUE(q->sent().empty());
}

// KillSpawnedData.register (KillSpawnedData.java:31-33) and KillSpawned.register (KillSpawned.java:44-60): no start npc; both end npcs talk;
// the monster is a kill npc and its spawner a talk npc
TEST_F(KillTemplatesTest, KillSpawnedRegistersItsEndNpcsItsMonsterAndItsSpawner) {
	registerXml(36504);
	EXPECT_EQ(talkQuests(RIMA), (std::vector<int32_t>{36504}));
	EXPECT_EQ(talkQuests(SOCINUS), (std::vector<int32_t>{36504}));
	EXPECT_EQ(talkQuests(PLUMA_BAIT), (std::vector<int32_t>{36504}));
	EXPECT_EQ(killQuests(LIGHTNINGBEAK_PABU), (std::vector<int32_t>{36504}));
	for (int32_t npcId : {RIMA, SOCINUS, PLUMA_BAIT})
		EXPECT_EQ(startQuests(npcId), (std::vector<int32_t>{})) << npcId;
}

// 36504 (KillSpawned.java:62-137): without start npcs every talk npc offers it (1011, not data-driven), the spawner included; in START the end
// npcs answer only once every monster var is at its end_var; the monster's kill counts var 0 to 1, which ends every monster and sets REWARD;
// in REWARD an end npc shows the reward page and finishes it
TEST_F(KillTemplatesTest, Quest36504FromTheMonsterKillToTheReward) {
	registerXml(36504);
	Quester* q = makePlayer(811001, "Avian", gameserver::model::Race::ELYOS, 50);
	Npc& rima = npcOf(RIMA);
	Npc& socinus = npcOf(SOCINUS);
	Npc& bait = npcOf(PLUMA_BAIT);
	Npc& pabu = npcOf(LIGHTNINGBEAK_PABU);

	EXPECT_TRUE(talk(*q, 36504, DialogAction::QUEST_SELECT, rima));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(rima.getObjectId(), 1011, 36504)}));
	EXPECT_TRUE(talk(*q, 36504, DialogAction::QUEST_SELECT, bait));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(bait.getObjectId(), 1011, 36504)})) << "startNpcIds.isEmpty()";
	EXPECT_TRUE(talk(*q, 0, DialogAction::QUEST_SELECT, socinus)) << "a talk to the end npc reaches 36504 through its list";
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(socinus.getObjectId(), 1011, 36504)}));
	EXPECT_FALSE(talk(*q, 0, DialogAction::QUEST_SELECT, pabu)) << "the monster is no talk npc";

	kill(*q, pabu);
	EXPECT_TRUE(q->sent().empty()) << "no quest state";
	Ref<QuestState> qs = hold(*q, 36504, QuestStatus::START);
	EXPECT_FALSE(talk(*q, 36504, DialogAction::QUEST_SELECT, rima)) << "var 0 is below the monster's end_var";
	EXPECT_FALSE(talk(*q, 36504, DialogAction::QUEST_SELECT, bait)) << "the spawner answers only USE_OBJECT";
	EXPECT_TRUE(q->sent().empty());

	kill(*q, pabu);
	EXPECT_EQ(qs->getQuestVarById(0), 1);
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(q->sent(), cp::exactly({questUpdate(36504, REWARD, 1), noNearbyQuests()}));
	kill(*q, pabu);
	EXPECT_TRUE(q->sent().empty()) << "REWARD counts nothing";
	EXPECT_EQ(qs->getQuestVarById(0), 1);

	EXPECT_FALSE(talk(*q, 36504, DialogAction::USE_OBJECT, bait)) << "in REWARD the spawner is no end npc";
	EXPECT_TRUE(talk(*q, 36504, DialogAction::QUEST_SELECT, socinus));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(socinus.getObjectId(), 5, 36504)}));
	EXPECT_TRUE(talk(*q, 36504, DialogAction::SELECTED_QUEST_NOREWARD, rima));
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(held(*q, FORTUNEERS_TOKEN), 2);
	EXPECT_EQ(held(*q, FORTUNEER_REWARD_CHEST), 1);
}

// 36504 in START with its monster var at end_var (KillSpawned.java:96-107; only a quest whose kills did not set REWARD gets there): a kill
// counts nothing more (KillSpawned.java:117-119); the end npc's QUEST_SELECT shows 10002, and SELECT_QUEST_REWARD asks for page 5 in START,
// which the reward-page guard refuses (nothing sent)
TEST_F(KillTemplatesTest, Quest36504AtItsEndVarInStartShowsTheReportPage) {
	registerXml(36504);
	Quester* q = makePlayer(811002, "Avian", gameserver::model::Race::ELYOS, 50);
	Npc& rima = npcOf(RIMA);
	Npc& elpas = npcOf(ELPAS);
	Ref<QuestState> qs = hold(*q, 36504, QuestStatus::START, 1);
	kill(*q, npcOf(LIGHTNINGBEAK_PABU));
	EXPECT_EQ(qs->getQuestVarById(0), 1) << "var 0 is at end_var";
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_TRUE(q->sent().empty());
	EXPECT_TRUE(talk(*q, 36504, DialogAction::QUEST_SELECT, rima));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(rima.getObjectId(), 10002, 36504)}));
	EXPECT_FALSE(talk(*q, 36504, DialogAction::SELECT_QUEST_REWARD, rima));
	EXPECT_TRUE(q->sent().empty());
	EXPECT_FALSE(talk(*q, 36504, DialogAction::QUEST_SELECT, elpas)) << "not an end npc";
}

// The spawner's USE_OBJECT in START (KillSpawned.java:80-94): the spawner's monster is looked up and its spot read from SPAWNS_DATA in the
// player's world before anything is spawned. This fixture publishes no spawns, so the lookup is Java's NullPointerException on the null
// DataManager.SPAWNS_DATA, which the engine catches and logs: the case proves the spawner branch is taken (the spawn itself needs the
// spawner's world, docs/deviations/P5-06c.md). Another action at the spawner answers nothing
TEST_F(KillTemplatesTest, Quest36504TheSpawnersUseObjectLooksUpItsSpawn) {
	dataholders::DataManager::SPAWNS_DATA.resetForTests();
	registerXml(36504);
	Quester* q = makePlayer(811004, "Avian", gameserver::model::Race::ELYOS, 50);
	Npc& bait = npcOf(PLUMA_BAIT);
	hold(*q, 36504, QuestStatus::START, 1);
	EXPECT_FALSE(talk(*q, 36504, DialogAction::QUEST_SELECT, bait)) << "the spawner answers only USE_OBJECT";
	network::test::LogCapture log({"com.aionemu.gameserver.questEngine"});
	EXPECT_FALSE(QuestEngine::getInstance().onDialog(*envOf(*q, 36504, DialogAction::USE_OBJECT, at(bait))));
	EXPECT_EQ(log.count("QE: exception in onDialog"), 1) << log.dump();
	EXPECT_EQ(log.count("SpawnsData is not published"), 1) << log.dump();
}

// MentorMonsterHuntData.register (MentorMonsterHuntData.java:42-66) adds a MentorMonsterHunt, registered as a MonsterHunt
// (MonsterHunt.java:88-111): the start npc starts and talks; without start npcs the end npc talks
TEST_F(KillTemplatesTest, MentorMonsterHuntRegistersAsAMonsterHunt) {
	registerXml(37101);
	registerXml(37000);
	EXPECT_EQ(startQuests(RIMA), (std::vector<int32_t>{37101}));
	EXPECT_EQ(talkQuests(RIMA), (std::vector<int32_t>{37101}));
	EXPECT_EQ(talkQuests(SOCINUS), (std::vector<int32_t>{37000}));
	EXPECT_EQ(startQuests(SOCINUS), (std::vector<int32_t>{}));
}

// MentorMonsterHunt.onKillEvent (MentorMonsterHunt.java:29-57): a kill counts only through MonsterHunt.onKillEvent, which it reaches only for a
// mentor (MENTOR) or a grouped mente (MENTE) with a group member in range; a player without the quest, outside START, not a mentor or not in
// a group gets false; a mentor without a group is Java's NullPointerException. (A group cannot be built before M5g: GeneralTeam.getMembers is
// unported, so the counting branch is not reached here.)
TEST_F(KillTemplatesTest, AMentorKillCountsOnlyForAMentorOrAGroupedMente) {
	Npc& kerub = npcOf(STRIPED_KERUB);
	auto kerubMonsters = [] {
		std::vector<models::Monster> monsters(1);
		monsters[0].setEndVar(2);
		monsters[0].addNpcIds({STRIPED_KERUB});
		return monsters;
	};
	template_::MentorMonsterHunt mentor(37101, std::vector<int32_t>{RIMA}, std::nullopt, kerubMonsters(), 10, 20, true, false);
	template_::MentorMonsterHunt mente(37000, std::nullopt, std::vector<int32_t>{SOCINUS}, kerubMonsters(), 1, 99, true, false);
	Quester* q = makePlayer(811003, "Mentor", gameserver::model::Race::ELYOS, 50);
	auto killOf = [&](template_::MentorMonsterHunt& hunt, int32_t questId) {
		return hunt.onKillEvent(*QuestEnv::create(at(kerub), q->player(), questId));
	};

	EXPECT_FALSE(killOf(mentor, 37101)) << "no quest state";
	Ref<QuestState> qs = hold(*q, 37101, QuestStatus::REWARD);
	q->player().setMentor(true);
	EXPECT_FALSE(killOf(mentor, 37101)) << "not START (a mentor without a group is not asked for his group)";
	q->player().setMentor(false);
	qs->setStatus(QuestStatus::START);
	EXPECT_FALSE(killOf(mentor, 37101)) << "MENTOR: the player is no mentor";
	q->player().setMentor(true);
	EXPECT_THROW(killOf(mentor, 37101), runtime::NullPointerException) << "a mentor without a group";
	EXPECT_EQ(qs->getQuestVarById(0), 0);

	hold(*q, 37000, QuestStatus::START);
	EXPECT_FALSE(killOf(mente, 37000)) << "MENTE: the player is in no group";
	q->player().setMentor(false);
	EXPECT_FALSE(killOf(mente, 37000));
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::test::templates
