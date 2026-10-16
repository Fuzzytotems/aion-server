// M5c C-01 / C-06 (m5c-plan.md §5, P5-09c): CraftSkillUpdateService - getProfessionByNpc, learnSkill's price table, its refusals and its
// question handler (CraftSkillUpdateService$1), canLearnMoreExpert/MasterCraftingSkill - and the Profession companion's six functions
// (ProfessionInfo.h, header request m5c-h04). The fixture and its rules are CraftTestSupport.h's.
//
// Java: CraftSkillUpdateService.java:79-175, Profession.java:37-87. Expectations: `oracle.py m5c-craft --no-profile --set
// gameserver.event.service.disabled_events=* --skill ID --level N` (its upgrade block: cost, newLevel, refusal, costTable,
// maxUpgradableLevel, masters, minCharacterLevel) where named, else the Java lines beside each assertion.

#include "CraftTestSupport.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/craft/Profession.h"
#include "aion/gameserver/model/craft/ProfessionInfo.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEARN_RECIPE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/craft/CraftSkillUpdateService.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"

namespace aion::gameserver::economy::test::craft {
namespace {

using model::craft::Profession;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using services::craft::CraftSkillUpdateService;

constexpr int32_t STR_CRAFT_ADDSKILL_CONFIRM = 900852; // SM_QUESTION_WINDOW.java

class CraftSkillUpdateServiceTest : public CraftTest {
protected:
	CraftSkillUpdateService& service() { return CraftSkillUpdateService::getInstance(); }

	void learn(int32_t npcId) { service().learnSkill(player(), npc(npcId)); }

	/**
	 * SM_QUESTION_WINDOW(STR_CRAFT_ADDSKILL_CONFIRM, 0, 0, name, price) as Java writes it (SM_QUESTION_WINDOW.java writeImpl): D(code), three
	 * writeS (the third null), D(0), C(0: no range), D(senderId 0), D(0)
	 */
	std::vector<uint8_t> addSkillQuestion(std::string_view professionName, int64_t price) {
		return itemtest::javaPacket(SM_QUESTION_WINDOW_OPCODE,
			PacketWriter().D(STR_CRAFT_ADDSKILL_CONFIRM).S(professionName).S(std::to_string(price)).H(0).D(0).C(0).D(0).D(0));
	}

	/** Java: profession.getClientName(level) - the grade's client string, a space, the skill's */
	static std::string gradedName(int32_t grade, int32_t skillName) { return javaL10n(grade) + " " + javaL10n(skillName); }

	std::vector<uint8_t> goldenMessage(std::string_view text) {
		return serializedFor(network::aion::serverpackets::SM_MESSAGE(0, "", text, model::ChatType::GOLDEN_YELLOW));
	}

	bool sentContains(const std::vector<uint8_t>& packet) {
		for (const std::vector<uint8_t>& each : sent())
			if (each == packet)
				return true;
		return false;
	}
};

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Profession (Profession.java:37-87)
// ---------------------------------------------------------------------------------------------------------------------------------------------

// :37-51 (--skill 40001 / 30002 --level 0: costTable): the five priced levels, the artisan grade (449) only for a crafting profession, null
// for every other level
TEST(ProfessionInfoTest, TheUpgradeCostTable) {
	using model::craft::getUpgradeCost;
	for (Profession profession : model::craft::PROFESSION_VALUES) {
		SCOPED_TRACE(model::craft::getSkillId(profession));
		EXPECT_EQ(getUpgradeCost(profession, 0), 3500);
		EXPECT_EQ(getUpgradeCost(profession, 99), 17000);
		EXPECT_EQ(getUpgradeCost(profession, 199), 115000);
		EXPECT_EQ(getUpgradeCost(profession, 299), 460000);
		EXPECT_EQ(getUpgradeCost(profession, 449), model::craft::isCrafting(profession) ? std::optional<int32_t>(6004900) : std::nullopt);
		for (int32_t level : {-1, 1, 98, 100, 198, 200, 298, 300, 399, 448, 450, 499, 500})
			EXPECT_EQ(getUpgradeCost(profession, level), std::nullopt) << level;
	}
}

// :53-55: 499 for a crafting profession, 399 for essence- and aethertapping
TEST(ProfessionInfoTest, TheMaxUpgradableLevel) {
	EXPECT_EQ(model::craft::getMaxUpgradableLevel(Profession::ESSENCETAPPING), 399);
	EXPECT_EQ(model::craft::getMaxUpgradableLevel(Profession::AETHERTAPPING), 399);
	for (Profession profession : {Profession::COOKING, Profession::WEAPONSMITHING, Profession::ARMORSMITHING, Profession::TAILORING,
			 Profession::ALCHEMY, Profession::HANDICRAFTING, Profession::CONSTRUCTION})
		EXPECT_EQ(model::craft::getMaxUpgradableLevel(profession), 499) << model::craft::getSkillId(profession);
}

// :81-87: the first constant of the skill id, null for the skills Profession does not list (human gathering 30001, the commented out
// leatherwork 40005 and carpentry 40006, the morph skill)
TEST(ProfessionInfoTest, GetBySkillIdFindsEachProfession) {
	for (Profession profession : model::craft::PROFESSION_VALUES)
		EXPECT_EQ(model::craft::getBySkillId(model::craft::getSkillId(profession)), profession) << model::craft::getSkillId(profession);
	for (int32_t skillId : {0, 30001, 40005, 40006, 40009, 40011})
		EXPECT_EQ(model::craft::getBySkillId(skillId), std::nullopt) << skillId;
}

// :57-79: the name is the skill template's l10n; with a level, the grade's client string before it - Amateur up to 99, Novice to 199, Apprentice
// to 299, Journeyman to 399, Expert to 449, then Artisan to 499 for a crafting profession, and Master above (tapping: Master from 450)
TEST_F(CraftSkillUpdateServiceTest, TheClientNamesAndTheSkillGrades) {
	EXPECT_EQ(model::craft::getClientName(Profession::COOKING), javaL10n(COOKING_NAME));
	EXPECT_EQ(model::craft::getClientName(Profession::ESSENCETAPPING), javaL10n(ESSENCETAPPING_NAME));
	EXPECT_EQ(model::craft::getClientName(Profession::CONSTRUCTION), javaL10n(CONSTRUCTION_NAME));
	struct Row {
		int32_t level;
		int32_t crafting;
		int32_t tapping;
	};
	const Row rows[] = {{0, AMATEUR, AMATEUR}, {99, AMATEUR, AMATEUR}, {100, NOVICE, NOVICE}, {199, NOVICE, NOVICE}, {200, APPRENTICE, APPRENTICE},
		{299, APPRENTICE, APPRENTICE}, {300, JOURNEYMAN, JOURNEYMAN}, {399, JOURNEYMAN, JOURNEYMAN}, {400, EXPERT, EXPERT}, {449, EXPERT, EXPERT},
		{450, ARTISAN, MASTER}, {499, ARTISAN, MASTER}, {500, MASTER, MASTER}};
	for (const Row& row : rows) {
		SCOPED_TRACE(row.level);
		EXPECT_EQ(model::craft::getSkillGrade(Profession::COOKING, row.level), javaL10n(row.crafting));
		EXPECT_EQ(model::craft::getSkillGrade(Profession::AETHERTAPPING, row.level), javaL10n(row.tapping));
		EXPECT_EQ(model::craft::getClientName(Profession::COOKING, row.level), gradedName(row.crafting, COOKING_NAME));
		EXPECT_EQ(model::craft::getClientName(Profession::AETHERTAPPING, row.level), gradedName(row.tapping, AETHERTAPPING_NAME));
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// CraftSkillUpdateService (CraftSkillUpdateService.java:79-175)
// ---------------------------------------------------------------------------------------------------------------------------------------------

// :79-81 with the constructor's map (:34-74; --skill 40001: masters 203784, 204100, 830058, 830142; --skill 30002: 203780, ...)
TEST_F(CraftSkillUpdateServiceTest, GetProfessionByNpcAnswersTheMastersProfession) {
	EXPECT_EQ(service().getProfessionByNpc(npc(HESTIA)), Profession::COOKING);
	EXPECT_EQ(service().getProfessionByNpc(npc(LAINITA)), Profession::COOKING);
	EXPECT_EQ(service().getProfessionByNpc(npc(CORNELIUS)), Profession::ESSENCETAPPING);
	EXPECT_EQ(service().getProfessionByNpc(npc(LUELAS)), std::nullopt);
}

// :93-126 (--skill 40001 / 30002 --level N: upgrade.cost): the question names the profession (the grade of the next level from 99 on) and the
// price as a string
TEST_F(CraftSkillUpdateServiceTest, LearnSkillAsksForThePriceOfTheNextLevel) {
	setDaevaLevel(10);
	struct Row {
		int32_t npcId;
		int32_t skillId;
		int32_t level; // 0: the skill is not learned
		int64_t price;
		std::string name;
	};
	const Row rows[] = {
		{HESTIA, COOKING, 0, 3500, javaL10n(COOKING_NAME)},
		{HESTIA, COOKING, 99, 17000, gradedName(NOVICE, COOKING_NAME)},
		{HESTIA, COOKING, 199, 115000, gradedName(APPRENTICE, COOKING_NAME)},
		{HESTIA, COOKING, 299, 460000, gradedName(JOURNEYMAN, COOKING_NAME)},
		{HESTIA, COOKING, 449, 6004900, gradedName(ARTISAN, COOKING_NAME)},
		{LAINITA, COOKING, 0, 3500, javaL10n(COOKING_NAME)},
		{CORNELIUS, ESSENCETAPPING, 0, 3500, javaL10n(ESSENCETAPPING_NAME)},
		{CORNELIUS, ESSENCETAPPING, 99, 17000, gradedName(NOVICE, ESSENCETAPPING_NAME)},
		{CORNELIUS, ESSENCETAPPING, 299, 460000, gradedName(JOURNEYMAN, ESSENCETAPPING_NAME)},
	};
	for (const Row& row : rows) {
		SCOPED_TRACE(std::to_string(row.npcId) + " at " + std::to_string(row.level));
		if (row.level == 0)
			setSkills({});
		else
			setSkills({{row.skillId, row.level}});
		clearSent();
		learn(row.npcId);
		EXPECT_EQ(sent(), cp::exactly({addSkillQuestion(row.name, row.price)}));
		player().getResponseRequester().denyAll(); // the next row asks again
	}
	// a skill learned at level 0 asks like no skill (:94: getSkillLevel is 0 either way)
	setSkills({{COOKING, 0}});
	clearSent();
	learn(HESTIA);
	EXPECT_EQ(sent(), cp::exactly({addSkillQuestion(javaL10n(COOKING_NAME), 3500)}));
	player().getResponseRequester().denyAll();
}

// :96-106 (--skill 40001 / 30002 --level N: upgrade.refusal): a level without a price - above the profession's max upgradable level, the two
// grades granted by quest (399, 499), any other
TEST_F(CraftSkillUpdateServiceTest, LearnSkillRefusesTheLevelsWithoutAPrice) {
	setDaevaLevel(10);
	struct Row {
		int32_t npcId;
		int32_t skillId;
		int32_t level;
		SM_SYSTEM_MESSAGE (*message)();
	};
	const Row rows[] = {
		{HESTIA, COOKING, 1, &SM_SYSTEM_MESSAGE::STR_MSG_DONT_RANK_UP},
		{HESTIA, COOKING, 98, &SM_SYSTEM_MESSAGE::STR_MSG_DONT_RANK_UP},
		{HESTIA, COOKING, 100, &SM_SYSTEM_MESSAGE::STR_MSG_DONT_RANK_UP},
		{HESTIA, COOKING, 399, &SM_SYSTEM_MESSAGE::STR_CRAFT_CANT_EXTEND_MONEY},
		{HESTIA, COOKING, 400, &SM_SYSTEM_MESSAGE::STR_MSG_DONT_RANK_UP},
		{HESTIA, COOKING, 450, &SM_SYSTEM_MESSAGE::STR_MSG_DONT_RANK_UP},
		{HESTIA, COOKING, 499, &SM_SYSTEM_MESSAGE::STR_CRAFT_CANT_EXTEND_GRAND_MASTER},
		{HESTIA, COOKING, 500, &SM_SYSTEM_MESSAGE::STR_MSG_DONT_RANK_UP_GATHERING},
		{CORNELIUS, ESSENCETAPPING, 1, &SM_SYSTEM_MESSAGE::STR_MSG_DONT_RANK_UP},
		{CORNELIUS, ESSENCETAPPING, 399, &SM_SYSTEM_MESSAGE::STR_CRAFT_CANT_EXTEND_MONEY},
		{CORNELIUS, ESSENCETAPPING, 400, &SM_SYSTEM_MESSAGE::STR_MSG_DONT_RANK_UP_GATHERING},
		{CORNELIUS, ESSENCETAPPING, 449, &SM_SYSTEM_MESSAGE::STR_MSG_DONT_RANK_UP_GATHERING},
	};
	for (const Row& row : rows) {
		SCOPED_TRACE(std::to_string(row.skillId) + " at " + std::to_string(row.level));
		setSkills({{row.skillId, row.level}});
		clearSent();
		learn(row.npcId);
		EXPECT_EQ(sent(), cp::exactly({systemMessage(row.message())}));
	}
}

// :84-88 (--skill 40001: upgrade.minCharacterLevel 10): nothing below level 10, nothing from an npc that teaches no profession
TEST_F(CraftSkillUpdateServiceTest, LearnSkillAsksNothingBelowLevelTenOrOfANpcThatTeachesNothing) {
	commonData().setLevel(9);
	learn(HESTIA);
	EXPECT_TRUE(sent().empty());

	setDaevaLevel(10);
	learn(LUELAS);
	EXPECT_TRUE(sent().empty());
	learn(HESTIA);
	EXPECT_EQ(sent(), cp::exactly({addSkillQuestion(javaL10n(COOKING_NAME), 3500)}));
}

// :108-120 (CraftSkillUpdateService$1): yes pays the price and adds the skill at the next level - a new skill announces itself with 1330061, a
// raised one with 1330064 (SkillLearnService.sendPacket). The kinah leaves as ItemUpdateType.DEC_KINAH_LEARN (:112): the kinah stack's
// SM_INVENTORY_UPDATE_ITEM closes with the mask 0x49 (ItemPacketService.java:47; Storage.decreaseKinah -> sendItemPacket)
TEST_F(CraftSkillUpdateServiceTest, YesPaysThePriceAndLearnsTheNextLevel) {
	setDaevaLevel(10);
	give(820101, KINAH, 3500);
	learn(HESTIA);
	clearSent();
	ASSERT_TRUE(player().getResponseRequester().respond(STR_CRAFT_ADDSKILL_CONFIRM, 1));
	EXPECT_EQ(kinah(), 0);
	const std::vector<std::vector<uint8_t>> kinahUpdates = itemtest::packetsOf(sent(), itemtest::SM_INVENTORY_UPDATE_ITEM_OPCODE);
	EXPECT_EQ(kinahUpdates, cp::exactly({serializedFor(network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM(player(),
								*player().getInventory().getKinahItem(), services::item::ItemPacketService_ItemUpdateType::DEC_KINAH_LEARN))}));
	EXPECT_EQ(kinahUpdates.empty() ? -1 : closingH(kinahUpdates.front()), 0x49);
	ASSERT_TRUE(player().getSkillList()->isSkillPresent(COOKING));
	EXPECT_EQ(skill(COOKING).getSkillLevel(), 1);
	EXPECT_TRUE(sentContains(serializedFor(network::aion::serverpackets::SM_SKILL_LIST(skill(COOKING), 1330061))));
	EXPECT_TRUE(itemtest::packetsOf(sent(), SM_LEARN_RECIPE_OPCODE).empty()) << "no database: RecipeList.addRecipe learns nothing";

	setSkills({{COOKING, 99}});
	give(820102, KINAH, 17000);
	learn(HESTIA);
	clearSent();
	ASSERT_TRUE(player().getResponseRequester().respond(STR_CRAFT_ADDSKILL_CONFIRM, 1));
	EXPECT_EQ(kinah(), 0);
	EXPECT_EQ(skill(COOKING).getSkillLevel(), 100);
	EXPECT_TRUE(sentContains(serializedFor(network::aion::serverpackets::SM_SKILL_LIST(skill(COOKING), 1330064))));
}

// :112-117: yes without the price in kinah is STR_NOT_ENOUGH_MONEY and changes nothing; no is nothing at all (RequestResponseHandler's
// denyRequest)
TEST_F(CraftSkillUpdateServiceTest, YesWithoutThePriceOrNoLearnsNothing) {
	setDaevaLevel(10);
	give(820111, KINAH, 3499);
	learn(HESTIA);
	clearSent();
	ASSERT_TRUE(player().getResponseRequester().respond(STR_CRAFT_ADDSKILL_CONFIRM, 1));
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_NOT_ENOUGH_MONEY())}));
	EXPECT_EQ(kinah(), 3499);
	EXPECT_FALSE(player().getSkillList()->isSkillPresent(COOKING));

	learn(HESTIA);
	clearSent();
	ASSERT_TRUE(player().getResponseRequester().respond(STR_CRAFT_ADDSKILL_CONFIRM, 0));
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(kinah(), 3499);
	EXPECT_FALSE(player().getSkillList()->isSkillPresent(COOKING));
}

// the same with exactly the price: yes succeeds (Storage.tryDecreaseKinah's `>=`, the gate's X17 exact-kinah purchase is its sibling)
TEST_F(CraftSkillUpdateServiceTest, YesWithExactlyThePriceLearns) {
	setDaevaLevel(10);
	setSkills({{COOKING, 199}});
	give(820113, KINAH, 115000);
	learn(HESTIA);
	clearSent();
	ASSERT_TRUE(player().getResponseRequester().respond(STR_CRAFT_ADDSKILL_CONFIRM, 1));
	EXPECT_EQ(kinah(), 0);
	EXPECT_EQ(skill(COOKING).getSkillLevel(), 200);
}

// :122: ResponseRequester.putRequest refuses a second question of the same id while the first is open, so no second window is sent
TEST_F(CraftSkillUpdateServiceTest, AnOpenQuestionIsNotAskedTwice) {
	setDaevaLevel(10);
	learn(HESTIA);
	learn(HESTIA);
	EXPECT_EQ(sent(), cp::exactly({addSkillQuestion(javaL10n(COOKING_NAME), 3500)}));
	player().getResponseRequester().denyAll();
}

// the gate's X17 (m5c-plan.md §10.3) at the unit level: learning cooking at its master learns exactly the race's autolearn recipe of level 1
// (addSkill -> SkillLearnService.onLearnSkill -> RecipeService.autoLearnRecipes; --skill 40001 --level 1: recipes.ELYOS / ASMODIANS)
TEST_F(CraftSkillUpdateServiceTest, LearningCookingLearnsTheRacesAutolearnRecipe) {
	CRAFT_REQUIRE_DATABASE();
	setDaevaLevel(10);
	give(820121, KINAH, 3500);
	learn(HESTIA);
	clearSent();
	ASSERT_TRUE(player().getResponseRequester().respond(STR_CRAFT_ADDSKILL_CONFIRM, 1));
	EXPECT_EQ(itemtest::packetsOf(sent(), SM_LEARN_RECIPE_OPCODE),
		cp::exactly({serializedFor(network::aion::serverpackets::SM_LEARN_RECIPE(ROAST_ININA_RECIPE))}));
	EXPECT_EQ(storedRecipes(), (std::vector<int32_t>{ROAST_ININA_RECIPE}));

	// an Asmodian at the Asmodian master
	commonData().setRace(model::Race::ASMODIANS);
	setSkills({});
	give(820122, KINAH, 3500);
	learn(LAINITA);
	clearSent();
	ASSERT_TRUE(player().getResponseRequester().respond(STR_CRAFT_ADDSKILL_CONFIRM, 1));
	EXPECT_EQ(itemtest::packetsOf(sent(), SM_LEARN_RECIPE_OPCODE),
		cp::exactly({serializedFor(network::aion::serverpackets::SM_LEARN_RECIPE(ASMODIAN_COOKING_RECIPE))}));
	EXPECT_EQ(storedRecipes(), (std::vector<int32_t>{ROAST_ININA_RECIPE, ASMODIAN_COOKING_RECIPE}));
}

// :159-175 with getTotalExpertCraftingSkills / getTotalMasterCraftingSkills (:129-157) and the shipped limits (craft.properties:12, :16: 2 and
// 1): an expert profession counts against the expert limit, a master one against both
TEST_F(CraftSkillUpdateServiceTest, TheExpertAndMasterLimits) {
	setSkills({{COOKING, 400}});
	EXPECT_TRUE(service().canLearnMoreExpertCraftingSkill(player())) << "1 expert < 2";
	EXPECT_TRUE(service().canLearnMoreMasterCraftingSkill(player())) << "0 masters < 1";
	EXPECT_TRUE(sent().empty());

	setSkills({{COOKING, 400}, {WEAPONSMITHING, 500}});
	EXPECT_FALSE(service().canLearnMoreExpertCraftingSkill(player())) << "1 expert + 1 master";
	EXPECT_EQ(sent(), cp::exactly({goldenMessage("You can only be an expert in 2 professions.")}));
	clearSent();
	EXPECT_FALSE(service().canLearnMoreMasterCraftingSkill(player()));
	EXPECT_EQ(sent(), cp::exactly({goldenMessage("You can only be a master in 1 professions.")}));
	clearSent();

	// the limits are read from the configuration
	mail::ConfigScope<int32_t> threeExperts(configs::main::CraftConfig::MAX_EXPERT_CRAFTING_SKILLS, 3);
	mail::ConfigScope<int32_t> twoMasters(configs::main::CraftConfig::MAX_MASTER_CRAFTING_SKILLS, 2);
	EXPECT_TRUE(service().canLearnMoreExpertCraftingSkill(player()));
	EXPECT_TRUE(service().canLearnMoreMasterCraftingSkill(player()));
	EXPECT_TRUE(sent().empty());
	setSkills({{COOKING, 400}, {WEAPONSMITHING, 500}, {TAILORING, 500}});
	EXPECT_FALSE(service().canLearnMoreExpertCraftingSkill(player()));
	EXPECT_EQ(sent(), cp::exactly({goldenMessage("You can only be an expert in 3 professions.")}));
	clearSent();
	EXPECT_FALSE(service().canLearnMoreMasterCraftingSkill(player()));
	EXPECT_EQ(sent(), cp::exactly({goldenMessage("You can only be a master in 2 professions.")}));
}

} // namespace
} // namespace aion::gameserver::economy::test::craft
