// P5-06c, M5d T-01b, T-02 and T-04 (m5d-plan.md §7, §18.3): the relic_rewards and fountain_rewards kinds - RelicRewardsData/FountainRewardsData
// .register_ building RelicRewards/FountainRewards (RelicRewards.java, FountainRewards.java), driven through QuestEngine on in-world players
// (QuestTemplate1bTestSupport.h):
// - 21281 "[Relic Reward] Ancient Icon" (gelkmaros.xml:416): EXCHANGE_COIN with a relic starts it (the collect items' start_check,
//   QuestService.checkAndGetCollectItemQuestRewardCategory) and offers the category page; SELECT1..4 hand in one relic of that category and
//   set its reward group, var and REWARD.
// - 15205 "Cygnea Fountain of Luck" (cygnea.xml:179): the fountain wants a Platinum Medal in the cube (inventory_items), SETPRO1 starts the
//   quest and sets REWARD at once; in REWARD any action but the reward selection abandons it.
// Both finishes pay through E-09's bodies, which the dialog-and-rewards lane ported in this wave: 21281 AP through AbyssPointsService.addAp
// (the reward group of its category, unrated for a NON_COUNT quest), 15205 a MEDAL bonus of level 2 through BonusService.getQuestBonus (one of
// the medal group's level-2 rows, QuestTemplate1bTestSupport.h's T1B_ITEM_GROUPS_XML). Registrations and rewards as `oracle.py m5d-quest
// --no-profile --quest 21281` (and 15205) print them.

#include "QuestTemplate1bTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANK.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test::templates {
namespace {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::gameobjects::Npc;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

class CoinTemplatesTest : public QuestTemplate1bTest {
protected:
	static std::string l10nOf(int32_t itemId) { return dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId)->getL10n(); }
};

// RelicRewardsData.register (RelicRewardsData.java:27-29) / FountainRewardsData.register (FountainRewardsData.java:27-29) and the templates'
// register (RelicRewards.java:32-37, FountainRewards.java:30-35): every start npc starts and talks
TEST_F(CoinTemplatesTest, RelicAndFountainRewardsRegisterTheirStartNpcs) {
	registerXml(21281);
	registerXml(15205);
	for (int32_t npcId : {FRIGGA, VALITH}) {
		EXPECT_EQ(startQuests(npcId), (std::vector<int32_t>{21281})) << npcId;
		EXPECT_EQ(talkQuests(npcId), (std::vector<int32_t>{21281})) << npcId;
	}
	for (int32_t npcId : {ORIEL_COIN_FOUNTAIN, FOUNTAIN_OF_LUCK}) {
		EXPECT_EQ(startQuests(npcId), (std::vector<int32_t>{15205})) << npcId;
		EXPECT_EQ(talkQuests(npcId), (std::vector<int32_t>{15205})) << npcId;
	}
}

// 21281 (RelicRewards.java:39-101): EXCHANGE_COIN below level 50 or without a relic shows 3398; with a relic it starts the quest and shows
// 1011; SELECT1 hands in the Lesser Ancient Icon (category 0): reward group 0, var 1, REWARD, page 5; SELECT2 without an Ancient Icon tells
// the player and shows 1009; in REWARD USE_OBJECT shows the page of the var (var + 4)
TEST_F(CoinTemplatesTest, Quest21281StartsWithARelicAndSetsTheRewardOfItsCategory) {
	registerXml(21281);
	Quester* young = makePlayer(811101, "Young", gameserver::model::Race::ASMODIANS, 49);
	Quester* q = makePlayer(811102, "Collector", gameserver::model::Race::ASMODIANS, 50);
	Npc& frigga = npcOf(FRIGGA);
	Npc& valith = npcOf(VALITH);
	const int32_t atFrigga = frigga.getObjectId();

	holdItem(*young, 811103, LESSER_ANCIENT_ICON, 1);
	EXPECT_TRUE(talk(*young, 21281, DialogAction::EXCHANGE_COIN, frigga));
	EXPECT_EQ(young->sent(), cp::exactly({dialogWindow(atFrigga, 3398, 21281)})) << "level 49 < minlevel_permitted 50";
	EXPECT_FALSE(young->player().getQuestStateList()->hasQuest(21281));

	EXPECT_TRUE(talk(*q, 21281, DialogAction::EXCHANGE_COIN, frigga));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atFrigga, 3398, 21281)})) << "no relic";
	EXPECT_FALSE(talk(*q, 21281, DialogAction::QUEST_SELECT, frigga)) << "only EXCHANGE_COIN before the quest";
	EXPECT_TRUE(q->sent().empty());

	holdItem(*q, 811104, LESSER_ANCIENT_ICON, 1);
	EXPECT_TRUE(talk(*q, 21281, DialogAction::EXCHANGE_COIN, frigga));
	Ptr<QuestState> qs = q->player().getQuestStateList()->getQuestState(21281);
	ASSERT_TRUE(qs);
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	std::vector<std::vector<uint8_t>> sent = q->sent();
	ASSERT_FALSE(sent.empty());
	EXPECT_EQ(sent.front(), emptyQuestAction()) << "SM_QUEST_ACTION(ADD) of a COIN_QUEST";
	EXPECT_EQ(sent.back(), dialogWindow(atFrigga, 1011, 21281));

	EXPECT_TRUE(talk(*q, 21281, DialogAction::USE_OBJECT, valith));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(valith.getObjectId(), 1011, 21281)}));
	EXPECT_TRUE(talk(*q, 21281, DialogAction::SELECT2, frigga));
	EXPECT_EQ(q->sent(), cp::exactly({q->serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_QUEST_COMPLETE_ERROR_QUEST_ITEM_RETRY(l10nOf(ANCIENT_ICON))),
							 dialogWindow(atFrigga, 1009, 21281)}));
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_TRUE(talk(*q, 21281, DialogAction::QUEST_SELECT, frigga)) << "any other action: no category";
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atFrigga, 1009, 21281)}));

	EXPECT_TRUE(talk(*q, 21281, DialogAction::SELECT1, frigga));
	EXPECT_EQ(held(*q, LESSER_ANCIENT_ICON), 0);
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(qs->getQuestVarById(0), 1);
	EXPECT_EQ(qs->getRewardGroup(), 0);
	sent = q->sent();
	ASSERT_FALSE(sent.empty());
	EXPECT_EQ(sentOf(*q, SM_QUEST_ACTION_OPCODE), cp::exactly({emptyQuestAction()}));
	EXPECT_EQ(sent.back(), dialogWindow(atFrigga, 5, 21281));

	EXPECT_TRUE(talk(*q, 21281, DialogAction::USE_OBJECT, valith));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(valith.getObjectId(), 5, 21281)})) << "var 1 + 4";
	EXPECT_FALSE(talk(*q, 21281, DialogAction::QUEST_SELECT, valith)) << "REWARD answers USE_OBJECT and the reward selection only";
	EXPECT_FALSE(talk(*q, 21281, DialogAction::USE_OBJECT, npcOf(ELPAS))) << "not a start npc";
}

// The fourth category (RelicRewards.java:74-86): SELECT4 hands in the Major Ancient Icon, sets reward group 3 and var 4, and its reward page
// is 8 (rewardId + 5, then var + 4); a START quest whose var is not 0 answers nothing
TEST_F(CoinTemplatesTest, Quest21281TheFourthCategory) {
	registerXml(21281);
	Quester* q = makePlayer(811105, "Collector", gameserver::model::Race::ASMODIANS, 50);
	Npc& frigga = npcOf(FRIGGA);
	const int32_t atFrigga = frigga.getObjectId();
	Ref<QuestState> qs = hold(*q, 21281, QuestStatus::START, 1);
	holdItem(*q, 811106, MAJOR_ANCIENT_ICON, 1);
	EXPECT_FALSE(talk(*q, 21281, DialogAction::SELECT4, frigga)) << "var 1: no START branch";
	EXPECT_TRUE(q->sent().empty());

	qs->setQuestVar(0);
	EXPECT_TRUE(talk(*q, 21281, DialogAction::SELECT4, frigga));
	EXPECT_EQ(held(*q, MAJOR_ANCIENT_ICON), 0);
	EXPECT_EQ(qs->getRewardGroup(), 3);
	EXPECT_EQ(qs->getQuestVarById(0), 4);
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	std::vector<std::vector<uint8_t>> sent = q->sent();
	ASSERT_FALSE(sent.empty());
	EXPECT_EQ(sent.back(), dialogWindow(atFrigga, 8, 21281));
	EXPECT_TRUE(talk(*q, 21281, DialogAction::USE_OBJECT, frigga));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atFrigga, 8, 21281)}));

	Quester* other = makePlayer(811110, "Other", gameserver::model::Race::ASMODIANS, 50);
	Ref<QuestState> otherQs = hold(*other, 21281, QuestStatus::START);
	holdItem(*other, 811107, GREATER_ANCIENT_ICON, 1);
	EXPECT_TRUE(talk(*other, 21281, DialogAction::SELECT3, frigga));
	EXPECT_EQ(otherQs->getRewardGroup(), 2) << "category 2";
	EXPECT_EQ(otherQs->getQuestVarById(0), 3);
	sent = other->sent();
	ASSERT_FALSE(sent.empty());
	EXPECT_EQ(sent.back(), dialogWindow(atFrigga, 7, 21281));
}

// 15205 (FountainRewards.java:37-79): USE_OBJECT without the medal tells the player and answers true, with it shows the selection page;
// SETPRO1 with the medal starts the quest and sets REWARD at once (page 5); in REWARD an action other than the reward selection abandons
// the quest (the medal stays); another npc is not a fountain
TEST_F(CoinTemplatesTest, Quest15205TheFountainStartsRewardsOrAbandons) {
	registerXml(15205);
	Quester* q = makePlayer(811108, "Lucky", gameserver::model::Race::ELYOS, 55);
	Npc& fountain = npcOf(ORIEL_COIN_FOUNTAIN);
	Npc& luck = npcOf(FOUNTAIN_OF_LUCK);
	const int32_t atFountain = fountain.getObjectId();

	EXPECT_TRUE(talk(*q, 15205, DialogAction::USE_OBJECT, fountain));
	EXPECT_EQ(q->sent(), cp::exactly({q->serializedFor(SM_SYSTEM_MESSAGE::STR_QUEST_ACQUIRE_ERROR_INVENTORY_ITEM(l10nOf(PLATINUM_MEDAL)))}));
	EXPECT_TRUE(talk(*q, 15205, DialogAction::SETPRO1, fountain));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atFountain, 10, 0)})) << "no medal: the selection page";
	EXPECT_FALSE(q->player().getQuestStateList()->hasQuest(15205));

	holdItem(*q, 811109, PLATINUM_MEDAL, 1);
	EXPECT_TRUE(talk(*q, 15205, DialogAction::USE_OBJECT, luck));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(luck.getObjectId(), 10, 0)}));
	EXPECT_FALSE(talk(*q, 15205, DialogAction::QUEST_SELECT, fountain)) << "neither USE_OBJECT nor SETPRO1";
	EXPECT_FALSE(talk(*q, 15205, DialogAction::USE_OBJECT, npcOf(ELPAS))) << "not a fountain";

	EXPECT_TRUE(talk(*q, 15205, DialogAction::SETPRO1, fountain));
	Ptr<QuestState> qs = q->player().getQuestStateList()->getQuestState(15205);
	ASSERT_TRUE(qs);
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(held(*q, PLATINUM_MEDAL), 1) << "collectItemCheck(env, false) keeps the medal";
	EXPECT_EQ(q->sent(), cp::exactly({emptyQuestAction(), noNearbyQuests(), emptyQuestAction(), noNearbyQuests(), dialogWindow(atFountain, 5, 15205)}))
		<< "ADD, then UPDATE in REWARD, both empty for a COIN_QUEST";

	EXPECT_TRUE(talk(*q, 15205, DialogAction::USE_OBJECT, luck));
	EXPECT_FALSE(q->player().getQuestStateList()->hasQuest(15205)) << "abandoned (never completed: deleted)";
	EXPECT_EQ(held(*q, PLATINUM_MEDAL), 1);
	std::vector<std::vector<uint8_t>> sent = q->sent();
	EXPECT_EQ(sentOf(*q, SM_QUEST_ACTION_OPCODE).size(), 1u) << "SM_QUEST_ACTION(ABANDON)";
	ASSERT_FALSE(sent.empty());
	EXPECT_EQ(sent.back(), noNearbyQuests());
}

// 21281's finish (RelicRewards.java:93-99, QuestService.finishQuest and giveReward, E-09): SELECTED_QUEST_NOREWARD in REWARD, at a start npc
// only, finishes the quest with the reward group its category set - SELECT2's Ancient Icon, group 1, 600 AP - not multiplied by the quest
// AP rate for a NON_COUNT quest (QuestService.java:229-231; the rate is 2.0 here to tell). The finish sends the AP gain and the rank (E-09's
// AbyssPointsService.addAp), then the COIN_QUEST's empty update and the nearby quests, then the npc's selection page: the exchange can start
// again (sendQuestEndDialog, AbstractQuestHandler.java:437-457)
TEST_F(CoinTemplatesTest, Quest21281TheRewardSelectionPaysTheApOfItsCategory) {
	registerXml(21281);
	configs::main::RatesConfig::AP_QUEST_RATES.set({2.0f});
	Quester* q = makePlayer(811111, "Collector", gameserver::model::Race::ASMODIANS, 50);
	Npc& valith = npcOf(VALITH);
	Ref<QuestState> qs = hold(*q, 21281, QuestStatus::REWARD, 2); // as SELECT2 leaves it (RelicRewards.java:83-87): var 2, reward group 1
	qs->setRewardGroup(1);

	EXPECT_FALSE(talk(*q, 21281, DialogAction::SELECTED_QUEST_NOREWARD, npcOf(ELPAS))) << "not a start npc";
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_TRUE(talk(*q, 21281, DialogAction::SELECTED_QUEST_NOREWARD, valith));
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(qs->getCompleteCount(), 1);
	EXPECT_EQ(q->player().getAbyssRank()->getAp(), 600) << "<rewards ap=\"600\" ccheck=\"1\"/>, unrated";
	EXPECT_EQ(q->sent(), cp::exactly({q->serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_COMBAT_MY_ABYSS_POINT_GAIN(600)),
						 q->serializedFor(network::aion::serverpackets::SM_ABYSS_RANK(q->player(), std::nullopt)), emptyQuestAction(),
						 noNearbyQuests(), dialogWindow(valith.getObjectId(), 10, 0)}));
}

// 15205's finish (FountainRewards.java:68-73, QuestService.finishQuest, BonusService.getQuestBonus, E-09): SELECTED_QUEST_NOREWARD in REWARD
// without the medal does nothing; with it the medal is taken (collectItemCheck(env, true)) and the quest finishes, paying its MEDAL bonus of
// level 2: exactly one of the medal group's level-2 rows (item_groups.xml:4513-4517), a new stack
TEST_F(CoinTemplatesTest, Quest15205TheRewardSelectionTakesTheMedalAndPaysALevel2MedalBonus) {
	registerXml(15205);
	SeededRnd seeded(15205);
	Quester* q = makePlayer(811113, "Lucky", gameserver::model::Race::ELYOS, 55);
	Npc& luck = npcOf(FOUNTAIN_OF_LUCK);
	Ref<QuestState> qs = hold(*q, 15205, QuestStatus::REWARD);

	EXPECT_FALSE(talk(*q, 15205, DialogAction::SELECTED_QUEST_NOREWARD, luck)) << "no medal: collectItemCheck(env, true) fails";
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_TRUE(q->sent().empty());

	holdItem(*q, 811114, PLATINUM_MEDAL, 1);
	EXPECT_TRUE(talk(*q, 15205, DialogAction::SELECTED_QUEST_NOREWARD, luck));
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	EXPECT_FALSE(q->player().getInventory().getItemByObjId(811114)) << "the medal's stack is gone: the bonus is a new one";
	const std::vector<std::pair<int32_t, int64_t>> level2Rows{
		{RUSTED_MEDAL_2, 1}, {PLATINUM_MEDAL, 1}, {PLATINUM_MEDAL, 2}, {MITHRIL_MEDAL, 1}, {MITHRIL_MEDAL, 2}};
	const std::vector<std::pair<int32_t, int64_t>> bonus = heldOf(*q, {RUSTED_MEDAL_2, PLATINUM_MEDAL, MITHRIL_MEDAL});
	EXPECT_TRUE(bonus.size() == 1 && std::find(level2Rows.begin(), level2Rows.end(), bonus.front()) != level2Rows.end())
		<< "exactly one bonus, a level-2 row: " << describe(bonus);
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::test::templates
