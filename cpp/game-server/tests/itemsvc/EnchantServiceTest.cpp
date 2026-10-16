// P5-07 EnchantService (m5c-plan.md E-01) and the ItemSocketService manastone bodies (E-02), against EnchantService.java:37-591 and
// ItemSocketService.java:30-151: breakItem's grade and count over a seeded Rnd, the success chances of enchantItem and socketManastone (read
// from the AdminConfig.ENCHANT_INFO message, which prints Java's float) with the supplement counts of the stones, both outcome arms of
// enchantItemAct and socketManastoneAct and their equipped-target arms, the enchant bonus, the tunings a failure or a new stone uses up, the
// storage each change marks for saving, applyEnchantEffect and W-24 (the equip of an enchanted item), the slot choice of addManaStone, W-25
// (ItemService.copyItemInfo of a socketed item), removeManastone's price and refusals, removeAllManastone and amplifyItem. Tests of E-05; the
// rows and the profile are EnchantTestSupport.h's.

#include "EnchantTestSupport.h"
#include "../dao/DaoTestDatabase.h"

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/dao/ItemStoneListDAO.h"
#include "aion/gameserver/dataholders/EnchantData.bind.h"
#include "aion/gameserver/dataholders/EnchantData.h"
#include "aion/gameserver/dataholders/ItemSetData.h"
#include "aion/gameserver/model/enchants/EnchantEffect.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/items/ManaStone.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/stats/listeners/ItemEquipmentListener.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/EnchantService.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/services/item/ItemSocketService.h"
#include "aion/gameserver/services/trade/PricesService.h"
#include "aion/gameserver/utils/stats/CalculationType.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::item::test {
namespace {

namespace Rnd = commons::utils::Rnd;
using model::items::ManaStone;
using network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using services::EnchantService;
using ItemUpdateType = ItemPacketService::ItemUpdateType;
using PersistentState = model::gameobjects::Persistable_PersistentState;

class EnchantServiceTest : public EnhanceTest {
protected:
	/** The slots and item ids of an item's stones, in slot order */
	std::vector<std::pair<int32_t, int32_t>> stonesOf(Item& item, bool fusion = false) {
		std::vector<std::pair<int32_t, int32_t>> stones;
		for (const Ptr<ManaStone>& stone : (fusion ? item.getFusionStones() : item.getItemStones())->snapshot())
			stones.emplace_back(stone->getSlot(), stone->getItemId());
		return stones;
	}

	/** The first seed from 1 on whose first Rnd.chance() falls in [from, to) (the crit roll of enchantItemAct, EnchantService.java:179) */
	uint64_t seedWithFirstChanceIn(float from, float to) {
		for (uint64_t seed = 1; seed < 100000; seed++) {
			Rnd::seedCurrentThreadForTests(seed);
			const float chance = Rnd::chance();
			if (chance >= from && chance < to)
				return seed;
		}
		ADD_FAILURE() << "no seed in range";
		return 0;
	}
};

// ------------------------------------------------------------------------------------------------------------------------- breakItem

TEST_F(EnchantServiceTest, BreakingAWeaponYieldsTheGradeOfTheRolledEffectiveLevelAndTwoToFiveStones) {
	// EnchantService.java:48-74: Tahabata's Sword is EPIC level 50 (item_templates.xml:4807), so calculateEffectiveLevel is 50 + 20 = 70 and the
	// roll 70 + Rnd.get(0, 10) + 5 (a weapon) lies in 75..85, against Delta 60 + 20 = 80 and Gamma 55 + 15 = 70 (EnchantmentStone.java: DELTA(60,
	// EPIC), GAMMA(55, UNIQUE)): Delta (166000194) for a roll of 5 or more, Gamma (166000193) below. The weapon's count is Rnd.get(2, 5). The
	// same draws, taken again from the same seed, predict each break; the tools and the sword are used up
	std::map<int32_t, int64_t> expected;
	std::set<int32_t> grades;
	std::set<int32_t> counts;
	for (uint64_t seed = 1; seed <= 30; seed++) {
		SCOPED_TRACE(seed);
		Rnd::seedCurrentThreadForTests(seed);
		const int32_t roll = Rnd::get(0, 10);
		const int32_t count = Rnd::get(2, 5);
		const int32_t stoneId = 75 + roll >= 80 ? DELTA_ENCHANTMENT_STONE : GAMMA_ENCHANTMENT_STONE;
		expected[stoneId] += count;
		grades.insert(stoneId);
		counts.insert(count);

		const int32_t objId = 820000 + static_cast<int32_t>(seed) * 2;
		Item& tools = inCube(objId, EXTRACTION_TOOLS, 1);
		Item& sword = inCube(objId + 1, TAHABATA_SWORD, 1);
		clearSent();
		Rnd::seedCurrentThreadForTests(seed);
		ASSERT_TRUE(EnchantService::breakItem(player(), sword, tools));
		EXPECT_FALSE(player().getInventory().getItemByObjId(objId + 1)) << "the sword is deleted";
		EXPECT_FALSE(player().getInventory().getItemByObjId(objId)) << "the only tool is used up";
		EXPECT_EQ(tools.getItemCount(), 0);
		EXPECT_EQ(cubeCount(GAMMA_ENCHANTMENT_STONE), expected[GAMMA_ENCHANTMENT_STONE]);
		EXPECT_EQ(cubeCount(DELTA_ENCHANTMENT_STONE), expected[DELTA_ENCHANTMENT_STONE]);
		EXPECT_EQ(cubeCount(ALPHA_ENCHANTMENT_STONE) + cubeCount(BETA_ENCHANTMENT_STONE) + cubeCount(EPSILON_ENCHANTMENT_STONE), 0);
		std::vector<std::vector<uint8_t>> messages = sentWithOpcode(SM_SYSTEM_MESSAGE_OPCODE);
		ASSERT_EQ(messages.size(), 1u);
		EXPECT_EQ(messages[0], serialized(SM_SYSTEM_MESSAGE::STR_DECOMPOSE_ITEM_SUCCEED(sword.getL10n())));
	}
	EXPECT_EQ(grades, (std::set<int32_t>{GAMMA_ENCHANTMENT_STONE, DELTA_ENCHANTMENT_STONE})) << "both grades over the seeds";
	EXPECT_EQ(counts, (std::set<int32_t>{2, 3, 4, 5}));
}

TEST_F(EnchantServiceTest, BreakingAnArmourYieldsOneToThreeAlphaStones) {
	// EnchantService.java:48-74: Plainsman's Tunic is COMMON level 4 armour (item_templates.xml:189490): 4 + 5 = 9, the roll 9..19 (no weapon
	// bonus) stays below Beta 40 + 10 = 50, so Alpha (166000191); the armour's count is Rnd.get(1, 3)
	int64_t expected = 0;
	std::set<int32_t> counts;
	for (uint64_t seed = 1; seed <= 15; seed++) {
		SCOPED_TRACE(seed);
		Rnd::seedCurrentThreadForTests(seed);
		static_cast<void>(Rnd::get(0, 10));
		const int32_t count = Rnd::get(1, 3);
		expected += count;
		counts.insert(count);

		const int32_t objId = 821000 + static_cast<int32_t>(seed) * 2;
		Item& tools = inCube(objId, EXTRACTION_TOOLS, 1);
		Item& tunic = inCube(objId + 1, PLAINSMANS_TUNIC, 1);
		Rnd::seedCurrentThreadForTests(seed);
		ASSERT_TRUE(EnchantService::breakItem(player(), tunic, tools));
		EXPECT_EQ(cubeCount(ALPHA_ENCHANTMENT_STONE), expected);
	}
	EXPECT_EQ(counts, (std::set<int32_t>{1, 2, 3}));
	EXPECT_EQ(cubeCount(PLAINSMANS_TUNIC), 0);
}

TEST_F(EnchantServiceTest, BreakItemRefusesWhatIsNotInTheCubeOrIsNoArmourOrWeapon) {
	// EnchantService.java:39-46: either item missing from the inventory -> false; a template that is neither armour nor weapon -> an audit line
	// and false (the potion, item_templates.xml:830724). Nothing is sent and nothing is used up
	Item& tools = inCube(822001, EXTRACTION_TOOLS, 2);
	Item& sword = inCube(822002, TRAINING_SWORD, 1);
	Item& potions = inCube(822003, MINOR_LIFE_POTION, 5);
	Ref<Item> looseSword = itemRow(822004, TRAINING_SWORD, 1);
	Ref<Item> looseTools = itemRow(822005, EXTRACTION_TOOLS, 1);

	EXPECT_FALSE(EnchantService::breakItem(player(), *looseSword, tools));
	EXPECT_FALSE(EnchantService::breakItem(player(), sword, *looseTools));
	EXPECT_FALSE(EnchantService::breakItem(player(), potions, tools));
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(tools.getItemCount(), 2);
	EXPECT_EQ(potions.getItemCount(), 5);
	EXPECT_TRUE(player().getInventory().getItemByObjId(822002));
	EXPECT_EQ(cubeCount(ALPHA_ENCHANTMENT_STONE), 0);
}

// ------------------------------------------------------------------------------------------------------------------------- enchantItem

TEST_F(EnchantServiceTest, TheEnchantChanceFollowsLevelQualityAndEnchantLevelAndIsCappedAtEighty) {
	// EnchantService.java:100-171 with the shipped base chance 65 (rates.properties:36) and ENCHANT_INFO granted (access 0 >= 0), whose message
	// prints the chance with Float.toString:
	// - Training Sword (COMMON level 1, item_templates.xml:375) with an Alpha stone (ALPHA(20, RARE)): the level is raised to 20, so 65 + 0 +
	//   (2 - 1) * 5 = 70, * 1.2f at +0 = 84 -> capped at 80; unchanged at +7 = 70 (without the raise it would be 65 + 19 + 5 = 89 -> 80)
	// - Tahabata's Sword (EPIC level 50): 65 + (20 - 50) + (2 - 5) * 5 = 20; * 1.2f at +0 = 24, * 1.1f at +3 = 22, unchanged at +5 (the boost
	//   ends below 5, :120-121) and +7 = 20, * 0.9f at +10 = 18
	// The result is Rnd.chance() < chance; the same seed predicts it
	AtomicConfigScope<int8_t> info(configs::administration::AdminConfig::ENCHANT_INFO, int8_t{0});
	Item& stones = inCube(823001, ALPHA_ENCHANTMENT_STONE, 10);
	struct Case {
		int32_t itemId;
		int32_t enchant;
		float chance;
		std::string text;
	};
	const std::vector<Case> cases{{TRAINING_SWORD, 0, 80.0f, "80.0"}, {TRAINING_SWORD, 7, 70.0f, "70.0"}, {TAHABATA_SWORD, 0, 24.0f, "24.0"},
		{TAHABATA_SWORD, 3, 22.0f, "22.0"}, {TAHABATA_SWORD, 5, 20.0f, "20.0"}, {TAHABATA_SWORD, 7, 20.0f, "20.0"},
		{TAHABATA_SWORD, 10, 18.0f, "18.0"}};
	int32_t objId = 823010;
	for (const Case& c : cases) {
		SCOPED_TRACE(c.text);
		Item& target = inCube(objId++, c.itemId, 1, c.enchant);
		for (uint64_t seed : {3u, 11u}) {
			Rnd::seedCurrentThreadForTests(seed);
			const bool expected = Rnd::chance() < c.chance;
			clearSent();
			Rnd::seedCurrentThreadForTests(seed);
			EXPECT_EQ(EnchantService::enchantItem(player(), stones, target, nullptr), expected);
			EXPECT_EQ(sent(), cp::exactly({message(std::string(expected ? "Success" : "Fail") + " (success chance:" + c.text + "%)")}));
		}
	}
	EXPECT_EQ(stones.getItemCount(), 10) << "enchantItem only decides";
}

TEST_F(EnchantServiceTest, AnAmplifiedItemUsesTheAmplifiedChanceAlone) {
	// EnchantService.java:103-104: an amplified target takes Rates.get(ENCHANTMENT_STONE_AMPLIFIED_CHANCES) (61, rates.properties:40) and none
	// of the level, quality or enchant-level terms
	AtomicConfigScope<int8_t> info(configs::administration::AdminConfig::ENCHANT_INFO, int8_t{0});
	Item& stones = inCube(823101, ALPHA_ENCHANTMENT_STONE, 1);
	Item& sword = inCube(823102, TAHABATA_SWORD, 1, 16, 0, true);
	Rnd::seedCurrentThreadForTests(5);
	const bool expected = Rnd::chance() < 61.0f;
	Rnd::seedCurrentThreadForTests(5);
	EXPECT_EQ(EnchantService::enchantItem(player(), stones, sword, nullptr), expected);
	EXPECT_EQ(sent(), cp::exactly({message(std::string(expected ? "Success" : "Fail") + " (success chance:61.0%)")}));
}

TEST_F(EnchantServiceTest, ASupplementAddsItsChanceAndNeedsTheStonesCountDoubledFromPlusTen) {
	// EnchantService.java:130-162: Lesser Supplements add 5.0 (item_templates.xml:838880); the count is the stone's enchant count (Alpha: 1,
	// :837972), doubled from +10; too few supplements -> false with no message; otherwise the supplements are queued (Player.subtractSupplements)
	// and used up only by updateSupplements. Tahabata's Sword: 24 + 5 = 29 at +0, 18 + 5 = 23 at +10
	AtomicConfigScope<int8_t> info(configs::administration::AdminConfig::ENCHANT_INFO, int8_t{0});
	Item& stones = inCube(823201, ALPHA_ENCHANTMENT_STONE, 5);
	Item& swordAtZero = inCube(823202, TAHABATA_SWORD, 1, 0);
	Item& swordAtTen = inCube(823203, TAHABATA_SWORD, 1, 10);
	Ref<Item> supplementRow = itemRow(823204, LESSER_SUPPLEMENTS, 1);

	// none in the cube: refused before any roll or message
	EXPECT_FALSE(EnchantService::enchantItem(player(), stones, swordAtZero, Ptr<Item>(supplementRow)));
	EXPECT_TRUE(sent().empty());

	Item& supplements = inCube(823205, LESSER_SUPPLEMENTS, 1);
	Rnd::seedCurrentThreadForTests(7);
	bool expected = Rnd::chance() < 29.0f;
	Rnd::seedCurrentThreadForTests(7);
	EXPECT_EQ(EnchantService::enchantItem(player(), stones, swordAtZero, Ptr<Item>(supplements)), expected);
	EXPECT_EQ(sent(), cp::exactly({message(std::string(expected ? "Success" : "Fail") + " (success chance:29.0%)")}));
	EXPECT_EQ(supplements.getItemCount(), 1) << "only queued";
	player().updateSupplements();
	EXPECT_EQ(cubeCount(LESSER_SUPPLEMENTS), 0) << "the queued count was 1";

	// +10: two needed, one there -> refused
	inCube(823206, LESSER_SUPPLEMENTS, 1);
	clearSent();
	EXPECT_FALSE(EnchantService::enchantItem(player(), stones, swordAtTen, supplementRow));
	EXPECT_TRUE(sent().empty());
	inCube(823207, LESSER_SUPPLEMENTS, 1);
	Rnd::seedCurrentThreadForTests(9);
	expected = Rnd::chance() < 23.0f;
	Rnd::seedCurrentThreadForTests(9);
	EXPECT_EQ(EnchantService::enchantItem(player(), stones, swordAtTen, supplementRow), expected);
	EXPECT_EQ(sent(), cp::exactly({message(std::string(expected ? "Success" : "Fail") + " (success chance:23.0%)")}));
	player().updateSupplements();
	EXPECT_EQ(cubeCount(LESSER_SUPPLEMENTS), 0) << "the queued count was 2";
}

TEST_F(EnchantServiceTest, AManastoneOnlySupplementCannotHelpAnEnchantment) {
	// EnchantService.java:137-139: the supplement's own enchant action says manastone_only (item_templates.xml:839000) -> false at once
	AtomicConfigScope<int8_t> info(configs::administration::AdminConfig::ENCHANT_INFO, int8_t{0});
	Item& stones = inCube(823301, ALPHA_ENCHANTMENT_STONE, 1);
	Item& sword = inCube(823302, TRAINING_SWORD, 1);
	Item& sand = inCube(823303, MANASTONE_ONLY_SAND, 3);
	EXPECT_FALSE(EnchantService::enchantItem(player(), stones, sword, Ptr<Item>(sand)));
	EXPECT_TRUE(sent().empty());
	player().updateSupplements();
	EXPECT_EQ(sand.getItemCount(), 3) << "nothing was queued";
}

TEST_F(EnchantServiceTest, ABetaStoneNeedsTenSupplementsWhichTheActUsesUp) {
	// EnchantService.java:144-157, 190-191: the supplements needed are the stone's own enchant count, 10 for a Beta stone (item_templates.xml:
	// 837977): 9 Lesser Supplements are refused before any roll or message, 10 are queued (Player.subtractSupplements) and enchantItemAct uses
	// them up (Player.updateSupplements). Tahabata's Sword with a Beta stone (BETA(40, LEGEND)): 65 + (40 - 50) + (3 - 5) * 5 = 45; * 1.2f at
	// +0 is 54.000004 in float (45 * 1.2f lies nearer the float above 54), + 5.0 = 59.000004
	AtomicConfigScope<int8_t> info(configs::administration::AdminConfig::ENCHANT_INFO, int8_t{0});
	Item& stones = inCube(823401, BETA_ENCHANTMENT_STONE, 1);
	Item& sword = inCube(823402, TAHABATA_SWORD, 1);
	Item& supplements = inCube(823403, LESSER_SUPPLEMENTS, 9);
	EXPECT_FALSE(EnchantService::enchantItem(player(), stones, sword, Ptr<Item>(supplements)));
	EXPECT_TRUE(sent().empty());
	player().updateSupplements();
	EXPECT_EQ(cubeCount(LESSER_SUPPLEMENTS), 9) << "nothing was queued";

	inCube(823404, LESSER_SUPPLEMENTS, 1);
	Rnd::seedCurrentThreadForTests(17);
	const bool success = Rnd::chance() < 59.000004f;
	Rnd::seedCurrentThreadForTests(17);
	EXPECT_EQ(EnchantService::enchantItem(player(), stones, sword, Ptr<Item>(supplements)), success);
	EXPECT_EQ(sent(), cp::exactly({message(std::string(success ? "Success" : "Fail") + " (success chance:59.000004%)")}));
	EXPECT_EQ(cubeCount(LESSER_SUPPLEMENTS), 10) << "only queued";
	EnchantService::enchantItemAct(player(), stones, sword, supplements, 0, success);
	EXPECT_EQ(cubeCount(LESSER_SUPPLEMENTS), 0) << "the act used up the 10 queued";
	EXPECT_FALSE(player().getInventory().getItemByObjId(823401)) << "and the stone";
}

// ------------------------------------------------------------------------------------------------------------------------- enchantItemAct

TEST_F(EnchantServiceTest, ASuccessAddsTheCritLevelsUpToTheMaximum) {
	// EnchantService.java:173-218: below the maximum Rnd.chance() picks the levels (< 5: 3, < 10: 2, else 1); one stone is used up; the new level
	// is min(current + add, max) for an item that is not amplified; STATS_CHANGE update, then STR_MSG_ENCHANT_ITEM_SUCCEED_NEW. Training Sword:
	// max_enchant 10 (item_templates.xml:375)
	struct Case {
		float from;
		float to;
		int32_t current;
		int32_t expected;
	};
	const std::vector<Case> cases{{0.0f, 5.0f, 2, 5}, {5.0f, 10.0f, 2, 4}, {10.0f, 100.0f, 2, 3}, {0.0f, 5.0f, 9, 10}};
	int32_t objId = 824000;
	for (const Case& c : cases) {
		SCOPED_TRACE(std::to_string(c.from) + " +" + std::to_string(c.current));
		Item& stones = inCube(objId++, ALPHA_ENCHANTMENT_STONE, 3);
		Item& sword = inCube(objId++, TRAINING_SWORD, 1, c.current);
		const uint64_t seed = seedWithFirstChanceIn(c.from, c.to);
		clearSent();
		Rnd::seedCurrentThreadForTests(seed);
		EnchantService::enchantItemAct(player(), stones, sword, stones, c.current, true);
		EXPECT_EQ(sword.getEnchantLevel(), c.expected);
		EXPECT_EQ(stones.getItemCount(), 2);
		std::vector<std::vector<uint8_t>> packets = sent();
		ASSERT_EQ(packets.size(), 3u);
		EXPECT_EQ(packets[0], serialized(SM_INVENTORY_UPDATE_ITEM(player(), stones, ItemUpdateType::DEC_ITEM_USE)));
		EXPECT_EQ(packets[1], serialized(SM_INVENTORY_UPDATE_ITEM(player(), sword, ItemUpdateType::STATS_CHANGE)));
		EXPECT_EQ(packets[2], serialized(SM_SYSTEM_MESSAGE::STR_MSG_ENCHANT_ITEM_SUCCEED_NEW(sword.getL10n(), c.expected)));
	}
}

TEST_F(EnchantServiceTest, AnAmplifiedSuccessIsNotCappedAtTheMaximum) {
	// EnchantService.java:195-198, 212-213: `!isAmplified() && current + add > max` - an amplified item takes current + add, and max becomes 255.
	// Tahabata's Sword at +15 (max_enchant 15) cannot roll a crit (:178 is false), so it gains exactly 1 - also from a seed whose first
	// Rnd.chance() would be a crit of 3 levels
	Item& stones = inCube(824101, ALPHA_ENCHANTMENT_STONE, 1);
	Item& sword = inCube(824102, TAHABATA_SWORD, 1, 15, 0, true);
	Rnd::seedCurrentThreadForTests(seedWithFirstChanceIn(0.0f, 5.0f));
	EnchantService::enchantItemAct(player(), stones, sword, stones, 15, true);
	EXPECT_EQ(sword.getEnchantLevel(), 16);
	EXPECT_TRUE(sword.isAmplified());
	EXPECT_FALSE(player().getInventory().getItemByObjId(824101)) << "the last stone is used up";
}

TEST_F(EnchantServiceTest, TheEnchantBonusOfAnItemRaisesItsMaximum) {
	// EnchantService.java:176-178, 195-196: the maximum is max_enchant plus the item's enchant bonus (identification sets it,
	// ItemActionService.java:46; copyItemInfo copies it). A Training Sword (max_enchant 10) with a bonus of 2 at +10 still rolls the crit and
	// goes to +11, or to the maximum +12 from a crit of 3 levels
	struct Case {
		float from;
		float to;
		int32_t expected;
	};
	const std::vector<Case> cases{{10.0f, 100.0f, 11}, {0.0f, 5.0f, 12}};
	int32_t objId = 824150;
	for (const Case& c : cases) {
		SCOPED_TRACE(c.expected);
		Item& stones = inCube(objId++, ALPHA_ENCHANTMENT_STONE, 1);
		Ref<Item> sword = Item::create(objId++, TRAINING_SWORD, 1, std::nullopt, 0, "", 0, 0, false, false, 0,
			model::items::storage::getId(StorageType::CUBE), 10, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, 0);
		items.push_back(sword);
		storage(StorageType::CUBE).onLoadHandler(*sword);
		ASSERT_EQ(sword->getEnchantBonus(), 2);
		Rnd::seedCurrentThreadForTests(seedWithFirstChanceIn(c.from, c.to));
		EnchantService::enchantItemAct(player(), stones, *sword, stones, 10, true);
		EXPECT_EQ(sword->getEnchantLevel(), c.expected);
	}
}

TEST_F(EnchantServiceTest, AFailureFallsBackByTheRulesOfItsArm) {
	// EnchantService.java:199-230: amplified -> the maximum, no longer amplified; above +10 with a maximum above 10 -> +10; above 0 -> one less
	// (also for a maximum above 10 at +10 or below); then STR_ENCHANT_ITEM_FAILED. Training Sword max 10, Tahabata's Sword max 15
	struct Case {
		int32_t itemId;
		int32_t current;
		bool amplified;
		int32_t expected;
	};
	const std::vector<Case> cases{{TRAINING_SWORD, 0, false, 0}, {TRAINING_SWORD, 5, false, 4}, {TRAINING_SWORD, 10, false, 9},
		{TAHABATA_SWORD, 12, false, 10}, {TAHABATA_SWORD, 11, false, 10}, {TAHABATA_SWORD, 5, false, 4}, {TAHABATA_SWORD, 17, true, 15}};
	int32_t objId = 824200;
	for (const Case& c : cases) {
		SCOPED_TRACE(std::to_string(c.itemId) + " +" + std::to_string(c.current));
		Item& stones = inCube(objId++, ALPHA_ENCHANTMENT_STONE, 2);
		Item& target = inCube(objId++, c.itemId, 1, c.current, 0, c.amplified);
		clearSent();
		EnchantService::enchantItemAct(player(), stones, target, stones, c.current, false);
		EXPECT_EQ(target.getEnchantLevel(), c.expected);
		EXPECT_FALSE(target.isAmplified());
		EXPECT_EQ(stones.getItemCount(), 1);
		std::vector<std::vector<uint8_t>> messages = sentWithOpcode(SM_SYSTEM_MESSAGE_OPCODE);
		ASSERT_EQ(messages.size(), 1u);
		EXPECT_EQ(messages[0], serialized(SM_SYSTEM_MESSAGE::STR_ENCHANT_ITEM_FAILED(target.getL10n())));
	}
}

TEST_F(EnchantServiceTest, AFailedEnchantOfAnEnchantTypeItemDestroysIt) {
	// EnchantService.java:221-226: enchant_type > 0 (Set Test Sword 01, item_templates.xml:4443) -> the item is decreased by one (deleted), then
	// STR_MSG_ENCHANT_TYPE1_ENCHANT_FAIL
	Item& stones = inCube(824301, ALPHA_ENCHANTMENT_STONE, 2);
	Item& sword = inCube(824302, SET_TEST_SWORD_01, 1);
	EnchantService::enchantItemAct(player(), stones, sword, stones, 0, false);
	EXPECT_FALSE(player().getInventory().getItemByObjId(824302));
	std::vector<std::vector<uint8_t>> messages = sentWithOpcode(SM_SYSTEM_MESSAGE_OPCODE);
	ASSERT_EQ(messages.size(), 2u);
	EXPECT_EQ(messages[0], serialized(SM_SYSTEM_MESSAGE::STR_ENCHANT_ITEM_FAILED(sword.getL10n())));
	EXPECT_EQ(messages[1], serialized(SM_SYSTEM_MESSAGE::STR_MSG_ENCHANT_TYPE1_ENCHANT_FAIL(sword.getL10n())));
}

TEST_F(EnchantServiceTest, WithoutItsStoneTheActChangesNothing) {
	// EnchantService.java:186-189: the stone left the cube -> an audit line and return, before the supplements and the level
	Item& sword = inCube(824401, TRAINING_SWORD, 1, 3);
	Ref<Item> stone = itemRow(824402, ALPHA_ENCHANTMENT_STONE, 1);
	EnchantService::enchantItemAct(player(), *stone, sword, *stone, 3, true);
	EnchantService::enchantItemAct(player(), *stone, sword, *stone, 3, false);
	EXPECT_EQ(sword.getEnchantLevel(), 3);
	EXPECT_TRUE(sent().empty());
}

// ------------------------------------------------------------------------------------------------------------------------- applyEnchantEffect

TEST_F(EnchantServiceTest, AnEquippedEnchantedSwordGetsTheStatsOfItsLevel) {
	// W-24 (m5c-plan.md §15.3): ItemEquipmentListener.onItemEquipment reaches applyEnchantEffect for an item above +0 (ItemEquipmentListener.java,
	// EnchantService.java:275-296). The SWORD list's highest level is 21: below it the level's own stats (+5: PHYSICAL_ATTACK 10); from it the
	// stats of level 20 (40) plus level 21's (2) once per level from 21 on (+23: 40 + 3 * 2 = 46). A new effect ends the old one first
	xml::LoadContext context;
	dataholders::DataManager::ENCHANT_DATA.publish(xml::bindString<dataholders::EnchantData>(context, SWORD_ENCHANT_TEMPLATES_XML));
	// the Training Sword is in no set of item_sets.xml, so an empty holder answers isItemSet() as the shipped one does
	dataholders::DataManager::ITEM_SET_DATA.publish(std::make_unique<dataholders::ItemSetData>());
	Ref<Item> sword = Item::create(829501, TRAINING_SWORD, 1, std::nullopt, 0, "", 0, 0, true, false, 1,
		model::items::storage::getId(StorageType::CUBE), 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, 0);
	items.push_back(sword);
	// a loaded player has its skill list before its equipment (PlayerService.java:116): Equipment.onLoadHandler keeps a sword only for a player
	// with one of its skills (Equipment.java:437-440, 303-314; ItemGroup.java:16, SWORD requires 37 or 44)
	player().setSkillList(model::skill::PlayerSkillList::create(
		{model::skill::PlayerSkillEntry::create(37, 1, 0, model::gameobjects::Persistable_PersistentState::UPDATED)}));
	player().getEquipment().onLoadHandler(*sword);
	// CreatureGameStats.java:132-136 counts an enchant effect's PHYSICAL_ATTACK only in the attack of its hand, the main hand here (slot 1):
	// PlayerGameStats.java:151-170, whose DISPLAY base is the weapon's mean damage (no Rnd)
	auto physicalAttack = [this] { return player().getGameStats()->getMainHandPAttack({utils::stats::CalculationType::DISPLAY})->getCurrent(); };

	EXPECT_FALSE(sword->getEnchantEffect());
	model::stats::listeners::ItemEquipmentListener::onItemEquipment(*sword, player());
	ASSERT_EQ(player().getEquipment().getMainHandWeapon().get(), sword.get()) << "the sword is equipped in the main hand";
	ASSERT_TRUE(sword->getEnchantEffect()) << "W-24: the equip applied the +5 effect";
	const int32_t equipped = physicalAttack();

	EnchantService::applyEnchantEffect(*sword, player(), 23);
	EXPECT_EQ(physicalAttack() - equipped, 46 - 10) << "the +5 effect ended, the +23 effect added";
	EnchantService::applyEnchantEffect(*sword, player(), 5);
	EXPECT_EQ(physicalAttack(), equipped);
	sword->getEnchantEffect()->endEffect(player());
	EXPECT_EQ(equipped - physicalAttack(), 10) << "the +5 effect is PHYSICAL_ATTACK 10";
}

TEST_F(EnchantServiceTest, AnEquippedSwordEnchantedByTheActTakesTheStatsOfItsNewLevel) {
	// EnchantService.java:233-273 through enchantItemAct (an equipped target is enchantable, EnchantItemAction.java:116): the old effect ends,
	// the stats are updated, the new level's effect is applied as W-24's equip applies it, and the equipment, not the cube, is marked for the
	// update. Training Sword +2 -> +3 (a crit roll of 10 or more adds 1): PHYSICAL_ATTACK 6 instead of 4 (the SWORD list above)
	xml::LoadContext context;
	dataholders::DataManager::ENCHANT_DATA.publish(xml::bindString<dataholders::EnchantData>(context, SWORD_ENCHANT_TEMPLATES_XML));
	dataholders::DataManager::ITEM_SET_DATA.publish(std::make_unique<dataholders::ItemSetData>());
	player().setSkillList(model::skill::PlayerSkillList::create(
		{model::skill::PlayerSkillEntry::create(37, 1, 0, model::gameobjects::Persistable_PersistentState::UPDATED)}));
	Ref<Item> sword = Item::create(829601, TRAINING_SWORD, 1, std::nullopt, 0, "", 0, 0, true, false, 1,
		model::items::storage::getId(StorageType::CUBE), 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, 0);
	items.push_back(sword);
	player().getEquipment().onLoadHandler(*sword);
	ASSERT_EQ(player().getEquipment().getMainHandWeapon().get(), sword.get());
	model::stats::listeners::ItemEquipmentListener::onItemEquipment(*sword, player());
	auto physicalAttack = [this] { return player().getGameStats()->getMainHandPAttack({utils::stats::CalculationType::DISPLAY})->getCurrent(); };
	const int32_t atPlusTwo = physicalAttack();
	Item& stones = inCube(829602, ALPHA_ENCHANTMENT_STONE, 2);
	player().getEquipment().setPersistentState(PersistentState::UPDATED);

	Rnd::seedCurrentThreadForTests(seedWithFirstChanceIn(10.0f, 100.0f));
	EnchantService::enchantItemAct(player(), stones, *sword, stones, 2, true);
	EXPECT_EQ(sword->getEnchantLevel(), 3);
	EXPECT_EQ(stones.getItemCount(), 1);
	EXPECT_EQ(physicalAttack() - atPlusTwo, 6 - 4) << "the +2 effect ended, the +3 effect added";
	EXPECT_EQ(player().getEquipment().getPersistentState(), PersistentState::UPDATE_REQUIRED);
}

TEST_F(EnchantServiceTest, TheNewLevelOfAnItemInTheCubeIsSavedWithTheCube) {
	// EnchantService.java:268-272: an item that is not equipped marks the inventory, not the equipment, for the update (the storages
	// Player.getDirtyItemsToUpdate saves, Player.java:536-552); setEnchantLevel alone, since the stone enchantItemAct uses up marks the cube too
	Item& sword = inCube(829651, TRAINING_SWORD, 1, 2);
	player().getInventory().setPersistentState(PersistentState::UPDATED);
	player().getEquipment().setPersistentState(PersistentState::UPDATED);
	EnchantService::setEnchantLevel(player(), sword, 3);
	EXPECT_EQ(sword.getEnchantLevel(), 3);
	EXPECT_EQ(player().getInventory().getPersistentState(), PersistentState::UPDATE_REQUIRED);
	EXPECT_EQ(player().getEquipment().getPersistentState(), PersistentState::UPDATED);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_INVENTORY_UPDATE_ITEM(player(), sword, ItemUpdateType::STATS_CHANGE))}));
}

// ------------------------------------------------------------------------------------------------------------------------- socketManastone

TEST_F(EnchantServiceTest, TheManastoneChanceFallsWithTheSocketedStones) {
	// EnchantService.java:298-402 with the shipped 75 (rates.properties:32): Manastone HP +20 is COMMON level 10 (item_templates.xml:839282).
	// Training Sword level 1: slot level 10 * ceil(11 / 10d) = 20, no stone: 75 + (20 - 10) / (0 * 1.25f + 1.75f) = 80.71429. Plainsman's Tunic
	// level 4 with one optional socket (2 sockets) and one stone: 75 + 10 / 3.0f = 78.333336
	AtomicConfigScope<int8_t> info(configs::administration::AdminConfig::ENCHANT_INFO, int8_t{0});
	Item& stone = inCube(825001, MANASTONE_HP_20, 5);
	Item& sword = inCube(825002, TRAINING_SWORD, 1);
	Item& tunic = inCube(825003, PLAINSMANS_TUNIC, 1, 0, 1);
	ASSERT_TRUE(ItemSocketService::addManaStone(Ptr<Item>(tunic), MANASTONE_HP_20, false));
	ASSERT_EQ(tunic.getSockets(false), 2);

	Rnd::seedCurrentThreadForTests(13);
	bool expected = Rnd::chance() < 80.71429f;
	Rnd::seedCurrentThreadForTests(13);
	EXPECT_EQ(EnchantService::socketManastone(player(), stone, sword, nullptr, 1), expected);
	EXPECT_EQ(sent(), cp::exactly({message(std::string(expected ? "Success" : "Fail") + " (success chance:80.71429%)")}));

	clearSent();
	Rnd::seedCurrentThreadForTests(13);
	expected = Rnd::chance() < 78.333336f;
	Rnd::seedCurrentThreadForTests(13);
	EXPECT_EQ(EnchantService::socketManastone(player(), stone, tunic, nullptr, 1), expected);
	EXPECT_EQ(sent(), cp::exactly({message(std::string(expected ? "Success" : "Fail") + " (success chance:78.333336%)")}));
	EXPECT_EQ(stone.getItemCount(), 5);
}

TEST_F(EnchantServiceTest, AStoneAboveTheSlotLevelOrAFullItemIsRefused) {
	// EnchantService.java:316-341: Manastone HP +60 is level 50 > the Training Sword's slot level 20 -> false; a sword whose one socket is taken
	// -> an audit line and false; the fused arm of an item without a fused weapon dereferences getFusionedItemTemplate() (NullPointerException,
	// :306). No message, no roll
	AtomicConfigScope<int8_t> info(configs::administration::AdminConfig::ENCHANT_INFO, int8_t{0});
	Item& bigStone = inCube(825101, MANASTONE_HP_60, 1);
	Item& stone = inCube(825102, MANASTONE_HP_20, 1);
	Item& sword = inCube(825103, TRAINING_SWORD, 1);
	Item& fullSword = inCube(825104, TRAINING_SWORD, 1);
	ASSERT_TRUE(ItemSocketService::addManaStone(Ptr<Item>(fullSword), MANASTONE_HP_20, false));

	EXPECT_FALSE(EnchantService::socketManastone(player(), bigStone, sword, nullptr, 1));
	EXPECT_FALSE(EnchantService::socketManastone(player(), stone, fullSword, nullptr, 1));
	EXPECT_THROW(EnchantService::socketManastone(player(), stone, sword, nullptr, 2), runtime::NullPointerException);
	EXPECT_TRUE(sent().empty());
}

TEST_F(EnchantServiceTest, ManastoneSupplementsCountPerSocketedStoneUnlessManastoneOnly) {
	// EnchantService.java:356-393: the manastone-only sand (chance 100, item_templates.xml:839000) needs exactly 1: 80.71429 + 100 = 180.7143.
	// Lesser Supplements (5.0) need the stone's count (1) times the stones after this one (1 + 1) once a stone is socketed: the tunic with one
	// stone needs 2, and 78.333336 + 5 = 83.333336. Too few -> false with no message
	AtomicConfigScope<int8_t> info(configs::administration::AdminConfig::ENCHANT_INFO, int8_t{0});
	Item& stone = inCube(825201, MANASTONE_HP_20, 5);
	Item& sword = inCube(825202, TRAINING_SWORD, 1);
	Item& tunic = inCube(825203, PLAINSMANS_TUNIC, 1, 0, 1);
	ASSERT_TRUE(ItemSocketService::addManaStone(Ptr<Item>(tunic), MANASTONE_HP_20, false));
	Ref<Item> sandRow = itemRow(825204, MANASTONE_ONLY_SAND, 1);
	EXPECT_FALSE(EnchantService::socketManastone(player(), stone, sword, sandRow, 1));
	EXPECT_TRUE(sent().empty());

	Item& sand = inCube(825205, MANASTONE_ONLY_SAND, 1);
	Rnd::seedCurrentThreadForTests(21);
	bool expected = Rnd::chance() < 180.7143f;
	Rnd::seedCurrentThreadForTests(21);
	EXPECT_EQ(EnchantService::socketManastone(player(), stone, sword, Ptr<Item>(sand), 1), expected);
	EXPECT_EQ(sent(), cp::exactly({message(std::string(expected ? "Success" : "Fail") + " (success chance:180.7143%)")}));
	player().updateSupplements();
	EXPECT_EQ(cubeCount(MANASTONE_ONLY_SAND), 0);

	Item& supplements = inCube(825206, LESSER_SUPPLEMENTS, 1);
	clearSent();
	EXPECT_FALSE(EnchantService::socketManastone(player(), stone, tunic, Ptr<Item>(supplements), 1)) << "2 needed";
	EXPECT_TRUE(sent().empty());
	inCube(825207, LESSER_SUPPLEMENTS, 1);
	Rnd::seedCurrentThreadForTests(23);
	expected = Rnd::chance() < 83.333336f;
	Rnd::seedCurrentThreadForTests(23);
	EXPECT_EQ(EnchantService::socketManastone(player(), stone, tunic, Ptr<Item>(supplements), 1), expected);
	EXPECT_EQ(sent(), cp::exactly({message(std::string(expected ? "Success" : "Fail") + " (success chance:83.333336%)")}));
	player().updateSupplements();
	EXPECT_EQ(cubeCount(LESSER_SUPPLEMENTS), 0);
}

TEST_F(EnchantServiceTest, ARareStoneAtTheSlotLevelHasFourFifthsOfTheChanceAndNeedsItsCountOfSupplements) {
	// EnchantService.java:316-317, 344-353, 372-374, 385-392: Manastone HP +55 is RARE level 20 (item_templates.xml:839626), the Training Sword's
	// slot level itself (only a stone above it is refused): 75 * 0.8f (RARE or better) = 60 + (20 - 20) / 1.75f = 60.0. Its enchant count 5 is
	// the supplements it needs on an item without stones (the per-stone factor applies from one stone on): 4 Lesser Supplements are refused
	// with no message, 5 give 60 + 5 = 65.0 and are queued
	AtomicConfigScope<int8_t> info(configs::administration::AdminConfig::ENCHANT_INFO, int8_t{0});
	Item& stone = inCube(825301, MANASTONE_HP_55, 1);
	Item& sword = inCube(825302, TRAINING_SWORD, 1);
	Rnd::seedCurrentThreadForTests(25);
	bool expected = Rnd::chance() < 60.0f;
	Rnd::seedCurrentThreadForTests(25);
	EXPECT_EQ(EnchantService::socketManastone(player(), stone, sword, nullptr, 1), expected);
	EXPECT_EQ(sent(), cp::exactly({message(std::string(expected ? "Success" : "Fail") + " (success chance:60.0%)")}));

	Item& supplements = inCube(825303, LESSER_SUPPLEMENTS, 4);
	clearSent();
	EXPECT_FALSE(EnchantService::socketManastone(player(), stone, sword, Ptr<Item>(supplements), 1)) << "5 needed";
	EXPECT_TRUE(sent().empty());
	inCube(825304, LESSER_SUPPLEMENTS, 1);
	Rnd::seedCurrentThreadForTests(27);
	expected = Rnd::chance() < 65.0f;
	Rnd::seedCurrentThreadForTests(27);
	EXPECT_EQ(EnchantService::socketManastone(player(), stone, sword, Ptr<Item>(supplements), 1), expected);
	EXPECT_EQ(sent(), cp::exactly({message(std::string(expected ? "Success" : "Fail") + " (success chance:65.0%)")}));
	player().updateSupplements();
	EXPECT_EQ(cubeCount(LESSER_SUPPLEMENTS), 0) << "the 5 queued";
}

// ------------------------------------------------------------------------------------------------------------------------- socketManastoneAct

TEST_F(EnchantServiceTest, ASocketedStoneTakesTheFirstFreeSlotAndTheStoneIsUsedUp) {
	// EnchantService.java:404-424: one stone used up, STR_GIVE_ITEM_OPTION_SUCCEED, ItemSocketService.addManaStone into the first free normal
	// slot (slot 0 of the Training Sword, a NEW stone), then the STATS_CHANGE update of the item; true
	Item& stones = inCube(826001, MANASTONE_HP_20, 2);
	Item& sword = inCube(826002, TRAINING_SWORD, 1);
	EXPECT_TRUE(EnchantService::socketManastoneAct(player(), stones, sword, stones, 1, true));
	EXPECT_EQ(stones.getItemCount(), 1);
	ASSERT_EQ(stonesOf(sword), (std::vector<std::pair<int32_t, int32_t>>{{0, MANASTONE_HP_20}})) << "before front() below";
	EXPECT_EQ(sword.getItemStones()->snapshot().front()->getPersistentState(), PersistentState::NEW);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 3u);
	EXPECT_EQ(packets[0], serialized(SM_INVENTORY_UPDATE_ITEM(player(), stones, ItemUpdateType::DEC_ITEM_USE)));
	EXPECT_EQ(packets[1], serialized(SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_OPTION_SUCCEED(sword.getL10n())));
	EXPECT_EQ(packets[2], serialized(SM_INVENTORY_UPDATE_ITEM(player(), sword, ItemUpdateType::STATS_CHANGE)));
}

TEST_F(EnchantServiceTest, AStoneSocketedIntoAnEquippedSwordAddsItsStatsAtOnce) {
	// EnchantService.java:412-416: for an equipped target (EnchantItemAction.java:116 lets one be socketed) the new stone's modifiers go to the
	// player's stats through ItemEquipmentListener.addStoneStats and the stats are updated. Manastone HP +20 (item_templates.xml:839282, MAXHP
	// 20) into the equipped Training Sword: max HP + 20
	player().setSkillList(model::skill::PlayerSkillList::create(
		{model::skill::PlayerSkillEntry::create(37, 1, 0, model::gameobjects::Persistable_PersistentState::UPDATED)}));
	Ref<Item> sword = Item::create(826201, TRAINING_SWORD, 1, std::nullopt, 0, "", 0, 0, true, false, 1,
		model::items::storage::getId(StorageType::CUBE), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, 0);
	items.push_back(sword);
	player().getEquipment().onLoadHandler(*sword);
	ASSERT_EQ(player().getEquipment().getMainHandWeapon().get(), sword.get());
	Item& stones = inCube(826202, MANASTONE_HP_20, 1);
	const int32_t maxHp = player().getGameStats()->getMaxHp()->getCurrent();

	EXPECT_TRUE(EnchantService::socketManastoneAct(player(), stones, *sword, stones, 1, true));
	EXPECT_EQ(stonesOf(*sword), (std::vector<std::pair<int32_t, int32_t>>{{0, MANASTONE_HP_20}}));
	EXPECT_EQ(player().getGameStats()->getMaxHp()->getCurrent() - maxHp, 20);
}

TEST_F(EnchantServiceTest, AFailedSocketingUsesTheStoneAndSocketsNothing) {
	// EnchantService.java:417-423: removeRemainingTuningCountIfPossible, STR_GIVE_ITEM_OPTION_FAILED, the update; and without the stone in the
	// cube (:405-406) false before anything
	Item& stones = inCube(826101, MANASTONE_HP_20, 2);
	Item& sword = inCube(826102, TRAINING_SWORD, 1);
	EXPECT_TRUE(EnchantService::socketManastoneAct(player(), stones, sword, stones, 1, false));
	EXPECT_EQ(stones.getItemCount(), 1);
	EXPECT_TRUE(stonesOf(sword).empty());
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 3u);
	EXPECT_EQ(packets[1], serialized(SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_OPTION_FAILED(sword.getL10n())));
	EXPECT_EQ(packets[2], serialized(SM_INVENTORY_UPDATE_ITEM(player(), sword, ItemUpdateType::STATS_CHANGE)));

	Ref<Item> loose = itemRow(826103, MANASTONE_HP_20, 1);
	clearSent();
	EXPECT_FALSE(EnchantService::socketManastoneAct(player(), *loose, sword, *loose, 1, true));
	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(stonesOf(sword).empty());
}

// ------------------------------------------------------------------------------------------------------------------------- addManaStone

TEST_F(EnchantServiceTest, SpecialStonesTakeTheSpecialSlotsAndNormalStonesTheRest) {
	// ItemSocketService.java:30-92: the test spellbook has 6 sockets, 2 of them special (m_slots 6, s_slots 2, item_templates.xml:72382). A
	// normal stone (HP +20) takes the first free slot from 2, a special one (Ancient Manastone, SPECIAL_MANASTONE, :846255) the first free
	// slot from 0; a third special stone or a fifth normal one finds no slot -> null
	Item& book = inCube(827001, ANCIENT_MANASTONE_TEST_SPELLBOOK, 1);
	Ptr<Item> bookPtr(book);
	ASSERT_TRUE(ItemSocketService::addManaStone(bookPtr, MANASTONE_HP_20, false));
	ASSERT_TRUE(ItemSocketService::addManaStone(bookPtr, ANCIENT_MANASTONE_HP_105, false));
	ASSERT_TRUE(ItemSocketService::addManaStone(bookPtr, MANASTONE_HP_20, false));
	ASSERT_TRUE(ItemSocketService::addManaStone(bookPtr, ANCIENT_MANASTONE_HP_105, false));
	EXPECT_FALSE(ItemSocketService::addManaStone(bookPtr, ANCIENT_MANASTONE_HP_105, false)) << "both special slots taken";
	ASSERT_TRUE(ItemSocketService::addManaStone(bookPtr, MANASTONE_HP_20, false));
	ASSERT_TRUE(ItemSocketService::addManaStone(bookPtr, MANASTONE_HP_20, false));
	EXPECT_FALSE(ItemSocketService::addManaStone(bookPtr, MANASTONE_HP_20, false)) << "the four normal slots taken";
	EXPECT_EQ(stonesOf(book), (std::vector<std::pair<int32_t, int32_t>>{{0, ANCIENT_MANASTONE_HP_105}, {1, ANCIENT_MANASTONE_HP_105},
								  {2, MANASTONE_HP_20}, {3, MANASTONE_HP_20}, {4, MANASTONE_HP_20}, {5, MANASTONE_HP_20}}));

	// a special stone for an item without special slots (Training Sword) -> null; a null item -> null
	Item& sword = inCube(827002, TRAINING_SWORD, 1);
	EXPECT_FALSE(ItemSocketService::addManaStone(Ptr<Item>(sword), ANCIENT_MANASTONE_HP_105, false));
	EXPECT_FALSE(ItemSocketService::addManaStone(nullptr, MANASTONE_HP_20, false));
	EXPECT_TRUE(stonesOf(sword).empty());
	EXPECT_TRUE(sent().empty());
}

TEST_F(EnchantServiceTest, AStoneForAChosenSlotStopsAtSixStones) {
	// ItemSocketService.java:42-49: the slot overload takes any slot while fewer than Item.MAX_BASIC_STONES (6) stones are socketed, whatever the
	// item's sockets
	Item& sword = inCube(827101, TRAINING_SWORD, 1);
	Ptr<Item> swordPtr(sword);
	for (int32_t slot = 0; slot < 6; slot++)
		ASSERT_TRUE(ItemSocketService::addManaStone(swordPtr, MANASTONE_HP_20, slot, false)) << slot;
	EXPECT_FALSE(ItemSocketService::addManaStone(swordPtr, MANASTONE_HP_20, 7, false));
	EXPECT_EQ(stonesOf(sword).size(), 6u);
	EXPECT_EQ(stonesOf(sword).back(), (std::pair<int32_t, int32_t>{5, MANASTONE_HP_20}));
}

TEST_F(EnchantServiceTest, AnItemCopiedFromASocketedSourceGetsItsStones) {
	// W-25 (m5c-plan.md §0 A-02): ItemService.addItem(player, sourceItem) -> copyItemInfo -> ItemSocketService.addManaStone per source stone
	// (ItemService.java:126-128, the buy-back and private-store path): the new sword in the cube carries the HP +20 in slot 0. The source's stone
	// is put in as ItemStoneListDAO.load puts a stored one, so that only the copy goes through addManaStone
	Ref<Item> source = itemRow(827201, TRAINING_SWORD, 1);
	source->getItemStones()->add(ManaStone::create(827201, MANASTONE_HP_20, 0, PersistentState::UPDATED));
	ASSERT_TRUE(source->hasManaStones());
	ASSERT_EQ(ItemService::addItem(player(), *source), 0);
	std::vector<Ptr<Item>> swords = player().getInventory().getItemsByItemId(TRAINING_SWORD);
	ASSERT_EQ(swords.size(), 1u);
	EXPECT_NE(swords[0]->getObjectId(), 827201) << "a new item";
	EXPECT_EQ(stonesOf(*swords[0]), (std::vector<std::pair<int32_t, int32_t>>{{0, MANASTONE_HP_20}}));
}

TEST_F(EnchantServiceTest, CopyFusionStonesCopiesTheSourceStonesAsNewFusionStones) {
	// ItemSocketService.java:94-100: each item stone of the source becomes a NEW fusion stone of the target in the same slot
	Item& source = inCube(827301, TAHABATA_SWORD, 1);
	Item& target = inCube(827302, TAHABATA_SWORD, 1);
	ASSERT_TRUE(ItemSocketService::addManaStone(Ptr<Item>(source), MANASTONE_HP_20, false));
	ASSERT_TRUE(ItemSocketService::addManaStone(Ptr<Item>(source), MANASTONE_HP_60, false));
	ItemSocketService::copyFusionStones(source, target);
	EXPECT_EQ(stonesOf(target, true), (std::vector<std::pair<int32_t, int32_t>>{{0, MANASTONE_HP_20}, {1, MANASTONE_HP_60}}));
	for (const Ptr<ManaStone>& stone : target.getFusionStones()->snapshot())
		EXPECT_EQ(stone->getPersistentState(), PersistentState::NEW);
	EXPECT_TRUE(stonesOf(target).empty());
	EXPECT_EQ(stonesOf(source).size(), 2u);
}

TEST_F(EnchantServiceTest, AFailureAndEveryNewStoneUseUpTheTuningsLeft) {
	// Item.java:838-841: removeRemainingTuningCountIfPossible sets an identified item's (tune_count != -1) tune count to its template's maximum
	// when the template can be tuned. Tune, Retune_Test_Option (item_templates.xml:9614: rnd_count 1) is loaded identified with tune_count 0.
	// Called by enchantItemAct's failure of an item without enchant_type (EnchantService.java:227-229; seen at +0, since any level above 0 uses
	// the tunings up in Item.setEnchantLevel, Item.java:496-500), by socketManastoneAct's failure (:418), for every stone inserted
	// (insertManastoneIntoSlot, ItemSocketService.java:88: a socketing success) and for the fusion stones copied (copyFusionStones, :98)
	Item& stones = inCube(827401, ALPHA_ENCHANTMENT_STONE, 1);
	Item& manastones = inCube(827402, MANASTONE_HP_20, 2);
	Item& notEnchanted = inCube(827404, TUNE_RETUNE_TEST_OPTION, 1);
	Item& notSocketed = inCube(827405, TUNE_RETUNE_TEST_OPTION, 1);
	Item& socketed = inCube(827406, TUNE_RETUNE_TEST_OPTION, 1);
	Item& fused = inCube(827407, TUNE_RETUNE_TEST_OPTION, 1);
	Item& source = inCube(827408, TAHABATA_SWORD, 1);
	ASSERT_TRUE(ItemSocketService::addManaStone(Ptr<Item>(source), MANASTONE_HP_20, false));
	for (Item* item : {&notEnchanted, &notSocketed, &socketed, &fused})
		ASSERT_EQ(item->getTuneCount(), 0);

	EnchantService::enchantItemAct(player(), stones, notEnchanted, stones, 0, false);
	EXPECT_EQ(notEnchanted.getEnchantLevel(), 0);
	EXPECT_EQ(notEnchanted.getTuneCount(), 1);
	EXPECT_TRUE(EnchantService::socketManastoneAct(player(), manastones, notSocketed, manastones, 1, false));
	EXPECT_TRUE(stonesOf(notSocketed).empty());
	EXPECT_EQ(notSocketed.getTuneCount(), 1);
	EXPECT_TRUE(EnchantService::socketManastoneAct(player(), manastones, socketed, manastones, 1, true));
	EXPECT_EQ(stonesOf(socketed).size(), 1u);
	EXPECT_EQ(socketed.getTuneCount(), 1);
	ItemSocketService::copyFusionStones(source, fused);
	EXPECT_EQ(stonesOf(fused, true).size(), 1u);
	EXPECT_EQ(fused.getTuneCount(), 1);
}

// ------------------------------------------------------------------------------------------------------------------------- removeManastone

TEST_F(EnchantServiceTest, RemovingAStoneCostsTheServicePriceOf650) {
	// ItemSocketService.java:102-138: PricesService.getPriceForService(650, ELYOS) with the shipped prices 100/100/100 (prices.properties) and no
	// siege influence (Influence 0, PricesService.java getGlobalPrices/getTaxes: prices 125, taxes 113): (long) (650 * 125 / 100D) = 812,
	// 812 * 100 / 100 = 812, (long) (812 * 113 / 100D) = 917. With 916 kinah -> STR_REMOVE_ITEM_OPTION_NOT_ENOUGH_GOLD and the stone stays;
	// with 1000 -> 917 taken, the stone of the slot removed, STR_REMOVE_ITEM_OPTION_SUCCEED and the item's update
	ASSERT_EQ(trade::PricesService::getPriceForService(650, model::Race::ELYOS), 917);
	Item& sword = inCube(828001, TAHABATA_SWORD, 1);
	ASSERT_TRUE(ItemSocketService::addManaStone(Ptr<Item>(sword), MANASTONE_HP_20, false));
	ASSERT_TRUE(ItemSocketService::addManaStone(Ptr<Item>(sword), MANASTONE_HP_60, false));
	storage(StorageType::CUBE).onLoadHandler(*loadedItem(828002, KINAH, 916, StorageType::CUBE));
	ASSERT_EQ(player().getInventory().getKinah(), 916);

	ItemSocketService::removeManastone(player(), 828001, 1, false);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_REMOVE_ITEM_OPTION_NOT_ENOUGH_GOLD(sword.getL10n()))}));
	EXPECT_EQ(stonesOf(sword).size(), 2u);
	EXPECT_EQ(player().getInventory().getKinah(), 916);

	player().getInventory().increaseKinah(84);
	clearSent();
	ItemSocketService::removeManastone(player(), 828001, 1, false);
	EXPECT_EQ(player().getInventory().getKinah(), 83);
	EXPECT_EQ(stonesOf(sword), (std::vector<std::pair<int32_t, int32_t>>{{0, MANASTONE_HP_20}}));
	std::vector<std::vector<uint8_t>> messages = sentWithOpcode(SM_SYSTEM_MESSAGE_OPCODE);
	ASSERT_EQ(messages.size(), 1u);
	EXPECT_EQ(messages[0], serialized(SM_SYSTEM_MESSAGE::STR_REMOVE_ITEM_OPTION_SUCCEED(sword.getL10n())));
	std::vector<std::vector<uint8_t>> updates = sentWithOpcode(SM_INVENTORY_UPDATE_ITEM_OPCODE);
	ASSERT_EQ(updates.size(), 2u) << "the kinah, then the sword";
	EXPECT_EQ(updates[1], serialized(SM_INVENTORY_UPDATE_ITEM(player(), sword)));
}

TEST_F(EnchantServiceTest, ARemovedStoneIsDeletedFromTheDatabase) {
	// ItemSocketService.java:124-131 and m5c-plan.md D11 (ItemStoneListDAO.storeManaStones is one of the milestone's first writes): the stone is
	// marked DELETED before ItemStoneListDAO.storeManaStones, which deletes its item_stones row (ItemStoneListDAO.java:134). The rows are loaded
	// by ItemStoneListDAO.load (state UPDATED), as at enter world; the test database of the DAO tests (tests/dao/DaoTestDatabase.h)
	if (!dao::test::isEnabled())
		GTEST_SKIP() << "AION_TEST_GS_DATABASE_URL is not set";
	dao::test::setUpDatabaseOnce();
	dao::test::clearTables();
	dao::test::execute("INSERT INTO inventory (item_unique_id, item_id, item_owner) VALUES (828301, 100000768, 700101)");
	dao::test::execute("INSERT INTO item_stones (item_unique_id, item_id, slot, category, polishNumber, polishCharge) VALUES "
					   "(828301, 167000226, 0, 0, 0, 0), (828301, 167000354, 1, 0, 0, 0)");
	Item& sword = inCube(828301, TAHABATA_SWORD, 1);
	dao::ItemStoneListDAO::load({Ptr<Item>(sword)});
	ASSERT_EQ(stonesOf(sword), (std::vector<std::pair<int32_t, int32_t>>{{0, MANASTONE_HP_20}, {1, MANASTONE_HP_60}}));
	storage(StorageType::CUBE).onLoadHandler(*loadedItem(828302, KINAH, 1000, StorageType::CUBE));

	ItemSocketService::removeManastone(player(), 828301, 1, false);
	EXPECT_EQ(stonesOf(sword), (std::vector<std::pair<int32_t, int32_t>>{{0, MANASTONE_HP_20}}));
	EXPECT_EQ(dao::test::queryLong("SELECT COUNT(*) FROM item_stones WHERE item_unique_id = 828301 AND slot = 1"), 0);
	EXPECT_EQ(dao::test::queryLong("SELECT item_id FROM item_stones WHERE item_unique_id = 828301 AND slot = 0"), MANASTONE_HP_20);
	EXPECT_EQ(player().getInventory().getKinah(), 83);
}

TEST_F(EnchantServiceTest, RemovingAStoneRefusesAMissingItemAStonelessItemAndAnEmptySlot) {
	// ItemSocketService.java:104-121, each with its message and nothing charged: no such item in the cube; an item without stones (of the asked
	// kind: a fusion removal from an item with only item stones); no stone in the slot
	Item& sword = inCube(828101, TAHABATA_SWORD, 1);
	Item& bare = inCube(828102, TRAINING_SWORD, 1);
	ASSERT_TRUE(ItemSocketService::addManaStone(Ptr<Item>(sword), MANASTONE_HP_20, false));
	storage(StorageType::CUBE).onLoadHandler(*loadedItem(828103, KINAH, 5000, StorageType::CUBE));

	ItemSocketService::removeManastone(player(), 828199, 0, false);
	ItemSocketService::removeManastone(player(), 828102, 0, false);
	ItemSocketService::removeManastone(player(), 828101, 0, true);
	ItemSocketService::removeManastone(player(), 828101, 3, false);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_REMOVE_ITEM_OPTION_NO_TARGET_ITEM()),
						  serialized(SM_SYSTEM_MESSAGE::STR_REMOVE_ITEM_OPTION_NO_OPTION_TO_REMOVE(bare.getL10n())),
						  serialized(SM_SYSTEM_MESSAGE::STR_REMOVE_ITEM_OPTION_NO_OPTION_TO_REMOVE(sword.getL10n())),
						  serialized(SM_SYSTEM_MESSAGE::STR_REMOVE_ITEM_OPTION_INVALID_OPTION_SLOT_NUMBER(sword.getL10n()))}));
	EXPECT_EQ(player().getInventory().getKinah(), 5000);
	EXPECT_EQ(stonesOf(sword).size(), 1u);
}

TEST_F(EnchantServiceTest, RemoveAllManastoneEmptiesTheItemsStones) {
	// ItemSocketService.java:140-151: every stone DELETED and stored, the set cleared, the item's update; a null or stoneless item does nothing
	Item& sword = inCube(828201, TAHABATA_SWORD, 1);
	ASSERT_TRUE(ItemSocketService::addManaStone(Ptr<Item>(sword), MANASTONE_HP_20, false));
	ASSERT_TRUE(ItemSocketService::addManaStone(Ptr<Item>(sword), MANASTONE_HP_60, false));
	ItemSocketService::removeAllManastone(player(), nullptr);
	ItemSocketService::removeAllManastone(player(), Ptr<Item>(inCube(828202, TRAINING_SWORD, 1)));
	EXPECT_TRUE(sent().empty());

	ItemSocketService::removeAllManastone(player(), Ptr<Item>(sword));
	EXPECT_TRUE(stonesOf(sword).empty());
	EXPECT_FALSE(sword.hasManaStones());
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_INVENTORY_UPDATE_ITEM(player(), sword))}));
}

// ------------------------------------------------------------------------------------------------------------------------- amplifyItem

TEST_F(EnchantServiceTest, AmplificationNeedsAMaxedExceedableItemAndItsMaterial) {
	// EnchantService.java:549-590, in order: a null player does nothing; a missing target, material or tool -> STR_MSG_EXCEED_NO_TARGET_ITEM;
	// already amplified -> STR_MSG_EXCEED_ALREADY; no can_exceed_enchant (Training Sword) -> STR_MSG_EXCEED_CANNOT_01; below the item's
	// maximum (Tahabata's Sword at +14 of 15) -> STR_MSG_EXCEED_CANNOT_02; a material that is neither the same item nor an Amplification
	// Stone (166500002, item_templates.xml:839275) -> STR_MSG_EXCEED_NO_TARGET_ITEM. Nothing is used up
	Item& material = inCube(829001, AMPLIFICATION_STONE, 2);
	Item& tool = inCube(829002, ALPHA_ENCHANTMENT_STONE, 2);
	Item& amplified = inCube(829003, TAHABATA_SWORD, 1, 15, 0, true);
	Item& training = inCube(829004, TRAINING_SWORD, 1, 10);
	Item& belowMax = inCube(829005, TAHABATA_SWORD, 1, 14);
	Item& maxed = inCube(829006, TAHABATA_SWORD, 1, 15);
	Item& wrongMaterial = inCube(829007, MANASTONE_HP_20, 1);

	EnchantService::amplifyItem(nullptr, 829006, 829001, 829002);
	EXPECT_TRUE(sent().empty());
	Ptr<model::gameobjects::player::Player> self(player());
	EnchantService::amplifyItem(self, 829099, 829001, 829002);
	EnchantService::amplifyItem(self, 829006, 829099, 829002);
	EnchantService::amplifyItem(self, 829006, 829001, 829099);
	EnchantService::amplifyItem(self, 829003, 829001, 829002);
	EnchantService::amplifyItem(self, 829004, 829001, 829002);
	EnchantService::amplifyItem(self, 829005, 829001, 829002);
	EnchantService::amplifyItem(self, 829006, 829007, 829002);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_MSG_EXCEED_NO_TARGET_ITEM()),
						  serialized(SM_SYSTEM_MESSAGE::STR_MSG_EXCEED_NO_TARGET_ITEM()), serialized(SM_SYSTEM_MESSAGE::STR_MSG_EXCEED_NO_TARGET_ITEM()),
						  serialized(SM_SYSTEM_MESSAGE::STR_MSG_EXCEED_ALREADY()),
						  serialized(SM_SYSTEM_MESSAGE::STR_MSG_EXCEED_CANNOT_01(training.getL10n())),
						  serialized(SM_SYSTEM_MESSAGE::STR_MSG_EXCEED_CANNOT_02()), serialized(SM_SYSTEM_MESSAGE::STR_MSG_EXCEED_NO_TARGET_ITEM())}));
	EXPECT_EQ(material.getItemCount(), 2);
	EXPECT_EQ(tool.getItemCount(), 2);
	EXPECT_EQ(wrongMaterial.getItemCount(), 1);
	EXPECT_FALSE(maxed.isAmplified());
	EXPECT_FALSE(belowMax.isAmplified());
	EXPECT_TRUE(amplified.isAmplified());
}

TEST_F(EnchantServiceTest, AmplificationUsesTheMaterialAndTheToolAndAmplifiesTheItem) {
	// EnchantService.java:580-589: one material and one tool used up, amplified, STR_MSG_EXCEED_SUCCEED, the item's update
	Item& material = inCube(829101, AMPLIFICATION_STONE, 2);
	Item& tool = inCube(829102, ALPHA_ENCHANTMENT_STONE, 2);
	Item& sword = inCube(829103, TAHABATA_SWORD, 1, 15);
	EnchantService::amplifyItem(Ptr<model::gameobjects::player::Player>(player()), 829103, 829101, 829102);
	EXPECT_TRUE(sword.isAmplified());
	EXPECT_EQ(material.getItemCount(), 1);
	EXPECT_EQ(tool.getItemCount(), 1);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 4u);
	EXPECT_EQ(packets[2], serialized(SM_SYSTEM_MESSAGE::STR_MSG_EXCEED_SUCCEED(sword.getL10n())));
	EXPECT_EQ(packets[3], serialized(SM_INVENTORY_UPDATE_ITEM(player(), sword)));
}

TEST_F(EnchantServiceTest, AnEquippedItemIsAmplifiedAndSavedWithTheEquipment) {
	// EnchantService.java:552-555, 580-586: the target is looked up in the equipment first, and an amplified equipped item marks the equipment,
	// not the cube, for the update. Tahabata's Sword at +15 in the main hand
	Item& sword = equippedSword(829201, TAHABATA_SWORD, 15);
	Item& material = inCube(829202, AMPLIFICATION_STONE, 1);
	Item& tool = inCube(829203, ALPHA_ENCHANTMENT_STONE, 1);
	player().getEquipment().setPersistentState(PersistentState::UPDATED);
	clearSent();
	EnchantService::amplifyItem(Ptr<model::gameobjects::player::Player>(player()), 829201, 829202, 829203);
	EXPECT_TRUE(sword.isAmplified());
	EXPECT_EQ(material.getItemCount(), 0);
	EXPECT_EQ(tool.getItemCount(), 0);
	EXPECT_EQ(player().getEquipment().getPersistentState(), PersistentState::UPDATE_REQUIRED);
	std::vector<std::vector<uint8_t>> messages = sentWithOpcode(SM_SYSTEM_MESSAGE_OPCODE);
	ASSERT_EQ(messages.size(), 1u);
	EXPECT_EQ(messages[0], serialized(SM_SYSTEM_MESSAGE::STR_MSG_EXCEED_SUCCEED(sword.getL10n())));
}

} // namespace
} // namespace aion::gameserver::services::item::test
