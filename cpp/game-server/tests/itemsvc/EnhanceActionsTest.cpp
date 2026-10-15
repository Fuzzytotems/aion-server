// P5-07 item actions of m5c-plan.md E-03/E-04: EnchantItemAction (canAct, both acts, the ItemUseObserver and the 4 s / 2 s task on a target in
// the cube or equipped, the supplements it hands on, isSuccess, checkSupplementLevel, isSupplementAction), ExtractAction (canAct, the 5 s task
// and its observer), DecomposeAction (canAct, the fixed, race-restricted, random and selectable arms, postValidate, the cooldown, the
// casting-delay task and its observer, and the use without a casting delay) and RemodelAction, against
// EnchantItemAction.java:44-220, ExtractAction.java:25-68, DecomposeAction.java:99-402 and RemodelAction.java:19-26. The tasks run on the
// fixture's DeterministicExecutor; Rnd is seeded and the same draws, taken again from the same seed, predict each outcome. Tests of E-05.

#include "EnchantTestSupport.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/model/items/ManaStone.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/ResultedItem.h"
#include "aion/gameserver/model/templates/item/actions/DecomposeAction.h"
#include "aion/gameserver/model/templates/item/actions/EnchantItemAction.h"
#include "aion/gameserver/model/templates/item/actions/ExtractAction.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/model/templates/item/actions/RemodelAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FIRST_SHOW_DECOMPOSABLE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_ADD_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/item/ItemPacketService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::item::test {
namespace {

using namespace std::chrono_literals;
namespace Rnd = commons::utils::Rnd;
using model::templates::item::actions::DecomposeAction;
using model::templates::item::actions::EnchantItemAction;
using model::templates::item::actions::ExtractAction;
using model::templates::item::actions::RemodelAction;
using network::aion::serverpackets::SM_FIRST_SHOW_DECOMPOSABLE;
using network::aion::serverpackets::SM_INVENTORY_ADD_ITEM;
using network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM;
using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using ItemUpdateType = ItemPacketService::ItemUpdateType;

/** the fixture player's object id (ItemServicesTest::SetUp) */
constexpr int32_t PLAYER_OBJECT_ID = 700101;

class EnhanceActionsTest : public EnhanceTest {
protected:
	const model::templates::item::ItemTemplate& templateOf(int32_t itemId) {
		const model::templates::item::ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
		EXPECT_TRUE(itemTemplate) << itemId;
		return *itemTemplate;
	}

	const EnchantItemAction& enchantActionOf(int32_t itemId) {
		const EnchantItemAction* action = templateOf(itemId).getActions()->getEnchantAction();
		EXPECT_TRUE(action) << itemId;
		return *action;
	}

	/** The only bound action of the item, as the action class its XML element binds */
	template <class A>
	const A& onlyActionOf(int32_t itemId) {
		const auto& list = templateOf(itemId).getActions()->getItemActions();
		EXPECT_EQ(list.size(), 1u) << itemId;
		const A* action = dynamic_cast<const A*>(list.front().get());
		EXPECT_TRUE(action) << itemId;
		return *action;
	}

	std::vector<uint8_t> animation(int32_t itemObjId, int32_t itemId, int32_t time, int32_t end) {
		return serialized(SM_ITEM_USAGE_ANIMATION(PLAYER_OBJECT_ID, itemObjId, itemId, time, end, 0));
	}

	std::vector<uint8_t> systemMessage(SM_SYSTEM_MESSAGE&& packet) { return serialized(std::move(packet)); }
};

// ------------------------------------------------------------------------------------------------------------------------- EnchantItemAction

TEST_F(EnhanceActionsTest, TheEnchantActionAcceptsOnlyStonesOnWeaponsAndArmour) {
	// EnchantItemAction.java:45-78, in order: a supplement's action (Lesser Supplements, chance 5, item_templates.xml:838880) never acts; a null
	// parent -> false; a null target -> the message of the stone's group; for an enchantment stone: a no-enchant target (Circulus' Sword, mask
	// bit 9, :3) -> false without a message; max_enchant 0 and not exceedable (the potion, :830724) -> IT_CAN_NOT_BE_GIVEN_OPTION; at the
	// maximum (+10 of 10) or at 255 -> IT_CAN_NOT_BE_GIVEN_OPTION_MORE_TIME; amplified and no Omega stone -> STR_MSG_EXCEED_CANNOT_02; then the
	// ids: stone 166xxxxxx or 167xxxxxx on a target below 120xxxxxx
	const EnchantItemAction& alpha = enchantActionOf(ALPHA_ENCHANTMENT_STONE);
	const EnchantItemAction& manastone = enchantActionOf(MANASTONE_HP_20);
	EXPECT_TRUE(enchantActionOf(LESSER_SUPPLEMENTS).isSupplementAction());
	EXPECT_TRUE(enchantActionOf(MANASTONE_ONLY_SAND).isSupplementAction());
	EXPECT_EQ(enchantActionOf(MANASTONE_SOCKETING_SUPPLEMENTS_31_50).getMinLevel(), 31);
	EXPECT_EQ(enchantActionOf(MANASTONE_SOCKETING_SUPPLEMENTS_31_50).getMaxLevel(), 50);
	EXPECT_FALSE(alpha.isSupplementAction());
	EXPECT_EQ(alpha.getMinLevel(), 0);
	EXPECT_EQ(alpha.getMaxLevel(), 0);

	Item& stone = inCube(830001, ALPHA_ENCHANTMENT_STONE, 5);
	Item& manastoneItem = inCube(830002, MANASTONE_HP_20, 5);
	Item& supplement = inCube(830003, LESSER_SUPPLEMENTS, 5);
	Item& sword = inCube(830004, TRAINING_SWORD, 1);
	Item& circulus = inCube(830005, CIRCULUS_SWORD, 1);
	Item& potion = inCube(830006, MINOR_LIFE_POTION, 1);
	Item& maxed = inCube(830007, TRAINING_SWORD, 1, 10);
	Item& at255 = inCube(830008, TAHABATA_SWORD, 1, 255, 0, true);
	Item& amplified = inCube(830009, TAHABATA_SWORD, 1, 16, 0, true);
	Item& tunic = inCube(830010, PLAINSMANS_TUNIC, 1);
	Item& hat = inCube(830011, RATTAN_HAT, 1);
	Item& tools = inCube(830012, EXTRACTION_TOOLS, 1);

	EXPECT_FALSE(enchantActionOf(LESSER_SUPPLEMENTS).canAct(player(), Ptr<Item>(supplement), Ptr<Item>(sword)));
	EXPECT_FALSE(alpha.canAct(player(), nullptr, Ptr<Item>(sword)));
	EXPECT_TRUE(sent().empty());
	EXPECT_FALSE(alpha.canAct(player(), Ptr<Item>(stone), nullptr));
	EXPECT_FALSE(manastone.canAct(player(), Ptr<Item>(manastoneItem), nullptr));
	EXPECT_FALSE(alpha.canAct(player(), Ptr<Item>(stone), Ptr<Item>(circulus)));
	EXPECT_FALSE(alpha.canAct(player(), Ptr<Item>(stone), Ptr<Item>(potion)));
	EXPECT_FALSE(alpha.canAct(player(), Ptr<Item>(stone), Ptr<Item>(maxed)));
	EXPECT_FALSE(alpha.canAct(player(), Ptr<Item>(stone), Ptr<Item>(at255)));
	EXPECT_FALSE(alpha.canAct(player(), Ptr<Item>(stone), Ptr<Item>(amplified)));
	const std::string stoneName = templateOf(ALPHA_ENCHANTMENT_STONE).getL10n();
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_ENCHANT_ITEM_NO_TARGET_ITEM()),
						  systemMessage(SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_OPTION_NO_TARGET_ITEM()),
						  systemMessage(SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_OPTION_IT_CAN_NOT_BE_GIVEN_OPTION(templateOf(MINOR_LIFE_POTION).getL10n(), stoneName)),
						  systemMessage(SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_OPTION_IT_CAN_NOT_BE_GIVEN_OPTION_MORE_TIME(templateOf(TRAINING_SWORD).getL10n(), stoneName)),
						  systemMessage(SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_OPTION_IT_CAN_NOT_BE_GIVEN_OPTION_MORE_TIME(templateOf(TAHABATA_SWORD).getL10n(), stoneName)),
						  systemMessage(SM_SYSTEM_MESSAGE::STR_MSG_EXCEED_CANNOT_02(stoneName))}));

	clearSent();
	EXPECT_TRUE(alpha.canAct(player(), Ptr<Item>(stone), Ptr<Item>(sword)));
	EXPECT_TRUE(manastone.canAct(player(), Ptr<Item>(manastoneItem), Ptr<Item>(tunic)));
	EXPECT_FALSE(manastone.canAct(player(), Ptr<Item>(manastoneItem), Ptr<Item>(hat))) << "a target of 125xxxxxx";
	EXPECT_FALSE(manastone.canAct(player(), Ptr<Item>(tools), Ptr<Item>(sword))) << "a parent of 165xxxxxx";
	EXPECT_TRUE(sent().empty());
}

TEST_F(EnhanceActionsTest, AnEnchantmentStoneEnchantsAfterFourSeconds) {
	// EnchantItemAction.java:81-141: the varargs act calls the overload with no supplement and weapon 1; the observer is attached, isSuccess
	// rolls (Rnd.chance() < 80 for the Training Sword, EnchantServiceTest), the 4,000 ms animation is broadcast, and the ITEM_USE task then
	// removes the observer, finds the target, enchantItemAct uses one stone and adds the crit levels (the second roll), and the closing
	// animation says 1 for a success and 2 for a failure
	Item& stones = inCube(831001, ALPHA_ENCHANTMENT_STONE, 10);
	Item& sword = inCube(831002, TRAINING_SWORD, 1);
	const EnchantItemAction& alpha = enchantActionOf(ALPHA_ENCHANTMENT_STONE);
	for (uint64_t seed : {1u, 2u, 3u, 4u, 5u, 6u}) {
		SCOPED_TRACE(seed);
		const int32_t before = sword.getEnchantLevel();
		const int64_t stonesBefore = stones.getItemCount();
		Rnd::seedCurrentThreadForTests(seed);
		const bool success = Rnd::chance() < 80.0f;
		const float crit = Rnd::chance();
		const int32_t add = crit < 5 ? 3 : crit < 10 ? 2 : 1;
		const int32_t expected = success ? std::min(before + add, 10) : std::max(before - 1, 0);

		clearSent();
		Rnd::seedCurrentThreadForTests(seed);
		alpha.act(player(), Ptr<Item>(stones), Ptr<Item>(sword));
		EXPECT_EQ(sent(), cp::exactly({serialized(SM_ITEM_USAGE_ANIMATION(PLAYER_OBJECT_ID, 831002, 831001, ALPHA_ENCHANTMENT_STONE, 4000, 0, 0, 1, 0, 0))}));
		clearSent();
		executor->advance(3999ms);
		EXPECT_TRUE(sent().empty());
		EXPECT_EQ(sword.getEnchantLevel(), before);

		executor->advance(1ms);
		EXPECT_EQ(sword.getEnchantLevel(), expected);
		EXPECT_EQ(stones.getItemCount(), stonesBefore - 1);
		std::vector<std::vector<uint8_t>> packets = sent();
		ASSERT_GE(packets.size(), 2u);
		EXPECT_EQ(packets.back(), animation(831001, ALPHA_ENCHANTMENT_STONE, 0, success ? 1 : 2));
		EXPECT_EQ(packets[packets.size() - 2], success
			? systemMessage(SM_SYSTEM_MESSAGE::STR_MSG_ENCHANT_ITEM_SUCCEED_NEW(sword.getL10n(), expected))
			: systemMessage(SM_SYSTEM_MESSAGE::STR_ENCHANT_ITEM_FAILED(sword.getL10n())));

		clearSent();
		player().getObserveController()->notifyMoveObservers();
		EXPECT_TRUE(sent().empty()) << "the task removed its observer";
		if (sword.getEnchantLevel() >= 10)
			break;
	}
}

TEST_F(EnhanceActionsTest, AMoveDuringTheEnchantmentAbortsIt) {
	// EnchantItemAction.java:93-103: the observer's abort cancels the ITEM_USE task, sends STR_ENCHANT_ITEM_CANCELED (an enchantment stone) or
	// STR_GIVE_ITEM_OPTION_CANCELED (a manastone) and the aborted animation (end 3), and removes itself; nothing is used up
	Item& stones = inCube(831101, ALPHA_ENCHANTMENT_STONE, 1);
	Item& manastones = inCube(831102, MANASTONE_HP_20, 1);
	Item& sword = inCube(831103, TRAINING_SWORD, 1, 3);
	Item& tunic = inCube(831104, PLAINSMANS_TUNIC, 1);

	enchantActionOf(ALPHA_ENCHANTMENT_STONE).act(player(), Ptr<Item>(stones), Ptr<Item>(sword));
	executor->advance(2000ms);
	clearSent();
	player().getObserveController()->notifyMoveObservers();
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_ENCHANT_ITEM_CANCELED(sword.getL10n())),
						  animation(831101, ALPHA_ENCHANTMENT_STONE, 0, 3)}));

	enchantActionOf(MANASTONE_HP_20).act(player(), Ptr<Item>(manastones), Ptr<Item>(tunic));
	executor->advance(1000ms);
	clearSent();
	player().getObserveController()->notifyMoveObservers();
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_OPTION_CANCELED(tunic.getL10n())),
						  animation(831102, MANASTONE_HP_20, 0, 3)}));

	clearSent();
	executor->advance(10000ms);
	player().getObserveController()->notifyMoveObservers();
	EXPECT_TRUE(sent().empty()) << "both tasks cancelled, both observers gone";
	EXPECT_EQ(stones.getItemCount(), 1);
	EXPECT_EQ(manastones.getItemCount(), 1);
	EXPECT_EQ(sword.getEnchantLevel(), 3);
	EXPECT_TRUE(tunic.getItemStones()->isEmpty());
}

TEST_F(EnhanceActionsTest, ATargetThatLeftTheCubeIsNotEnchanted) {
	// EnchantItemAction.java:116-121: the task finds the target neither in the inventory nor equipped -> STR_ENCHANT_ITEM_NO_TARGET_ITEM and
	// the failure animation (end 2); the stone is not used
	Item& stones = inCube(831201, ALPHA_ENCHANTMENT_STONE, 2);
	Item& sword = inCube(831202, TRAINING_SWORD, 1, 1);
	enchantActionOf(ALPHA_ENCHANTMENT_STONE).act(player(), Ptr<Item>(stones), Ptr<Item>(sword));
	executor->advance(1000ms);
	ASSERT_TRUE(player().getInventory().delete_(sword));
	clearSent();
	executor->advance(3000ms);
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_ENCHANT_ITEM_NO_TARGET_ITEM()), animation(831201, ALPHA_ENCHANTMENT_STONE, 0, 2)}));
	EXPECT_EQ(stones.getItemCount(), 2);
	EXPECT_EQ(sword.getEnchantLevel(), 1);
}

TEST_F(EnhanceActionsTest, AnEquippedTargetIsEnchantedAndSocketedByTheTask) {
	// EnchantItemAction.java:114-130: the task gives up only on a target that is neither in the cube nor equipped (:116), so an equipped weapon
	// - the one CM_MANASTONE and CM_USE_ITEM look up in the equipment first - is enchanted after 4 s and socketed after 2 s. The Training Sword
	// in the main hand: an Alpha stone from a seed whose first roll succeeds (Rnd.chance() < 80, EnchantServiceTest), the second roll being the
	// crit of enchantItemAct; the equipped item takes the new level's EnchantEffect (EnchantService.java:262-266). Then Manastone HP +20 with
	// the chance raised to 200: slot 0 and MAXHP 20 at once (:412-416). Both closing animations say 1
	publishSwordEnchantData();
	Item& sword = equippedSword(831501, TRAINING_SWORD);
	Item& stones = inCube(831502, ALPHA_ENCHANTMENT_STONE, 2);
	Item& manastones = inCube(831503, MANASTONE_HP_20, 1);
	uint64_t seed = 1;
	for (;; seed++) {
		Rnd::seedCurrentThreadForTests(seed);
		if (Rnd::chance() < 80.0f)
			break;
	}
	const float crit = Rnd::chance();
	const int32_t expected = crit < 5 ? 3 : crit < 10 ? 2 : 1;

	clearSent();
	Rnd::seedCurrentThreadForTests(seed);
	enchantActionOf(ALPHA_ENCHANTMENT_STONE).act(player(), Ptr<Item>(stones), Ptr<Item>(sword));
	executor->advance(4000ms);
	EXPECT_EQ(sword.getEnchantLevel(), expected);
	EXPECT_EQ(stones.getItemCount(), 1);
	EXPECT_TRUE(sword.getEnchantEffect()) << "the equipped arm of setEnchantLevel";
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_GE(packets.size(), 2u);
	EXPECT_EQ(packets.back(), animation(831502, ALPHA_ENCHANTMENT_STONE, 0, 1));
	EXPECT_EQ(packets[packets.size() - 2], systemMessage(SM_SYSTEM_MESSAGE::STR_MSG_ENCHANT_ITEM_SUCCEED_NEW(sword.getL10n(), expected)));

	ConfigValueScope<std::vector<float>> certain(configs::main::RatesConfig::MANASTONE_CHANCES, std::vector<float>{200.0f, 200.0f});
	const int32_t maxHp = player().getGameStats()->getMaxHp()->getCurrent();
	clearSent();
	enchantActionOf(MANASTONE_HP_20).act(player(), Ptr<Item>(manastones), Ptr<Item>(sword));
	executor->advance(2000ms);
	EXPECT_FALSE(player().getInventory().getItemByObjId(831503)) << "the stone is used up";
	std::vector<Ptr<model::items::ManaStone>> socketed = sword.getItemStones()->snapshot();
	ASSERT_EQ(socketed.size(), 1u);
	EXPECT_EQ(socketed[0]->getSlot(), 0);
	EXPECT_EQ(socketed[0]->getItemId(), MANASTONE_HP_20);
	EXPECT_EQ(player().getGameStats()->getMaxHp()->getCurrent() - maxHp, 20);
	packets = sent();
	ASSERT_FALSE(packets.empty());
	EXPECT_EQ(packets.back(), animation(831503, MANASTONE_HP_20, 0, 1));
}

TEST_F(EnhanceActionsTest, TheSupplementsOfTheActAreUsedUpByItsTask) {
	// EnchantItemAction.java:86-88, 109, 125: the act hands its supplements to enchantItem, which only queues the stone's count (an Alpha stone:
	// 1, Player.subtractSupplements), and the task's enchantItemAct uses them up (Player.updateSupplements, EnchantService.java:190-191). Lesser
	// Supplements (item_templates.xml:838880) are no ENCHANTMENT, so checkSupplementLevel compares the target's level with itself and passes
	Item& stones = inCube(831601, ALPHA_ENCHANTMENT_STONE, 2);
	Item& supplements = inCube(831602, LESSER_SUPPLEMENTS, 3);
	Item& sword = inCube(831603, TRAINING_SWORD, 1);
	enchantActionOf(ALPHA_ENCHANTMENT_STONE).act(player(), stones, sword, Ptr<Item>(supplements), 1);
	executor->advance(3999ms);
	EXPECT_EQ(supplements.getItemCount(), 3) << "only queued";
	executor->advance(1ms);
	EXPECT_EQ(supplements.getItemCount(), 2);
	EXPECT_EQ(stones.getItemCount(), 1);
}

TEST_F(EnhanceActionsTest, AManastoneIsSocketedAfterTwoSeconds) {
	// EnchantItemAction.java:90-91, 127: a manastone (not ENCHANTMENT) takes 2,000 ms and socketManastoneAct; with the chance raised to 200 the
	// roll always succeeds (socketManastone has no cap, EnchantService.java:344-395, m5c-plan.md D6). The stone goes into slot 0 of the tunic
	ConfigValueScope<std::vector<float>> certain(configs::main::RatesConfig::MANASTONE_CHANCES, std::vector<float>{200.0f, 200.0f});
	Item& manastones = inCube(831301, MANASTONE_HP_20, 2);
	Item& tunic = inCube(831302, PLAINSMANS_TUNIC, 1);
	enchantActionOf(MANASTONE_HP_20).act(player(), Ptr<Item>(manastones), Ptr<Item>(tunic));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_ITEM_USAGE_ANIMATION(PLAYER_OBJECT_ID, 831302, 831301, MANASTONE_HP_20, 2000, 0, 0, 1, 0, 0))}));
	clearSent();
	executor->advance(1999ms);
	EXPECT_TRUE(sent().empty());
	executor->advance(1ms);
	EXPECT_EQ(manastones.getItemCount(), 1);
	std::vector<Ptr<model::items::ManaStone>> stones = tunic.getItemStones()->snapshot();
	ASSERT_EQ(stones.size(), 1u);
	EXPECT_EQ(stones[0]->getSlot(), 0);
	EXPECT_EQ(stones[0]->getItemId(), MANASTONE_HP_20);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 4u);
	EXPECT_EQ(packets[1], systemMessage(SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_OPTION_SUCCEED(tunic.getL10n())));
	EXPECT_EQ(packets[2], serialized(SM_INVENTORY_UPDATE_ITEM(player(), tunic, ItemUpdateType::STATS_CHANGE)));
	EXPECT_EQ(packets[3], animation(831301, MANASTONE_HP_20, 0, 1));
}

TEST_F(EnhanceActionsTest, ASupplementOutsideItsLevelRangeStopsTheActBeforeAnything) {
	// EnchantItemAction.java:87-88, 198-220: a manastone supplement for levels 31..50 (item_templates.xml:839031) on the level-1 Training Sword ->
	// STR_ITEM_ENCHANT_ASSISTANT_NO_RIGHT_ITEM and no observer, roll, animation or task. On Tahabata's Sword (level 50) the check passes and the
	// act goes on (the animation of a manastone)
	Item& manastones = inCube(831401, MANASTONE_HP_20, 2);
	Item& supplements = inCube(831402, MANASTONE_SOCKETING_SUPPLEMENTS_31_50, 2);
	Item& sword = inCube(831403, TRAINING_SWORD, 1);
	Item& tahabata = inCube(831404, TAHABATA_SWORD, 1);
	const EnchantItemAction& action = enchantActionOf(MANASTONE_HP_20);
	action.act(player(), manastones, sword, Ptr<Item>(supplements), 1);
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_ITEM_ENCHANT_ASSISTANT_NO_RIGHT_ITEM())}));
	clearSent();
	executor->advance(5000ms);
	player().getObserveController()->notifyMoveObservers();
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(manastones.getItemCount(), 2);

	action.act(player(), manastones, tahabata, Ptr<Item>(supplements), 1);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_ITEM_USAGE_ANIMATION(PLAYER_OBJECT_ID, 831404, 831401, MANASTONE_HP_20, 2000, 0, 0, 1, 0, 0))}));
	executor->advance(2000ms);
	EXPECT_EQ(manastones.getItemCount(), 1);
	EXPECT_EQ(supplements.getItemCount(), 1) << "the manastone-only supplement: 1 used by updateSupplements";
}

// ------------------------------------------------------------------------------------------------------------------------- ExtractAction

TEST_F(EnhanceActionsTest, ExtractionNeedsAnUnequippedWeaponOrArmour) {
	// ExtractAction.java:25-40: no target -> STR_DECOMPOSE_ITEM_NO_TARGET_ITEM; neither armour nor weapon -> IT_CAN_NOT_BE_DECOMPOSED(item);
	// equipped -> STR_DECOMPOSE_EQUIP_ITEM_CAN_NOT_BE_DECOMPOSED; a sword or a tunic of the cube -> true
	const ExtractAction& extract = onlyActionOf<ExtractAction>(EXTRACTION_TOOLS);
	Item& tools = inCube(832001, EXTRACTION_TOOLS, 1);
	Item& potion = inCube(832002, MINOR_LIFE_POTION, 1);
	Item& sword = inCube(832003, TRAINING_SWORD, 1);
	Item& tunic = inCube(832004, PLAINSMANS_TUNIC, 1);
	Ref<Item> equipped = loadedItem(832005, TRAINING_SWORD, 1, StorageType::CUBE, 1, true);
	items.push_back(equipped);

	EXPECT_FALSE(extract.canAct(player(), Ptr<Item>(tools), nullptr));
	EXPECT_FALSE(extract.canAct(player(), Ptr<Item>(tools), Ptr<Item>(potion)));
	EXPECT_FALSE(extract.canAct(player(), Ptr<Item>(tools), Ptr<Item>(equipped)));
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_DECOMPOSE_ITEM_NO_TARGET_ITEM()),
						  systemMessage(SM_SYSTEM_MESSAGE::STR_DECOMPOSE_ITEM_IT_CAN_NOT_BE_DECOMPOSED(potion.getL10n())),
						  systemMessage(SM_SYSTEM_MESSAGE::STR_DECOMPOSE_EQUIP_ITEM_CAN_NOT_BE_DECOMPOSED())}));
	clearSent();
	EXPECT_TRUE(extract.canAct(player(), Ptr<Item>(tools), Ptr<Item>(sword)));
	EXPECT_TRUE(extract.canAct(player(), Ptr<Item>(tools), Ptr<Item>(tunic)));
	EXPECT_TRUE(sent().empty());
}

TEST_F(EnhanceActionsTest, ExtractionBreaksTheItemAfterFiveSeconds) {
	// ExtractAction.java:42-67: the 5,000 ms animation (sent to the player only), the observer, then the ITEM_USE task: canAct again and
	// EnchantService.breakItem - Plainsman's Sword (COMMON level 2 weapon, item_templates.xml:478): 7 + roll + 5 < 50 -> Alpha, Rnd.get(2, 5)
	// stones -, the closing animation (end 1)
	Item& tools = inCube(832101, EXTRACTION_TOOLS, 2);
	Item& sword = inCube(832102, PLAINSMANS_SWORD, 1);
	Rnd::seedCurrentThreadForTests(31);
	static_cast<void>(Rnd::get(0, 10));
	const int32_t count = Rnd::get(2, 5);

	onlyActionOf<ExtractAction>(EXTRACTION_TOOLS).act(player(), Ptr<Item>(tools), Ptr<Item>(sword));
	EXPECT_EQ(sent(), cp::exactly({animation(832101, EXTRACTION_TOOLS, 5000, 0)}));
	clearSent();
	executor->advance(4999ms);
	EXPECT_TRUE(sent().empty());
	Rnd::seedCurrentThreadForTests(31);
	executor->advance(1ms);
	EXPECT_FALSE(player().getInventory().getItemByObjId(832102));
	EXPECT_EQ(tools.getItemCount(), 1);
	EXPECT_EQ(cubeCount(ALPHA_ENCHANTMENT_STONE), count);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_FALSE(packets.empty());
	EXPECT_EQ(packets.back(), animation(832101, EXTRACTION_TOOLS, 0, 1));
	clearSent();
	player().getObserveController()->notifyMoveObservers();
	EXPECT_TRUE(sent().empty()) << "the task removed its observer";
}

TEST_F(EnhanceActionsTest, AMoveDuringTheExtractionAbortsIt) {
	// ExtractAction.java:46-56: STR_DECOMPOSE_ITEM_CANCELED(target), the failure animation (end 2), the task cancelled; nothing is broken
	Item& tools = inCube(832201, EXTRACTION_TOOLS, 1);
	Item& sword = inCube(832202, PLAINSMANS_SWORD, 1);
	onlyActionOf<ExtractAction>(EXTRACTION_TOOLS).act(player(), Ptr<Item>(tools), Ptr<Item>(sword));
	executor->advance(3000ms);
	clearSent();
	player().getObserveController()->notifyMoveObservers();
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_DECOMPOSE_ITEM_CANCELED(sword.getL10n())), animation(832201, EXTRACTION_TOOLS, 0, 2)}));
	clearSent();
	executor->advance(5000ms);
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(tools.getItemCount(), 1);
	EXPECT_TRUE(player().getInventory().getItemByObjId(832202));
}

TEST_F(EnhanceActionsTest, AnExtractionWhoseItemLeftTheCubeFailsAtTheEnd) {
	// ExtractAction.java:60-63: `canAct(...) && breakItem(...)` - the sword was deleted during the 5 s: canAct still holds (a weapon, not
	// equipped), breakItem finds it missing and answers false, so no cooldown and the closing animation says 2; the tools stay
	Item& tools = inCube(832301, EXTRACTION_TOOLS, 1);
	Item& sword = inCube(832302, PLAINSMANS_SWORD, 1);
	onlyActionOf<ExtractAction>(EXTRACTION_TOOLS).act(player(), Ptr<Item>(tools), Ptr<Item>(sword));
	executor->advance(1000ms);
	ASSERT_TRUE(player().getInventory().delete_(sword));
	clearSent();
	executor->advance(4000ms);
	EXPECT_EQ(sent(), cp::exactly({animation(832301, EXTRACTION_TOOLS, 0, 2)}));
	EXPECT_EQ(tools.getItemCount(), 1);
	EXPECT_EQ(cubeCount(ALPHA_ENCHANTMENT_STONE), 0);
}

TEST_F(EnhanceActionsTest, TheExtractionTaskAsksCanActAgain) {
	// ExtractAction.java:60: the task repeats canAct before breakItem. A sword that became equipped during the 5 s (the flag set directly: the
	// check reads only isEquipped, ExtractAction.java:34) -> STR_DECOMPOSE_EQUIP_ITEM_CAN_NOT_BE_DECOMPOSED, nothing broken, the closing
	// animation says 2
	Item& tools = inCube(832401, EXTRACTION_TOOLS, 1);
	Item& sword = inCube(832402, PLAINSMANS_SWORD, 1);
	onlyActionOf<ExtractAction>(EXTRACTION_TOOLS).act(player(), Ptr<Item>(tools), Ptr<Item>(sword));
	executor->advance(1000ms);
	sword.setEquipped(true);
	clearSent();
	executor->advance(4000ms);
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_DECOMPOSE_EQUIP_ITEM_CAN_NOT_BE_DECOMPOSED()),
						  animation(832401, EXTRACTION_TOOLS, 0, 2)}));
	EXPECT_TRUE(player().getInventory().getItemByObjId(832402));
	EXPECT_EQ(tools.getItemCount(), 1);
	EXPECT_EQ(cubeCount(ALPHA_ENCHANTMENT_STONE), 0);
}

// ------------------------------------------------------------------------------------------------------------------------- DecomposeAction

class DecomposeActionTest : public EnhanceActionsTest {
protected:
	void SetUp() override {
		EnhanceActionsTest::SetUp();
		player().getPosition()->setIsSpawned(true); // DecomposeAction.java:101: a dead or unspawned player cannot decompose
	}
};

TEST_F(DecomposeActionTest, OnlyASpawnedLivingPlayerDecomposesADecomposableItemWithRoomInTheCube) {
	// DecomposeAction.java:99-115: unspawned -> false; an item without a decomposable row -> IT_CAN_NOT_BE_DECOMPOSED(item); a selectable row
	// (the Cold Box, decomposable_items.xml:7309) -> true at once; a full cube -> STR_DECOMPOSE_ITEM_INVENTORY_IS_FULL
	Item& pepento = inCube(833001, JUICY_PEPENTO, 2);
	Item& sword = inCube(833002, TRAINING_SWORD, 1);
	Item& box = inCube(833003, EVENT_COLD_BOX, 1);
	const DecomposeAction& decompose = onlyActionOf<DecomposeAction>(JUICY_PEPENTO);

	player().getPosition()->setIsSpawned(false);
	EXPECT_FALSE(decompose.canAct(player(), Ptr<Item>(pepento), nullptr));
	EXPECT_TRUE(sent().empty());
	player().getPosition()->setIsSpawned(true);

	EXPECT_TRUE(decompose.canAct(player(), Ptr<Item>(pepento), nullptr));
	EXPECT_TRUE(decompose.canAct(player(), Ptr<Item>(box), nullptr));
	EXPECT_TRUE(sent().empty());
	EXPECT_FALSE(decompose.canAct(player(), Ptr<Item>(sword), nullptr));
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_DECOMPOSE_ITEM_IT_CAN_NOT_BE_DECOMPOSED(sword.getL10n()))}));

	for (int32_t objId = 833100; !player().getInventory().isFull(); objId++)
		inCube(objId, TRAINING_SWORD, 1);
	clearSent();
	EXPECT_FALSE(decompose.canAct(player(), Ptr<Item>(pepento), nullptr));
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_DECOMPOSE_ITEM_INVENTORY_IS_FULL())}));
}

TEST_F(DecomposeActionTest, AFixedRewardArrivesAfterTheCastingDelay) {
	// DecomposeAction.java:117-184: Juicy Pepento (casting_delay 3000, item_templates.xml:743603) -> the broadcast animation (3000), the
	// observer and the ITEM_USE task; then postValidate uses one Juicy Pepento, STR_UNCOMPRESS_COMPRESSED_ITEM_SUCCEEDED, STR_USE_ITEM, two
	// Pepento (decomposable_items.xml:3: min_count 2, max_count unset = 2) and the closing animation (end 1)
	Item& juicy = inCube(833201, JUICY_PEPENTO, 2);
	onlyActionOf<DecomposeAction>(JUICY_PEPENTO).act(player(), Ptr<Item>(juicy), nullptr);
	EXPECT_EQ(sent(), cp::exactly({animation(833201, JUICY_PEPENTO, 3000, 0)}));
	clearSent();
	executor->advance(2999ms);
	EXPECT_TRUE(sent().empty());
	executor->advance(1ms);
	EXPECT_EQ(juicy.getItemCount(), 1);
	EXPECT_EQ(cubeCount(PEPENTO), 2);
	std::vector<std::vector<uint8_t>> messages = sentWithOpcode(SM_SYSTEM_MESSAGE_OPCODE);
	ASSERT_EQ(messages.size(), 2u);
	EXPECT_EQ(messages[0], systemMessage(SM_SYSTEM_MESSAGE::STR_UNCOMPRESS_COMPRESSED_ITEM_SUCCEEDED(juicy.getL10n())));
	EXPECT_EQ(messages[1], systemMessage(SM_SYSTEM_MESSAGE::STR_USE_ITEM(juicy.getL10n())));
	EXPECT_EQ(sent().back(), animation(833201, JUICY_PEPENTO, 0, 1));
	clearSent();
	player().getObserveController()->notifyMoveObservers();
	EXPECT_TRUE(sent().empty()) << "the task removed its observer";
}

TEST_F(DecomposeActionTest, ARandomEnchantmentRewardIsOneOfTheFourStonesOfTheLevel) {
	// DecomposeAction.java:190-201, 358-361: the Suspicious Old Sack (level 30, item_templates.xml:904910; decomposable_items.xml:4014: one
	// random ENCHANTMENT, max_count 3) gives 166000191 + Math.round(30 / 100f) (0) + Rnd.nextInt(4), retried while the id is no item, and
	// Rnd.get(1, 3) of it. The draws: Chance.selectElement's nextFloat(100), then nextInt(4), then the count
	std::set<int32_t> stoneIds;
	for (uint64_t seed = 1; seed <= 12; seed++) {
		SCOPED_TRACE(seed);
		Rnd::seedCurrentThreadForTests(seed);
		static_cast<void>(Rnd::nextFloat(100.0f));
		const int32_t stoneId = ALPHA_ENCHANTMENT_STONE + Rnd::nextInt(4);
		const int32_t count = Rnd::get(1, 3);
		stoneIds.insert(stoneId);
		const int64_t before = cubeCount(stoneId);

		const int32_t objId = 833300 + static_cast<int32_t>(seed);
		Item& sack = inCube(objId, SUSPICIOUS_OLD_SACK, 1);
		Rnd::seedCurrentThreadForTests(seed);
		onlyActionOf<DecomposeAction>(SUSPICIOUS_OLD_SACK).act(player(), Ptr<Item>(sack), nullptr);
		executor->advance(1500ms);
		EXPECT_FALSE(player().getInventory().getItemByObjId(objId)) << "the sack is used up";
		EXPECT_EQ(cubeCount(stoneId), before + count);
	}
	EXPECT_EQ(stoneIds, (std::set<int32_t>{ALPHA_ENCHANTMENT_STONE, BETA_ENCHANTMENT_STONE, GAMMA_ENCHANTMENT_STONE, DELTA_ENCHANTMENT_STONE}));
	EXPECT_EQ(cubeCount(EPSILON_ENCHANTMENT_STONE), 0);
}

TEST_F(DecomposeActionTest, ALevelFiftySackGivesTheStonesFromBetaOnAndStartsItsCooldown) {
	// DecomposeAction.java:175, 192-200: the Suspicious Red Sack is level 50 (item_templates.xml:904922; decomposable_items.xml:4024: one random
	// ENCHANTMENT, max_count 5), so its stone is 166000191 + Math.round(50 / 100f) (1) + Rnd.nextInt(4): Beta to Epsilon, never Alpha, and
	// Rnd.get(1, 5) of it. finishUse starts the sack's cooldown (usedelay 5000, usedelayid 85; Player.startCooldown)
	std::set<int32_t> stoneIds;
	for (uint64_t seed = 1; seed <= 12; seed++) {
		SCOPED_TRACE(seed);
		Rnd::seedCurrentThreadForTests(seed);
		static_cast<void>(Rnd::nextFloat(100.0f));
		const int32_t stoneId = BETA_ENCHANTMENT_STONE + Rnd::nextInt(4);
		const int32_t count = Rnd::get(1, 5);
		stoneIds.insert(stoneId);
		const int64_t before = cubeCount(stoneId);

		const int32_t objId = 833700 + static_cast<int32_t>(seed);
		Item& sack = inCube(objId, SUSPICIOUS_RED_SACK, 1);
		if (seed == 1)
			EXPECT_FALSE(player().hasCooldown(sack));
		Rnd::seedCurrentThreadForTests(seed);
		onlyActionOf<DecomposeAction>(SUSPICIOUS_RED_SACK).act(player(), Ptr<Item>(sack), nullptr);
		executor->advance(1500ms);
		EXPECT_FALSE(player().getInventory().getItemByObjId(objId)) << "the sack is used up";
		EXPECT_EQ(cubeCount(stoneId), before + count);
		EXPECT_TRUE(player().hasCooldown(sack)) << "usedelay 5000";
	}
	EXPECT_EQ(stoneIds, (std::set<int32_t>{BETA_ENCHANTMENT_STONE, GAMMA_ENCHANTMENT_STONE, DELTA_ENCHANTMENT_STONE, EPSILON_ENCHANTMENT_STONE}));
	EXPECT_EQ(cubeCount(ALPHA_ENCHANTMENT_STONE), 0);
}

TEST_F(DecomposeActionTest, WithoutACastingDelayTheRewardsOfThePlayersRaceArriveAtOnce) {
	// DecomposeAction.java:133-136, 172-184: the Chocolate Candy Box has no casting_delay (item_templates.xml:905076), so act finishes the use at
	// once - no animation before it, no task. Of its two rows (decomposable_items.xml:4213) only the ELYOS candy is obtainable for the ELYOS
	// fixture player (ResultedItem.isObtainableFor): 3 of it (min_count 3), added as ItemAddType.DECOMPOSABLE (the SM_INVENTORY_ADD_ITEM of the
	// new stack, ItemService's predicate); then the two messages and the closing animation (end 1)
	Item& box = inCube(833801, CHOCOLATE_CANDY_BOX, 1);
	onlyActionOf<DecomposeAction>(CHOCOLATE_CANDY_BOX).act(player(), Ptr<Item>(box), nullptr);
	EXPECT_FALSE(player().getInventory().getItemByObjId(833801)) << "the box is used up";
	EXPECT_EQ(cubeCount(ENAMORED_TIGER_FORM_CANDY_ELYOS), 3);
	EXPECT_EQ(cubeCount(ENAMORED_TIGER_FORM_CANDY_ASMODIANS), 0);
	std::vector<Ptr<Item>> candies = player().getInventory().getItemsByItemId(ENAMORED_TIGER_FORM_CANDY_ELYOS);
	ASSERT_EQ(candies.size(), 1u);
	std::vector<std::vector<uint8_t>> added = sentWithOpcode(SM_INVENTORY_ADD_ITEM_OPCODE);
	ASSERT_EQ(added.size(), 1u);
	EXPECT_EQ(added[0], serialized(SM_INVENTORY_ADD_ITEM({candies[0]}, player(), ItemPacketService::ItemAddType::DECOMPOSABLE)));
	std::vector<std::vector<uint8_t>> messages = sentWithOpcode(SM_SYSTEM_MESSAGE_OPCODE);
	ASSERT_EQ(messages.size(), 2u);
	EXPECT_EQ(messages[0], systemMessage(SM_SYSTEM_MESSAGE::STR_UNCOMPRESS_COMPRESSED_ITEM_SUCCEEDED(box.getL10n())));
	EXPECT_EQ(messages[1], systemMessage(SM_SYSTEM_MESSAGE::STR_USE_ITEM(box.getL10n())));
	EXPECT_EQ(sentWithOpcode(SM_ITEM_USAGE_ANIMATION_OPCODE), (std::vector<std::vector<uint8_t>>{animation(833801, CHOCOLATE_CANDY_BOX, 0, 1)}));
	EXPECT_EQ(sent().back(), animation(833801, CHOCOLATE_CANDY_BOX, 0, 1));
	clearSent();
	executor->advance(5000ms);
	player().getObserveController()->notifyMoveObservers();
	EXPECT_TRUE(sent().empty()) << "no task and no observer";
}

TEST_F(DecomposeActionTest, ASelectableItemOnlyShowsItsChoice) {
	// DecomposeAction.java:119-124: a selectable decomposable sends SM_FIRST_SHOW_DECOMPOSABLE with the items the player may obtain (all six of
	// the Cold Box, which name no race or class) and returns: no animation, no task, nothing used up
	Item& box = inCube(833401, EVENT_COLD_BOX, 1);
	std::optional<std::vector<const model::templates::item::ResultedItem*>> choice =
		dataholders::DataManager::DECOMPOSABLE_ITEMS_DATA->getSelectableItems(EVENT_COLD_BOX);
	ASSERT_TRUE(choice);
	ASSERT_EQ(choice->size(), 6u);
	onlyActionOf<DecomposeAction>(EVENT_COLD_BOX).act(player(), Ptr<Item>(box), nullptr);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_FIRST_SHOW_DECOMPOSABLE(833401, *choice))}));
	clearSent();
	executor->advance(5000ms);
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(box.getItemCount(), 1);
}

TEST_F(DecomposeActionTest, AMoveDuringTheCastingDelayAbortsTheDecomposition) {
	// DecomposeAction.java:141-152: STR_UNCOMPRESS_COMPRESSED_ITEM_CANCELED(parent) and the broadcast failure animation (end 2); the task is
	// cancelled, nothing is used up
	Item& juicy = inCube(833501, JUICY_PEPENTO, 1);
	onlyActionOf<DecomposeAction>(JUICY_PEPENTO).act(player(), Ptr<Item>(juicy), nullptr);
	executor->advance(1000ms);
	clearSent();
	player().getObserveController()->notifyMoveObservers();
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_UNCOMPRESS_COMPRESSED_ITEM_CANCELED(juicy.getL10n())),
						  animation(833501, JUICY_PEPENTO, 0, 2)}));
	clearSent();
	executor->advance(5000ms);
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(juicy.getItemCount(), 1);
	EXPECT_EQ(cubeCount(PEPENTO), 0);
}

TEST_F(DecomposeActionTest, AParentThatLeftTheCubeDuringTheCastingDelayGivesNothing) {
	// DecomposeAction.java:161-170, 173, 365-366: postValidate's canAct holds (the row exists, the cube has room), but decreaseByObjectId finds
	// no item -> STR_DECOMPOSE_ITEM_NO_TARGET_ITEM, no reward, the closing animation says 2
	Item& juicy = inCube(833601, JUICY_PEPENTO, 1);
	onlyActionOf<DecomposeAction>(JUICY_PEPENTO).act(player(), Ptr<Item>(juicy), nullptr);
	executor->advance(1000ms);
	ASSERT_TRUE(player().getInventory().delete_(juicy));
	clearSent();
	executor->advance(2000ms);
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_DECOMPOSE_ITEM_NO_TARGET_ITEM()), animation(833601, JUICY_PEPENTO, 0, 2)}));
	EXPECT_EQ(cubeCount(PEPENTO), 0);
}

// ------------------------------------------------------------------------------------------------------------------------- RemodelAction

TEST_F(EnhanceActionsTest, TheRemodelActionNeverActs) {
	// RemodelAction.java:19-26: canAct is false, act does nothing (Tahabata's Sword's <remodel type="2"/>, item_templates.xml:4807)
	const RemodelAction& remodel = onlyActionOf<RemodelAction>(TAHABATA_SWORD);
	Item& sword = inCube(834001, TAHABATA_SWORD, 1);
	EXPECT_FALSE(remodel.canAct(player(), Ptr<Item>(sword), Ptr<Item>(sword)));
	EXPECT_FALSE(remodel.canAct(player(), nullptr, nullptr));
	remodel.act(player(), Ptr<Item>(sword), nullptr);
	executor->advance(5000ms);
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(sword.getItemCount(), 1);
	EXPECT_EQ(remodel.getExtractType(), 2);
}

} // namespace
} // namespace aion::gameserver::services::item::test
