// P5-06c, M5d T-01b, T-02 and T-04 (m5d-plan.md §7, §18.3): the item_order and skill_use kinds - ItemOrdersData/SkillUseData.register_
// building ItemOrders/SkillUse (ItemOrders.java, SkillUse.java), driven through QuestEngine on in-world players (QuestTemplate1bTestSupport.h):
// - 1323 "Lost Jewel Box" (eltnen.xml:399), the item_order whose start item 182201309 has no <queststart>, so CM_USE_ITEM hands it to
//   QuestEngine.onItemUseEvent directly (m5d-plan.md §17.3 N1): the item's accept window, the accept with and without the item, lodas's
//   SETPRO1 setting REWARD, justachys's report, the finish (2550 kinah, 100097 exp, a Bronze Coin; the Jewel Box taken back as the work item).
// - The other route, a start item with <queststart>, through E-10's QuestStartAction.act (CM_USE_ITEM leaves such an item to it): 1182
//   (verteron.xml:198), an item_order without talk npc, from its start item to its finish at selene; and 2274 (altgard.xml:154-157), stage
//   1a's report_to_many, from its start item to its accept.
// - 1514 (heiron.xml:441), the one item_order with two talk npcs: the second SETPRO1 sets REWARD.
// - 4922 "The Gladiator Preceptor's Test" (pandaemonium.xml:359-361): ten casts of 599 count var 0 up to REWARD; a completed one counts none.
// - 3910 "Meaning of Life" (sanctum.xml:392-394): end_var 100 spreads the count over vars 0 and 1, and the reward check compares var 0 alone
//   with 100 (SkillUse.java:117), so casting never sets REWARD; the end npc's SELECT_QUEST_REWARD does (the TODO at SkillUse.java:69).
// - 18738 (raksang_ruins.xml:47-49): its skill counts in var 1 (var_num 1), and the reward sets var 0 to 1 (SkillUse.java:122-123).
// Registrations and rewards as `oracle.py m5d-quest --no-profile --quest 1323` (and 1182, 2274, 1514, 4922, 3910, 18738) print them.

#include "QuestTemplate1bTestSupport.h"

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test::templates {
namespace {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::gameobjects::Item;
using gameserver::model::gameobjects::Npc;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

class ItemAndSkillTemplatesTest : public QuestTemplate1bTest {};

// ItemOrdersData.register (ItemOrdersData.java:31-33) and ItemOrders.register (ItemOrders.java:45-54): the start item is a quest item of 1323;
// the talk npc and the end npc talk; no npc starts it
TEST_F(ItemAndSkillTemplatesTest, ItemOrdersRegistersItsStartItemAndItsNpcs) {
	registerXml(1323);
	EXPECT_TRUE(QuestEngine::getInstance().isHaveHandler(1323));
	EXPECT_EQ(talkQuests(LODAS), (std::vector<int32_t>{1323}));
	EXPECT_EQ(talkQuests(JUSTACHYS), (std::vector<int32_t>{1323}));
	EXPECT_EQ(startQuests(LODAS), (std::vector<int32_t>{}));
	EXPECT_EQ(startQuests(JUSTACHYS), (std::vector<int32_t>{}));

	Quester* q = makePlayer(810801, "Finder", gameserver::model::Race::ELYOS, 22);
	Item& box = holdItem(*q, 820801, JEWEL_BOX, 1);
	EXPECT_EQ(useItem(*q, box), HandlerResult::SUCCESS);
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(0, 4, 1323)})) << "the accept window of 1323 (ItemOrders.java:112-120)";
}

// 1323 (ItemOrders.java:56-110): the accept needs the start item in the cube; in START lodas's SETPRO1 moves var 0 to 1 and, with one talk npc,
// sets REWARD; in REWARD justachys shows 2375 and finishes it; the item answers FAILED once the quest is held
TEST_F(ItemAndSkillTemplatesTest, Quest1323FromTheJewelBoxThroughLodasToTheReward) {
	registerXml(1323);
	Quester* q = makePlayer(810802, "Finder", gameserver::model::Race::ELYOS, 22);
	holdItem(*q, 820802, items::KINAH, 1000);
	Npc& lodas = npcOf(LODAS);
	Npc& justachys = npcOf(JUSTACHYS);

	EXPECT_TRUE(select(*q, 1323, DialogAction::QUEST_ACCEPT_1));
	EXPECT_FALSE(q->player().getQuestStateList()->hasQuest(1323)) << "no Jewel Box: no start";
	const std::string l10n = dataholders::DataManager::ITEM_DATA->getItemTemplate(JEWEL_BOX)->getL10n();
	EXPECT_EQ(q->sent(), cp::exactly({q->serializedFor(network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_QUEST_ACQUIRE_ERROR_INVENTORY_ITEM(l10n)),
							 dialogWindow(0, 0, 0)}));

	Item& box = holdItem(*q, 820803, JEWEL_BOX, 1);
	EXPECT_TRUE(select(*q, 1323, DialogAction::QUEST_ACCEPT_SIMPLE));
	Ptr<QuestState> qs = q->player().getQuestStateList()->getQuestState(1323);
	ASSERT_TRUE(qs);
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_EQ(q->sent(), cp::exactly({questAction(1, 1323, START), noNearbyQuests(), dialogWindow(0, 0, 0)}));
	EXPECT_EQ(held(*q, JEWEL_BOX), 1) << "the start item stays until the reward";
	EXPECT_EQ(useItem(*q, box), HandlerResult::FAILED) << "held already (ItemOrders.java:112-120)";
	EXPECT_TRUE(q->sent().empty());

	EXPECT_TRUE(talk(*q, 1323, DialogAction::QUEST_SELECT, lodas));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(lodas.getObjectId(), 1352, 1323)}));
	EXPECT_FALSE(talk(*q, 1323, DialogAction::USE_OBJECT, lodas)) << "neither QUEST_SELECT nor SETPRO1";
	EXPECT_TRUE(talk(*q, 1323, DialogAction::SETPRO1, lodas));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(qs->getQuestVarById(0), 1);
	EXPECT_EQ(q->sent(), cp::exactly({questUpdate(1323, REWARD, 1), noNearbyQuests(), dialogWindow(lodas.getObjectId(), 0, 0)}));

	EXPECT_FALSE(talk(*q, 1323, DialogAction::USE_OBJECT, lodas)) << "in REWARD only the end npc answers";
	EXPECT_TRUE(talk(*q, 1323, DialogAction::USE_OBJECT, justachys));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(justachys.getObjectId(), 2375, 1323)}));
	EXPECT_TRUE(talk(*q, 1323, DialogAction::SELECTED_QUEST_NOREWARD, justachys));
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(q->player().getInventory().getKinah(), 1000 + 2550);
	EXPECT_EQ(held(*q, BRONZE_COIN), 1);
	EXPECT_EQ(held(*q, JEWEL_BOX), 0) << "the work item is taken back when the quest completes";
}

// 1323's end npc in START (ItemOrders.java:93-99): 2375, and SELECT_QUEST_REWARD sets var 1 and REWARD at once (defaultCloseDialog(env, 0, 1,
// true, true)) and shows the reward page; a talk npc's SETPRO1 at var 1 with one talk npc (the second SETPRO1) moves the var without REWARD
TEST_F(ItemAndSkillTemplatesTest, Quest1323CanBeReportedAtItsEndNpcWithoutTheTalkNpc) {
	registerXml(1323);
	Quester* q = makePlayer(810803, "Hasty", gameserver::model::Race::ELYOS, 22);
	Npc& lodas = npcOf(LODAS);
	Npc& justachys = npcOf(JUSTACHYS);

	Ref<QuestState> qs = hold(*q, 1323, QuestStatus::START, 1);
	EXPECT_TRUE(talk(*q, 1323, DialogAction::SETPRO1, lodas));
	EXPECT_EQ(qs->getQuestVarById(0), 2);
	EXPECT_EQ(qs->getStatus(), QuestStatus::START) << "var 1 with one talk npc is no reward";
	EXPECT_EQ(q->sent(), cp::exactly({questUpdate(1323, START, 2), dialogWindow(lodas.getObjectId(), 0, 0)}))
		<< "no nearby-quest update in START (AbstractQuestHandler.java:125-126)";

	qs->setQuestVar(0);
	EXPECT_TRUE(talk(*q, 1323, DialogAction::QUEST_SELECT, justachys));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(justachys.getObjectId(), 2375, 1323)}));
	EXPECT_TRUE(talk(*q, 1323, DialogAction::SELECT_QUEST_REWARD, justachys));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(qs->getQuestVarById(0), 1);
	std::vector<std::vector<uint8_t>> sent = q->sent();
	ASSERT_FALSE(sent.empty());
	EXPECT_EQ(sent.back(), dialogWindow(justachys.getObjectId(), 5, 1323));
}

// SkillUseData.register (SkillUseData.java:37-39) and SkillUse.register (SkillUse.java:37-51): the start npc (and so end npc) starts and talks;
// every skill id of every <skill> is a quest skill
TEST_F(ItemAndSkillTemplatesTest, SkillUseRegistersItsNpcAndItsSkills) {
	registerXml(4922);
	registerXml(3910);
	EXPECT_EQ(startQuests(TRAUFNIR), (std::vector<int32_t>{4922}));
	EXPECT_EQ(talkQuests(TRAUFNIR), (std::vector<int32_t>{4922}));
	EXPECT_EQ(startQuests(THRASYMEDES), (std::vector<int32_t>{3910}));
	EXPECT_EQ(talkQuests(THRASYMEDES), (std::vector<int32_t>{3910}));

	Quester* q = makePlayer(810804, "Caster", gameserver::model::Race::ASMODIANS, 31);
	Ref<QuestState> qs4922 = hold(*q, 4922, QuestStatus::START);
	Ref<QuestState> qs3910 = hold(*q, 3910, QuestStatus::START);
	useSkill(*q, 599);
	EXPECT_EQ(qs4922->getQuestVarById(0), 1) << "599 is 4922's";
	EXPECT_EQ(qs3910->getQuestVarById(0), 0);
	useSkill(*q, 9912);
	EXPECT_EQ(qs3910->getQuestVarById(0), 1) << "9912 is 3910's";
	EXPECT_EQ(qs4922->getQuestVarById(0), 1);
	useSkill(*q, 600);
	EXPECT_TRUE(q->sent().empty()) << "no quest registered 600";
}

// 4922 (SkillUse.java:53-127): the start page is always 4762; each cast of 599 in START adds 1 to var 0; the tenth reaches end_var 10 and sets
// REWARD (var 0 stays 10); casts outside START count nothing; the end npc shows 10002 in START (another npc nothing) and the reward page in
// REWARD
TEST_F(ItemAndSkillTemplatesTest, Quest4922CountsTenCastsToItsReward) {
	registerXml(4922);
	Quester* q = makePlayer(810805, "Fury", gameserver::model::Race::ASMODIANS, 31);
	Npc& traufnir = npcOf(TRAUFNIR);
	const int32_t atNpc = traufnir.getObjectId();

	useSkill(*q, 599);
	EXPECT_TRUE(q->sent().empty()) << "no quest state";
	EXPECT_TRUE(talk(*q, 4922, DialogAction::QUEST_SELECT, traufnir));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atNpc, 4762, 4922)}));
	EXPECT_TRUE(talk(*q, 4922, DialogAction::QUEST_ACCEPT_1, traufnir));
	Ptr<QuestState> qs = q->player().getQuestStateList()->getQuestState(4922);
	ASSERT_TRUE(qs);
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);

	EXPECT_TRUE(talk(*q, 4922, DialogAction::QUEST_SELECT, traufnir));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atNpc, 10002, 4922)}));
	EXPECT_FALSE(talk(*q, 4922, DialogAction::QUEST_SELECT, npcOf(ELPAS))) << "not its end npc";
	EXPECT_TRUE(q->sent().empty());
	for (int32_t cast = 1; cast <= 9; ++cast) {
		useSkill(*q, 599);
		EXPECT_EQ(qs->getQuestVarById(0), cast);
		EXPECT_EQ(q->sent(), cp::exactly({questUpdate(4922, START, cast)})) << cast;
	}
	useSkill(*q, 599);
	EXPECT_EQ(qs->getQuestVarById(0), 10);
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(q->sent(), cp::exactly({questUpdate(4922, START, 10), questUpdate(4922, REWARD, 10), noNearbyQuests()}));
	useSkill(*q, 599);
	EXPECT_EQ(qs->getQuestVarById(0), 10);
	EXPECT_TRUE(q->sent().empty()) << "REWARD counts nothing";

	EXPECT_TRUE(talk(*q, 4922, DialogAction::QUEST_SELECT, traufnir));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atNpc, 5, 4922)}));
}

// 3910 (SkillUse.java:86-127): the count over two 6-bit vars (63 carries into var 1); at 100 var 0 is 36, not 100, so no REWARD, and a cast
// past 100 changes nothing; the end npc's SELECT_QUEST_REWARD in START sets REWARD whatever the count (changeQuestStep(var, var, true)),
// and the quest finishes for 3000 kinah and 64850 exp
TEST_F(ItemAndSkillTemplatesTest, Quest3910CarriesItsCountButOnlyTheEndNpcRewardsIt) {
	registerXml(3910);
	Quester* q = makePlayer(810806, "Resurrector", gameserver::model::Race::ELYOS, 17);
	holdItem(*q, 820806, items::KINAH, 1000);
	Npc& thrasymedes = npcOf(THRASYMEDES);
	const int32_t atNpc = thrasymedes.getObjectId();

	Ref<QuestState> qs = hold(*q, 3910, QuestStatus::START, 63);
	useSkill(*q, 9912);
	EXPECT_EQ(qs->getQuestVarById(0), 0);
	EXPECT_EQ(qs->getQuestVarById(1), 1) << "64 = 1 << 6";
	EXPECT_EQ(q->sent(), cp::exactly({questUpdate(3910, START, 1 << 6)}));

	qs->setQuestVarById(0, 35); // 99
	useSkill(*q, 9912);
	EXPECT_EQ(qs->getQuestVarById(0), 36);
	EXPECT_EQ(qs->getQuestVarById(1), 1);
	EXPECT_EQ(qs->getStatus(), QuestStatus::START) << "var 0 (36) is compared with end_var 100";
	EXPECT_EQ(q->sent(), cp::exactly({questUpdate(3910, START, 36 + (1 << 6))}));
	useSkill(*q, 9912);
	EXPECT_EQ(qs->getQuestVarById(0), 36) << "101 is past end_var";
	EXPECT_TRUE(q->sent().empty());

	EXPECT_TRUE(talk(*q, 3910, DialogAction::SELECT_QUEST_REWARD, thrasymedes));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(qs->getQuestVarById(0), 36) << "the var stays";
	EXPECT_EQ(q->sent(), cp::exactly({questUpdate(3910, REWARD, 36 + (1 << 6)), noNearbyQuests(), dialogWindow(atNpc, 5, 3910)}));
	EXPECT_TRUE(talk(*q, 3910, DialogAction::SELECTED_QUEST_NOREWARD, thrasymedes));
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(q->player().getInventory().getKinah(), 1000 + 3000);
}

// 1182 (ItemOrders.java:45-54, 56-120; QuestStartAction.java:40-88, E-10): register_ registers the end npc only (no talk_npc_id1, so no npc 0).
// The stone's use finishes at once (no casting delay): the use animation, STR_USE_ITEM, then QuestEngine.onItemUseEvent, where ItemOrders
// answers the accept window (page 4, no target) and SUCCESS, so no ASK_QUEST_ACCEPT dialog follows. The accept starts the quest; selene shows
// 2375 in START and SELECT_QUEST_REWARD sets var 1 and REWARD at once and shows the reward page; her reward selection finishes it (14200 kinah,
// 50400 exp; the stone taken back as the work item)
TEST_F(ItemAndSkillTemplatesTest, Quest1182StartsFromItsStartItemThroughQuestStartAction) {
	registerXml(1182);
	EXPECT_EQ(talkQuests(SELENE), (std::vector<int32_t>{1182}));
	EXPECT_EQ(talkQuests(0), (std::vector<int32_t>{})) << "talk_npc_id1 and talk_npc_id2 absent: nothing on npc 0";
	Quester* q = makePlayer(810807, "Digger", gameserver::model::Race::ELYOS, 17);
	holdItem(*q, 820807, items::KINAH, 1000);
	Item& stone = holdItem(*q, 820808, STONE_FRAGMENT, 1);
	Npc& selene = npcOf(SELENE);
	const int32_t atNpc = selene.getObjectId();
	const int64_t exp = q->player().getCommonData()->getExp();

	useStartItem(*q, stone);
	EXPECT_EQ(q->sent(), cp::exactly({itemUsageAnimation(810807, 820808, STONE_FRAGMENT, 0, 1, 1),
							 q->serializedFor(SM_SYSTEM_MESSAGE::STR_USE_ITEM(stone.getL10n())), dialogWindow(0, 4, 1182)}));
	EXPECT_TRUE(select(*q, 1182, DialogAction::QUEST_ACCEPT_1));
	EXPECT_EQ(q->sent(), cp::exactly({questAction(1, 1182, START), noNearbyQuests(), dialogWindow(0, 0, 0)}));
	Ptr<QuestState> qs = q->player().getQuestStateList()->getQuestState(1182);
	ASSERT_TRUE(qs);

	EXPECT_TRUE(talk(*q, 1182, DialogAction::QUEST_SELECT, selene));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atNpc, 2375, 1182)}));
	EXPECT_TRUE(talk(*q, 1182, DialogAction::SELECT_QUEST_REWARD, selene));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(qs->getQuestVarById(0), 1);
	EXPECT_EQ(lastSent(*q), dialogWindow(atNpc, 5, 1182));
	EXPECT_TRUE(talk(*q, 1182, DialogAction::SELECTED_QUEST_NOREWARD, selene));
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(q->player().getInventory().getKinah(), 1000 + 14200);
	EXPECT_EQ(q->player().getCommonData()->getExp(), exp + 50400);
	EXPECT_EQ(held(*q, STONE_FRAGMENT), 0) << "the work item is taken back when the quest completes";
}

// 2274 (ReportToMany.java:69-86, 189-196; QuestStartAction.java:40-88, E-10): the baton's use hands it to ReportToMany.onItemUseEvent, which
// answers the accept window (page 4); the accept starts the quest while the baton is in the cube; a second use of the baton is silent (held and
// not startable, QuestStartAction.java:74-83)
TEST_F(ItemAndSkillTemplatesTest, Quest2274StartsFromItsStartItemThroughQuestStartAction) {
	registerXml(2274);
	Quester* q = makePlayer(810808, "Chieftain", gameserver::model::Race::ASMODIANS, 16);
	Item& baton = holdItem(*q, 820809, CHIEFTAINS_BATON, 1);
	const std::vector<uint8_t> animation = itemUsageAnimation(810808, 820809, CHIEFTAINS_BATON, 0, 1, 1);
	const std::vector<uint8_t> used = q->serializedFor(SM_SYSTEM_MESSAGE::STR_USE_ITEM(baton.getL10n()));

	useStartItem(*q, baton);
	EXPECT_EQ(q->sent(), cp::exactly({animation, used, dialogWindow(0, 4, 2274)}));
	EXPECT_TRUE(select(*q, 2274, DialogAction::QUEST_ACCEPT_1));
	EXPECT_EQ(sentOf(*q, SM_QUEST_ACTION_OPCODE), cp::exactly({questAction(1, 2274, START)}));
	useStartItem(*q, baton);
	EXPECT_EQ(q->sent(), cp::exactly({animation, used}));
}

// 1514 (ItemOrders.java:79-88): with two talk npcs the first SETPRO1 (var 0) moves the var without REWARD, the second (var 1, at the other
// talk npc) sets REWARD; register_ registers both talk npcs and the end npc
TEST_F(ItemAndSkillTemplatesTest, Quest1514WithTwoTalkNpcsRewardsAtTheSecondSetpro1) {
	registerXml(1514);
	EXPECT_EQ(talkQuests(IBELIA), (std::vector<int32_t>{1514}));
	EXPECT_EQ(talkQuests(SULATES), (std::vector<int32_t>{1514}));
	EXPECT_EQ(talkQuests(203831), (std::vector<int32_t>{1514})) << "the end npc";
	Quester* q = makePlayer(810809, "Necklace", gameserver::model::Race::ELYOS, 31);
	Npc& ibelia = npcOf(IBELIA);
	Npc& sulates = npcOf(SULATES);
	Ref<QuestState> qs = hold(*q, 1514, QuestStatus::START);
	EXPECT_TRUE(talk(*q, 1514, DialogAction::SETPRO1, ibelia));
	EXPECT_EQ(qs->getQuestVarById(0), 1);
	EXPECT_EQ(qs->getStatus(), QuestStatus::START) << "var 0 with a second talk npc is no reward";
	EXPECT_EQ(q->sent(), cp::exactly({questUpdate(1514, START, 1), dialogWindow(ibelia.getObjectId(), 0, 0)}));
	EXPECT_TRUE(talk(*q, 1514, DialogAction::SETPRO1, sulates));
	EXPECT_EQ(qs->getQuestVarById(0), 2);
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(q->sent(), cp::exactly({questUpdate(1514, REWARD, 2), noNearbyQuests(), dialogWindow(sulates.getObjectId(), 0, 0)}));
}

// 18738 (SkillUse.java:98-124, var_num 1): each cast of 10981 counts in var 1; the tenth reaches end_var 10 and, var 0 being 0, sets var 0 to 1
// before REWARD
TEST_F(ItemAndSkillTemplatesTest, Quest18738CountsInVar1AndSetsVar0AtTheReward) {
	registerXml(18738);
	Quester* q = makePlayer(810810, "Bomber", gameserver::model::Race::ELYOS, 60);
	Ref<QuestState> qs = hold(*q, 18738, QuestStatus::START, 8 << 6);
	useSkill(*q, 10981);
	EXPECT_EQ(qs->getQuestVarById(1), 9);
	EXPECT_EQ(qs->getQuestVarById(0), 0);
	EXPECT_EQ(q->sent(), cp::exactly({questUpdate(18738, START, 9 << 6)}));
	useSkill(*q, 10981);
	EXPECT_EQ(qs->getQuestVarById(1), 10);
	EXPECT_EQ(qs->getQuestVarById(0), 1);
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(q->sent(), cp::exactly({questUpdate(18738, START, 10 << 6), questUpdate(18738, REWARD, 1 + (10 << 6)), noNearbyQuests()}));
}

// A completed skill_use quest (SkillUse.java:98: only START counts): 4922's cast changes nothing although the var is 0 again
TEST_F(ItemAndSkillTemplatesTest, ACompletedSkillUseQuestCountsNoCast) {
	registerXml(4922);
	Quester* q = makePlayer(810811, "Fury", gameserver::model::Race::ASMODIANS, 31);
	Ref<QuestState> qs = hold(*q, 4922, QuestStatus::COMPLETE);
	useSkill(*q, 599);
	EXPECT_EQ(qs->getQuestVarById(0), 0);
	EXPECT_TRUE(q->sent().empty());
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::test::templates
