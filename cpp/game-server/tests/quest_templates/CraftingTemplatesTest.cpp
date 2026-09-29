// P5-06c, M5d T-01b, T-02 and T-04 (m5d-plan.md §7, §18.3): the crafting_rewards and work_order kinds - CraftingRewardsData/WorkOrdersData
// .register_ building CraftingRewards/WorkOrders (CraftingRewards.java, WorkOrders.java), driven through QuestEngine on in-world players
// (QuestTemplate1bTestSupport.h):
// - 1941 "[Expert] Weaponsmithing Expert" (sanctum.xml:547): offered and started at anteros, reported at fasimedes, whose report teaches
//   Weaponsmithing 400 and plays movie 93; finished for 58283 exp and the Triumphant Helm. Both npcs answer only while the player can learn
//   another expert grade (CraftSkillUpdateService.canLearnMoreExpertCraftingSkill).
// - 3943 (sanctum.xml:540), 1941's pair without a movie; 19009 (sanctum.xml:554), a master grade (level_reward 500), whose npcs ask the
//   master limit (CraftSkillUpdateService.canLearnMoreMasterCraftingSkill) instead of the expert one.
// - 5000 "Steel Chisel Supplies" (work_order.xml:4-6): the accept window, the combine-task window, the accept giving 4 Issued Steel Ingots
//   (refused without Weaponsmithing: RecipeService.validateNewRecipe), the report with 3 Steel Chisels (REWARD, the ingots taken back), the
//   reward page after the chisels are taken, and USE_OBJECT's finish with a TASK bonus through E-09's BonusService.getQuestBonus (one row of the
//   craft groups for combineskill 40002 at combine_skillpoint 1, QuestTemplate1bTestSupport.h's T1B_ITEM_GROUPS_XML) and page 1008.
// Registrations and rewards as `oracle.py m5d-quest --no-profile --quest 1941` (and 3943, 19009, 5000) print them.

#include "QuestTemplate1bTestSupport.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/RecipeList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test::templates {
namespace {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::gameobjects::Npc;

class CraftingTemplatesTest : public QuestTemplate1bTest {};

// CraftingRewardsData.register (CraftingRewardsData.java:28-30) and CraftingRewards.register (CraftingRewards.java:36-45): the start npc starts
// and talks, the end npc (another one) talks
TEST_F(CraftingTemplatesTest, CraftingRewardsRegistersItsStartAndEndNpcs) {
	registerXml(1941);
	EXPECT_TRUE(QuestEngine::getInstance().isHaveHandler(1941));
	EXPECT_EQ(startQuests(ANTEROS), (std::vector<int32_t>{1941}));
	EXPECT_EQ(talkQuests(ANTEROS), (std::vector<int32_t>{1941}));
	EXPECT_EQ(talkQuests(FASIMEDES), (std::vector<int32_t>{1941}));
	EXPECT_EQ(startQuests(FASIMEDES), (std::vector<int32_t>{})) << "the end npc starts nothing";
}

// 1941 (CraftingRewards.java:47-81): anteros offers (1011) and starts it; in START fasimedes shows 2375 and SELECT_QUEST_REWARD sets var 0 and
// REWARD, adds Weaponsmithing at 400, plays movie 93 and shows the reward page; in REWARD fasimedes finishes it
TEST_F(CraftingTemplatesTest, Quest1941TeachesTheExpertGradeAtItsEndNpc) {
	registerXml(1941);
	Quester* q = makePlayer(810701, "Smith", gameserver::model::Race::ELYOS, 29);
	knowsWeaponsmithing(*q, 399);
	hold(*q, 1973, QuestStatus::COMPLETE); // 1941's <finished quest_id="1973"/>
	Npc& anteros = npcOf(ANTEROS);
	Npc& fasimedes = npcOf(FASIMEDES);
	const int32_t atEnd = fasimedes.getObjectId();

	EXPECT_TRUE(talk(*q, 1941, DialogAction::QUEST_SELECT, anteros));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(anteros.getObjectId(), 1011, 1941)}));
	EXPECT_FALSE(talk(*q, 1941, DialogAction::QUEST_SELECT, fasimedes)) << "fasimedes is no start npc";
	EXPECT_TRUE(q->sent().empty());

	EXPECT_TRUE(talk(*q, 1941, DialogAction::QUEST_ACCEPT_1, anteros));
	Ptr<QuestState> qs = q->player().getQuestStateList()->getQuestState(1941);
	ASSERT_TRUE(qs);
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_EQ(q->sent(), cp::exactly({questAction(1, 1941, START), noNearbyQuests(), dialogWindow(anteros.getObjectId(), 1003, 1941)}));

	EXPECT_FALSE(talk(*q, 1941, DialogAction::QUEST_SELECT, anteros)) << "in START anteros is no end npc";
	EXPECT_TRUE(q->sent().empty());
	EXPECT_TRUE(talk(*q, 1941, DialogAction::QUEST_SELECT, fasimedes));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atEnd, 2375, 1941)}));
	EXPECT_FALSE(talk(*q, 1941, DialogAction::USE_OBJECT, fasimedes)) << "an action outside the START switch";
	EXPECT_EQ(q->player().getSkillList()->getSkillLevel(WEAPONSMITHING), 399);

	qs->setQuestVar(3);
	EXPECT_TRUE(talk(*q, 1941, DialogAction::SELECT_QUEST_REWARD, fasimedes));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(qs->getQuestVarById(0), 0) << "setQuestVar(0)";
	EXPECT_EQ(q->player().getSkillList()->getSkillLevel(WEAPONSMITHING), 400);
	EXPECT_EQ(sentOf(*q, SM_QUEST_ACTION_OPCODE), cp::exactly({questUpdate(1941, REWARD, 0)}));
	EXPECT_EQ(sentOf(*q, SM_PLAY_MOVIE_OPCODE), cp::exactly({playMovie(false, atEnd, 1941, 93)}));
	std::vector<std::vector<uint8_t>> sent = q->sent();
	ASSERT_FALSE(sent.empty());
	EXPECT_EQ(sent.back(), dialogWindow(atEnd, 5, 1941));

	EXPECT_FALSE(talk(*q, 1941, DialogAction::USE_OBJECT, anteros)) << "in REWARD only the end npc answers";
	EXPECT_TRUE(talk(*q, 1941, DialogAction::USE_OBJECT, fasimedes)) << "REWARD answers whether or not the player can learn more";
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atEnd, 5, 1941)}));
	EXPECT_TRUE(talk(*q, 1941, DialogAction::SELECTED_QUEST_NOREWARD, fasimedes));
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(held(*q, TRIUMPHANT_HELM), 1);
	EXPECT_EQ(held(*q, EXPERT_DAGGER_DESIGN), 0) << "no selectable item without SELECTED_QUEST_REWARDn";
}

// canLearn (CraftingRewards.java:83-89): at the limit of expert grades the start npc and the end npc (in START) do not answer, and the player
// is told the limit; the REWARD branch does not ask
TEST_F(CraftingTemplatesTest, AtTheExpertLimitNeitherNpcAnswers) {
	registerXml(1941);
	Quester* q = makePlayer(810702, "Limited", gameserver::model::Race::ELYOS, 29);
	knowsWeaponsmithing(*q, 399);
	hold(*q, 1973, QuestStatus::COMPLETE);
	Npc& anteros = npcOf(ANTEROS);
	Npc& fasimedes = npcOf(FASIMEDES);
	configs::main::CraftConfig::MAX_EXPERT_CRAFTING_SKILLS.store(0);
	const std::vector<uint8_t> limit = q->serializedFor(network::aion::serverpackets::SM_MESSAGE(0, "",
		"You can only be an expert in 0 professions.", gameserver::model::ChatType::GOLDEN_YELLOW));

	EXPECT_FALSE(talk(*q, 1941, DialogAction::QUEST_SELECT, anteros));
	EXPECT_EQ(q->sent(), cp::exactly({limit}));
	EXPECT_FALSE(talk(*q, 1941, DialogAction::QUEST_ACCEPT_1, anteros));
	EXPECT_FALSE(q->player().getQuestStateList()->hasQuest(1941));

	Ref<QuestState> qs = hold(*q, 1941, QuestStatus::START);
	EXPECT_FALSE(talk(*q, 1941, DialogAction::SELECT_QUEST_REWARD, fasimedes));
	EXPECT_EQ(q->sent(), cp::exactly({limit}));
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_EQ(q->player().getSkillList()->getSkillLevel(WEAPONSMITHING), 399);

	qs->setStatus(QuestStatus::REWARD);
	EXPECT_TRUE(talk(*q, 1941, DialogAction::USE_OBJECT, fasimedes));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(fasimedes.getObjectId(), 5, 1941)}));
}

// WorkOrdersData.register (WorkOrdersData.java:43-45) and WorkOrders.register (WorkOrders.java:40-45): every start npc starts and talks
TEST_F(CraftingTemplatesTest, WorkOrdersRegistersItsStartNpcs) {
	registerXml(5000);
	for (int32_t npcId : {ANTEROS, AUMINUS}) {
		EXPECT_EQ(startQuests(npcId), (std::vector<int32_t>{5000})) << npcId;
		EXPECT_EQ(talkQuests(npcId), (std::vector<int32_t>{5000})) << npcId;
	}
}

// 5000 up to its reward page (WorkOrders.java:47-103): the accept window (4) and the combine-task window (28, quest 0); the accept gives the
// 4 components (the recipe goes through PlayerRecipesDAO, which the unit tests have no database for); in START the report without the 3
// chisels shows the selection page, with them it sets REWARD, takes the components back (removeQuestWorkItems) and shows page 5; in REWARD
// any action first takes the chisels, and a non-finishing action shows the reward page
TEST_F(CraftingTemplatesTest, Quest5000FromTheAcceptToTheRewardPage) {
	registerXml(5000);
	Quester* q = makePlayer(810703, "Chiseler", gameserver::model::Race::ELYOS, 10);
	knowsWeaponsmithing(*q, 1); // 5000's combineskill 40002, combine_skillpoint 1
	Npc& anteros = npcOf(ANTEROS);
	Npc& auminus = npcOf(AUMINUS);
	Npc& elpas = npcOf(ELPAS);
	const int32_t atStart = anteros.getObjectId();

	EXPECT_TRUE(talk(*q, 5000, DialogAction::QUEST_SELECT, anteros));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atStart, 4, 5000)}));
	EXPECT_TRUE(talk(*q, 5000, DialogAction::COMBINE_TASK, auminus));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(auminus.getObjectId(), 28, 0)})) << "the quest id is cleared first";
	EXPECT_FALSE(talk(*q, 5000, DialogAction::USE_OBJECT, anteros)) << "an action outside the switch";
	EXPECT_FALSE(talk(*q, 5000, DialogAction::QUEST_SELECT, elpas)) << "not a start npc";
	EXPECT_TRUE(q->sent().empty());

	EXPECT_TRUE(talk(*q, 5000, DialogAction::QUEST_ACCEPT_1, anteros));
	Ptr<QuestState> qs = q->player().getQuestStateList()->getQuestState(5000);
	ASSERT_TRUE(qs);
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_EQ(held(*q, ISSUED_STEEL_INGOT), 4);
	std::vector<std::vector<uint8_t>> sent = q->sent();
	ASSERT_FALSE(sent.empty());
	EXPECT_EQ(sent.front(), questAction(1, 5000, START));
	EXPECT_EQ(sent.back(), dialogWindow(atStart, 0, 0)) << "closeDialogWindow";

	EXPECT_TRUE(talk(*q, 5000, DialogAction::QUEST_SELECT, auminus));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(auminus.getObjectId(), 10, 0)})) << "no chisels: the selection page";
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_FALSE(talk(*q, 5000, DialogAction::USE_OBJECT, auminus)) << "only QUEST_SELECT in START";

	holdItem(*q, 820701, STEEL_CHISEL, 3);
	EXPECT_TRUE(talk(*q, 5000, DialogAction::QUEST_SELECT, auminus));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(held(*q, ISSUED_STEEL_INGOT), 0) << "removeQuestWorkItems";
	EXPECT_EQ(held(*q, STEEL_CHISEL), 3) << "collectItemCheck(env, false) keeps the chisels";
	EXPECT_EQ(sentOf(*q, SM_QUEST_ACTION_OPCODE), cp::exactly({questUpdate(5000, REWARD, 0)}));
	sent = q->sent();
	ASSERT_FALSE(sent.empty());
	EXPECT_EQ(sent.back(), dialogWindow(auminus.getObjectId(), 5, 5000));

	EXPECT_TRUE(talk(*q, 5000, DialogAction::QUEST_SELECT, anteros));
	EXPECT_EQ(held(*q, STEEL_CHISEL), 0) << "REWARD takes every collect item first";
	sent = q->sent();
	ASSERT_FALSE(sent.empty());
	EXPECT_EQ(sent.back(), dialogWindow(atStart, 5, 5000));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_FALSE(talk(*q, 5000, DialogAction::QUEST_SELECT, elpas));
}

// 3943 (CraftingRewards.java:67-72 with movie 0): SELECT_QUEST_REWARD in START teaches the grade and shows the reward page, and plays no movie
TEST_F(CraftingTemplatesTest, Quest3943WithoutAMovieTeachesTheGradeAndPlaysNone) {
	registerXml(3943);
	Quester* q = makePlayer(810704, "Smith", gameserver::model::Race::ELYOS, 29);
	knowsWeaponsmithing(*q, 399);
	Npc& fasimedes = npcOf(FASIMEDES);
	Ref<QuestState> qs = hold(*q, 3943, QuestStatus::START);
	EXPECT_TRUE(talk(*q, 3943, DialogAction::SELECT_QUEST_REWARD, fasimedes));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(q->player().getSkillList()->getSkillLevel(WEAPONSMITHING), 400);
	EXPECT_EQ(sentOf(*q, SM_PLAY_MOVIE_OPCODE), cp::exactly({})) << "no movie attribute";
	EXPECT_EQ(lastSent(*q), dialogWindow(fasimedes.getObjectId(), 5, 3943));
}

// 19009 (CraftingRewards.java:83-89, level_reward 500): anteros offers it only while the player can learn another master grade - refused at
// MAX_MASTER_CRAFTING_SKILLS 0 with the master-limit message, offered at 1 although the expert limit (1) is reached; in START eremitia's
// SELECT_QUEST_REWARD teaches Weaponsmithing 500 and plays movie 109
TEST_F(CraftingTemplatesTest, Quest19009TheMasterGradeAsksTheMasterLimit) {
	registerXml(19009);
	EXPECT_EQ(startQuests(ANTEROS), (std::vector<int32_t>{19009}));
	EXPECT_EQ(talkQuests(EREMITIA), (std::vector<int32_t>{19009}));
	Quester* q = makePlayer(810705, "Master", gameserver::model::Race::ELYOS, 29);
	knowsWeaponsmithing(*q, 499); // one expert grade
	Npc& anteros = npcOf(ANTEROS);
	Npc& eremitia = npcOf(EREMITIA);

	configs::main::CraftConfig::MAX_MASTER_CRAFTING_SKILLS.store(0);
	EXPECT_FALSE(talk(*q, 19009, DialogAction::QUEST_SELECT, anteros));
	EXPECT_EQ(q->sent(), cp::exactly({q->serializedFor(network::aion::serverpackets::SM_MESSAGE(0, "",
							 "You can only be a master in 0 professions.", gameserver::model::ChatType::GOLDEN_YELLOW))}));

	configs::main::CraftConfig::MAX_MASTER_CRAFTING_SKILLS.store(1);
	configs::main::CraftConfig::MAX_EXPERT_CRAFTING_SKILLS.store(1);
	EXPECT_TRUE(talk(*q, 19009, DialogAction::QUEST_SELECT, anteros));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(anteros.getObjectId(), 1011, 19009)}));

	Ref<QuestState> qs = hold(*q, 19009, QuestStatus::START);
	EXPECT_TRUE(talk(*q, 19009, DialogAction::QUEST_SELECT, eremitia));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(eremitia.getObjectId(), 2375, 19009)}));
	EXPECT_TRUE(talk(*q, 19009, DialogAction::SELECT_QUEST_REWARD, eremitia));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(q->player().getSkillList()->getSkillLevel(WEAPONSMITHING), 500);
	EXPECT_EQ(sentOf(*q, SM_PLAY_MOVIE_OPCODE), cp::exactly({playMovie(false, eremitia.getObjectId(), 19009, 109)}));
}

// 5000's accept without Weaponsmithing (WorkOrders.java:62-72, RecipeService.validateNewRecipe): the recipe cannot be learned, so the player
// is told and QuestService.startQuest is not asked (it would refuse the start as well, and tell its own message: checkCombineSkill,
// QuestService.java:379-380, so the quest and the ingots are not what this case can tell apart)
TEST_F(CraftingTemplatesTest, Quest5000IsNotStartedWithoutItsCraftingSkill) {
	registerXml(5000);
	Quester* q = makePlayer(810706, "Unskilled", gameserver::model::Race::ELYOS, 10);
	Npc& anteros = npcOf(ANTEROS);
	EXPECT_FALSE(talk(*q, 5000, DialogAction::QUEST_ACCEPT_1, anteros));
	const std::string weaponsmithing = dataholders::DataManager::SKILL_DATA->getSkillTemplate(WEAPONSMITHING)->getL10n();
	EXPECT_EQ(q->sent(), cp::exactly({q->serializedFor(
							 network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_CRAFT_RECIPE_CANT_LEARN_SKILL(weaponsmithing))}));
}

// 5000's finish (WorkOrders.java:90-99, QuestService.finishQuest, BonusService.getQuestBonus, E-09): USE_OBJECT in REWARD at a start npc takes
// the chisels, finishes the quest and shows page 1008 for it; the empty <rewards/> pays nothing and the TASK bonus exactly one row of the craft
// groups an Elyos of combine_skillpoint 1 matches (item_groups.xml:387, 392, 541, 2237)
TEST_F(CraftingTemplatesTest, Quest5000UseObjectInRewardFinishesItWithATaskBonus) {
	registerXml(5000);
	SeededRnd seeded(5000);
	Quester* q = makePlayer(810707, "Chiseler", gameserver::model::Race::ELYOS, 10);
	knowsWeaponsmithing(*q, 1);
	Npc& auminus = npcOf(AUMINUS);
	Ref<QuestState> qs = hold(*q, 5000, QuestStatus::REWARD);
	holdItem(*q, 820702, STEEL_CHISEL, 3);

	EXPECT_TRUE(talk(*q, 5000, DialogAction::USE_OBJECT, auminus));
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(held(*q, STEEL_CHISEL), 0);
	const std::vector<std::pair<int32_t, int64_t>> bonus =
		heldOf(*q, {CHARCOAL_BRIQUETTE, LESSER_WHETSTONE, STEEL_INGOT_BUNDLE_DESIGN, STEEL_INGOT_DESIGN});
	// craft_shop's rows are CraftItems, Rnd.get(3, 5) of them (CraftItem.java:55-57); a CraftRecipe row (bundle, recipe) is one
	auto isTaskRow = [](const std::pair<int32_t, int64_t>& row) {
		if (row.first == CHARCOAL_BRIQUETTE || row.first == LESSER_WHETSTONE)
			return row.second >= 3 && row.second <= 5;
		return row.second == 1;
	};
	EXPECT_TRUE(bonus.size() == 1 && isTaskRow(bonus.front())) << "exactly one bonus, a row of the TASK groups: " << describe(bonus);
	EXPECT_EQ(sentOf(*q, SM_QUEST_ACTION_OPCODE), cp::exactly({questUpdate(5000, COMPLETE, 0)}));
	EXPECT_EQ(lastSent(*q), dialogWindow(auminus.getObjectId(), 1008, 5000));
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::test::templates
