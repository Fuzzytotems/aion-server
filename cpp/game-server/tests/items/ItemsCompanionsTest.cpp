// P4-13 bodies without a Player: the enum companions of the item model (ItemSlot, EnchantmentStone, BrokerMessages, BrokerItemMask with its
// filters), ItemId/ItemMask, NpcEquippedGear, the drop model (Drop, DropModifiers, DropItem, DropGroup, NpcDrop) and the trade values.
// Expectations are derived by hand from the Java sources named in each test; static data is bound from XML text.

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "ItemsTestSupport.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataException.h"
#include "aion/gameserver/dataholders/loadingutils/adapters/NpcEquipmentList.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/broker/BrokerItemMask.h"
#include "aion/gameserver/model/broker/BrokerItemMaskInfo.h"
#include "aion/gameserver/model/broker/BrokerMessagesInfo.h"
#include "aion/gameserver/model/drop/Drop.bind.h"
#include "aion/gameserver/model/drop/Drop.h"
#include "aion/gameserver/model/drop/DropGroup.bind.h"
#include "aion/gameserver/model/drop/DropGroup.h"
#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/drop/DropModifiers.h"
#include "aion/gameserver/model/drop/NpcDrop.bind.h"
#include "aion/gameserver/model/drop/NpcDrop.h"
#include "aion/gameserver/model/enchants/EnchantmentStoneInfo.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/items/ItemId.h"
#include "aion/gameserver/model/items/ItemMask.h"
#include "aion/gameserver/model/items/ItemSlotInfo.h"
#include "aion/gameserver/model/items/NpcEquippedGear.h"
#include "aion/gameserver/model/templates/item/actions/CraftLearnAction.bind.h"
#include "aion/gameserver/model/templates/item/actions/CraftLearnAction.h"
#include "aion/gameserver/model/templates/recipe/RecipeTemplate.bind.h"
#include "aion/gameserver/model/trade/ExchangeItem.h"
#include "aion/gameserver/model/trade/TradeItem.h"
#include "aion/gameserver/model/trade/TradeList.h"
#include "aion/gameserver/model/trade/TradePSItem.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

namespace aion::gameserver::model::items {
namespace {

using runtime::Ptr;
using runtime::Ref;
using test::registerItem;

// ---- ItemId, ItemMask, ItemSlot (ItemSlot.java) -----------------------------------------------------------------------------------------------

static_assert(ItemId::KINAH == 182400001);
static_assert(ItemMask::LIMIT_ONE == 1 && ItemMask::STORABLE_IN_LWH == 32 && ItemMask::CAN_SPLIT == 8192 && ItemMask::LEGION_TRADEABLE == 262144);

static_assert(getSlotIdMask(ItemSlot::MAIN_OR_SUB) == 3 && getSlotIdMask(ItemSlot::EARRING_RIGHT_OR_LEFT) == 192);
static_assert(getSlotIdMask(ItemSlot::RING_RIGHT_OR_LEFT) == 768 && getSlotIdMask(ItemSlot::SHARD_RIGHT_OR_LEFT) == 24576);
static_assert(getSlotIdMask(ItemSlot::STIGMA3) == 4294967296LL && getSlotIdMask(ItemSlot::ALL_STIGMA) == 0xFC0000000LL);
static_assert(getSlotIdMask(ItemSlot::VISIBLE) == (1 | 2 | 4 | 8 | 16 | 32 | 64 | 128 | 1024 | 2048 | 4096 | 8192 | 16384 | 32768 | 524288));
static_assert(isCombo(ItemSlot::VISIBLE) && !isCombo(ItemSlot::PLUME) && isCombo(ItemSlot::LEFT_HAND));
static_assert(isStigma(getSlotIdMask(ItemSlot::STIGMA1) | getSlotIdMask(ItemSlot::ADV_STIGMA2)));
static_assert(!isRegularStigma(getSlotIdMask(ItemSlot::ADV_STIGMA1)) && isAdvancedStigma(getSlotIdMask(ItemSlot::ADV_STIGMA3)));
static_assert(isAdvancedStigma(0) && !isVisible(getSlotIdMask(ItemSlot::RING_LEFT)) && isVisible(getSlotIdMask(ItemSlot::PLUME)));
static_assert(isTwoHandedWeapon(3) && isTwoHandedWeapon(getSlotIdMask(ItemSlot::MAIN_OFF_OR_SUB_OFF)) && !isTwoHandedWeapon(1));
// getEquipmentSlotType: not visible 0, right-hand 1, left-hand 2 (a two-handed weapon is right-hand)
static_assert(getEquipmentSlotType(getSlotIdMask(ItemSlot::RING_LEFT)) == 0 && getEquipmentSlotType(getSlotIdMask(ItemSlot::TORSO)) == 1);
static_assert(getEquipmentSlotType(getSlotIdMask(ItemSlot::SUB_HAND)) == 2 && getEquipmentSlotType(3) == 1);
static_assert(getEquipmentSlotType(getSlotIdMask(ItemSlot::EARRINGS_LEFT)) == 2 && getEquipmentSlotType(getSlotIdMask(ItemSlot::SUB_OFF_HAND)) == 0);

TEST(ItemSlotCompanionTest, SlotsForAMaskAreTheNonComboSlotsInOrdinalOrder) {
	EXPECT_EQ(getSlotsFor(3), (std::vector<ItemSlot>{ItemSlot::MAIN_HAND, ItemSlot::SUB_HAND}));
	EXPECT_EQ(getSlotsFor(getSlotIdMask(ItemSlot::RIGHT_HAND)), (std::vector<ItemSlot>{ItemSlot::MAIN_HAND, ItemSlot::MAIN_OFF_HAND}));
	EXPECT_EQ(getSlotsFor(getSlotIdMask(ItemSlot::ALL_STIGMA)).size(), 6u);
	EXPECT_EQ(getSlotFor(768), ItemSlot::RING_LEFT);
	try {
		static_cast<void>(getSlotsFor(0));
		ADD_FAILURE() << "Java throws for mask 0";
	} catch (const runtime::IllegalArgumentException& e) {
		EXPECT_STREQ(e.what(), "slotIdMask cannot be 0");
	}
	EXPECT_TRUE(getSlotsFor(1LL << 20).empty()) << "an unused bit matches no slot";
	EXPECT_THROW(static_cast<void>(getSlotFor(1LL << 20)), runtime::ArrayIndexOutOfBoundsException) << "Java indexes the empty array";
}

// ---- EnchantmentStone (EnchantmentStone.java) -----------------------------------------------------------------------------------------------

TEST(EnchantmentStoneCompanionTest, StoneOfAnItemIdFollowsJavasRanges) {
	using enchants::EnchantmentStone;
	EXPECT_EQ(enchants::getByItemId(166000191), EnchantmentStone::ALPHA);
	EXPECT_EQ(enchants::getByItemId(166000195), EnchantmentStone::EPSILON);
	EXPECT_EQ(enchants::getByItemId(166020003), EnchantmentStone::OMEGA);
	// old stones L1-L190
	EXPECT_EQ(enchants::getByItemId(166000001), EnchantmentStone::ALPHA);
	EXPECT_EQ(enchants::getByItemId(166000030), EnchantmentStone::ALPHA);
	EXPECT_EQ(enchants::getByItemId(166000031), EnchantmentStone::BETA);
	EXPECT_EQ(enchants::getByItemId(166000050), EnchantmentStone::BETA);
	EXPECT_EQ(enchants::getByItemId(166000051), EnchantmentStone::GAMMA);
	EXPECT_EQ(enchants::getByItemId(166000060), EnchantmentStone::GAMMA);
	EXPECT_EQ(enchants::getByItemId(166000061), EnchantmentStone::DELTA);
	EXPECT_EQ(enchants::getByItemId(166000100), EnchantmentStone::DELTA);
	EXPECT_EQ(enchants::getByItemId(166000101), EnchantmentStone::EPSILON);
	EXPECT_EQ(enchants::getByItemId(166000190), EnchantmentStone::EPSILON);
	for (int32_t itemId : {166000000, 166000196, 166020004}) {
		try {
			static_cast<void>(enchants::getByItemId(itemId));
			ADD_FAILURE() << itemId << " is no enchantment stone";
		} catch (const runtime::IllegalArgumentException& e) {
			EXPECT_EQ(std::string(e.what()), "No matching enchantment stone found for item ID " + std::to_string(itemId));
		}
	}
	EXPECT_EQ(enchants::getBaseLevel(EnchantmentStone::GAMMA), 55);
	EXPECT_EQ(enchants::getBaseLevel(EnchantmentStone::OMEGA), 65);
	EXPECT_EQ(enchants::getBaseQuality(EnchantmentStone::DELTA), templates::item::ItemQuality::EPIC);
	EXPECT_EQ(enchants::getBaseQuality(EnchantmentStone::ALPHA), templates::item::ItemQuality::RARE);
}

// ---- broker (BrokerMessages.java, BrokerItemMask.java, filter/*.java) -----------------------------------------------------------------------

class BrokerItemMaskTest : public testing::Test {
protected:
	void TearDown() override { runtime::Reclaimer::getInstance().drain(); }

	test::StaticDataScope staticData;
};

TEST_F(BrokerItemMaskTest, ConstructorDataAndParentChain) {
	using broker::BrokerItemMask;
	EXPECT_EQ(broker::getId(broker::BrokerMessages::CANT_REGISTER_ITEM), 2);
	EXPECT_EQ(broker::getId(broker::BrokerMessages::NO_ENOUGHT_KINAH), 5);

	EXPECT_EQ(broker::getId(BrokerItemMask::WEAPON), 9010);
	EXPECT_EQ(broker::getId(BrokerItemMask::UNKNOWN), 1);
	EXPECT_EQ(broker::getBrokerMaskById(7030), BrokerItemMask::ACCESSORY_HEADGEAR);
	EXPECT_EQ(broker::getBrokerMaskById(1400), BrokerItemMask::SKILL_RELATED_STIGMA);
	EXPECT_EQ(broker::getBrokerMaskById(12345), BrokerItemMask::UNKNOWN);
	EXPECT_TRUE(broker::isChildrenMask(BrokerItemMask::ARMOR_CLOTHING_JACKET, 8010)) << "parent ARMOR_CLOTHING";
	EXPECT_TRUE(broker::isChildrenMask(BrokerItemMask::ARMOR_CLOTHING_JACKET, 9020)) << "grandparent ARMOR";
	EXPECT_FALSE(broker::isChildrenMask(BrokerItemMask::ARMOR_CLOTHING_JACKET, 1100)) << "its own id is no ancestor";
	EXPECT_FALSE(broker::isChildrenMask(BrokerItemMask::WEAPON, 9010));
	EXPECT_TRUE(broker::isChildrenMask(BrokerItemMask::CRAFT_DESIGN_COOKING, 9050));
	EXPECT_TRUE(broker::hasChildren(BrokerItemMask::CRAFT_MATERIALS));
	EXPECT_FALSE(broker::hasChildren(BrokerItemMask::OTHER));
}

TEST_F(BrokerItemMaskTest, FiltersMatchTheItemTemplateIds) {
	using broker::BrokerItemMask;
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<gameobjects::Item> sword = gameobjects::Item::create(1, registerItem(R"(id="100000001")"));
	EXPECT_TRUE(broker::isMatches(BrokerItemMask::WEAPON, *sword)) << "BrokerMinMaxFilter(1000, 1021): 100000001 / 100000 = 1000";
	EXPECT_TRUE(broker::isMatches(BrokerItemMask::WEAPON_SWORD, *sword));
	EXPECT_FALSE(broker::isMatches(BrokerItemMask::WEAPON_MACE, *sword));
	EXPECT_FALSE(broker::isMatches(BrokerItemMask::ARMOR, *sword));
	Ref<gameobjects::Item> shield = gameobjects::Item::create(2, registerItem(R"(id="115000001")"));
	EXPECT_TRUE(broker::isMatches(BrokerItemMask::ARMOR, *shield)) << "1150 is the upper bound of BrokerMinMaxFilter(1100, 1150)";
	EXPECT_TRUE(broker::isMatches(BrokerItemMask::ARMOR_SHIELD, *shield));

	// BrokerContainsExtraFilter divides by 10000
	Ref<gameobjects::Item> material = gameobjects::Item::create(3, registerItem(R"(id="152000123")"));
	EXPECT_TRUE(broker::isMatches(BrokerItemMask::CRAFT_MATERIALS, *material));
	EXPECT_TRUE(broker::isMatches(BrokerItemMask::CRAFT_MATERIALS_GATHERED, *material)) << "15200";
	EXPECT_FALSE(broker::isMatches(BrokerItemMask::CRAFT_MATERIALS_LOOTED, *material));
	Ref<gameobjects::Item> other = gameobjects::Item::create(4, registerItem(R"(id="99999")"));
	EXPECT_TRUE(broker::isMatches(BrokerItemMask::UNKNOWN, *other)) << "BrokerContainsFilter(0)";

	// BrokerPlayerClassExtraFilter: mask and ItemTemplate.isClassSpecific (restrictions in PlayerClass ordinal order, then the starting class)
	Ref<gameobjects::Item> gladiatorStigma =
		gameobjects::Item::create(5, registerItem(R"(id="140000001" restrict="0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0")"));
	EXPECT_TRUE(broker::isMatches(BrokerItemMask::SKILL_RELATED_STIGMA_GLADIATOR, *gladiatorStigma));
	EXPECT_FALSE(broker::isMatches(BrokerItemMask::SKILL_RELATED_STIGMA_TEMPLAR, *gladiatorStigma)) << "neither TEMPLAR nor WARRIOR restricted";
	EXPECT_FALSE(broker::isMatches(BrokerItemMask::SKILL_RELATED_SKILL_MANUAL_GLADIATOR, *gladiatorStigma)) << "mask 1695";
	Ref<gameobjects::Item> warriorStigma =
		gameobjects::Item::create(6, registerItem(R"(id="140000002" restrict="1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0")"));
	EXPECT_TRUE(broker::isMatches(BrokerItemMask::SKILL_RELATED_STIGMA_TEMPLAR, *warriorStigma)) << "the starting class WARRIOR is restricted";
	EXPECT_FALSE(broker::isMatches(BrokerItemMask::SKILL_RELATED_STIGMA_RANGER, *warriorStigma));
}

TEST_F(BrokerItemMaskTest, RecipeFilterNeedsACraftLearnActionOfTheCraftSkill) {
	using broker::BrokerItemMask;
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	test::StaticDataRegistry& registry = test::StaticDataRegistry::get();
	const templates::item::ItemTemplate* cookingDesign = registerItem(R"(id="152200001")", "<actions/>");
	const auto* craftLearn = test::bindStatic<templates::item::actions::CraftLearnAction>(R"(<craftlearn recipeid="155000001"/>)");
	const auto* recipe = test::bindStatic<templates::recipe::RecipeTemplate>(R"(<recipe_template id="155000001" skillid="40001"/>)");
	registry.craftLearnActions[cookingDesign->getActions()] = craftLearn;
	registry.recipes[155000001] = recipe;
	registry.recipeSkillIds[recipe] = 40001;
	Ref<gameobjects::Item> item = gameobjects::Item::create(7, cookingDesign);
	EXPECT_TRUE(broker::isMatches(BrokerItemMask::CRAFT_DESIGN_COOKING, *item));
	EXPECT_FALSE(broker::isMatches(BrokerItemMask::CRAFT_DESIGN_ALCHEMY, *item)) << "recipe of another craft skill";
	EXPECT_TRUE(broker::isMatches(BrokerItemMask::CRAFT_DESIGN, *item)) << "the parent is a plain BrokerContainsFilter(1522)";

	Ref<gameobjects::Item> withoutActions = gameobjects::Item::create(8, registerItem(R"(id="152200002")"));
	EXPECT_FALSE(broker::isMatches(BrokerItemMask::CRAFT_DESIGN_COOKING, *withoutActions)) << "no actions: no CraftLearnAction";
	const templates::item::ItemTemplate* unknownRecipe = registerItem(R"(id="152200003")", "<actions/>");
	registry.craftLearnActions[unknownRecipe->getActions()] =
		test::bindStatic<templates::item::actions::CraftLearnAction>(R"(<craftlearn recipeid="9"/>)");
	EXPECT_FALSE(broker::isMatches(BrokerItemMask::CRAFT_DESIGN_COOKING, *gameobjects::Item::create(9, unknownRecipe))) << "no recipe template";
	const templates::item::ItemTemplate* wrongMask = registerItem(R"(id="152000004")", "<actions/>");
	registry.craftLearnActions[wrongMask->getActions()] = craftLearn;
	EXPECT_FALSE(broker::isMatches(BrokerItemMask::CRAFT_DESIGN_COOKING, *gameobjects::Item::create(10, wrongMask))) << "mask 1520 is no design";
}

// ---- NpcEquippedGear (NpcEquippedGear.java) --------------------------------------------------------------------------------------------------

class NpcEquippedGearTest : public testing::Test {
protected:
	void TearDown() override { runtime::Reclaimer::getInstance().drain(); }
};

/**
 * Runs a reader at every `Field::set` yield point of the initializing thread, i.e. in the middle of NpcEquippedGear::init: it reads the gear the
 * way SM_NPC_INFO does on another thread (getItem, then getItemsMask without the monitor once the map is visible).
 */
struct GearPublicationProbe {
	static inline NpcEquippedGear* gear = nullptr;
	static inline std::thread::id initializer{};
	static inline bool inside = false;
	static inline int32_t visible = 0;
	static inline std::vector<int32_t> masksSeen{};

	static void yield(const char* site) noexcept {
		if (inside || std::this_thread::get_id() != initializer || std::string_view(site) != "Field::set")
			return;
		inside = true;
		if (gear->getItem(ItemSlot::MAIN_HAND) != nullptr && masksSeen.size() < masksSeen.capacity()) {
			visible++;
			masksSeen.push_back(gear->getItemsMask());
		}
		inside = false;
	}
};

TEST_F(NpcEquippedGearTest, AReaderThatSeesTheItemsSeesTheWholeMask) {
#if !AION_PCT
	GTEST_SKIP() << "needs the yield points of checked builds";
#else
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	const auto* sword = test::bindStatic<templates::item::ItemTemplate>(R"(<item_template id="100000011" item_group="SWORD"/>)");
	const auto* torso = test::bindStatic<templates::item::ItemTemplate>(R"(<item_template id="110000011" item_group="CL_TORSO"/>)");
	const auto* ring = test::bindStatic<templates::item::ItemTemplate>(R"(<item_template id="122000011" item_group="RING"/>)");
	auto list = std::make_unique<dataholders::loadingutils::adapters::NpcEquipmentList>();
	list->items = {sword, torso, ring};
	Ref<NpcEquippedGear> gear = NpcEquippedGear::create(std::move(list));

	GearPublicationProbe::gear = gear.get();
	GearPublicationProbe::initializer = std::this_thread::get_id();
	GearPublicationProbe::visible = 0;
	GearPublicationProbe::masksSeen.clear();
	GearPublicationProbe::masksSeen.reserve(64);
	static constexpr runtime::pct::PctHooks hooks{&GearPublicationProbe::yield, nullptr, nullptr};
	struct Install {
		Install() { runtime::pct::installHooks(&hooks); }
		~Install() { runtime::pct::installHooks(nullptr); }
	};
	{
		Install install;
		gear->init();
	}
	// Deviation (D6): Java publishes the empty TreeMap before filling it, so this reader could see MAIN_HAND with mask 1 (or 0)
	EXPECT_GE(GearPublicationProbe::visible, 1) << "the reader must also run after the map is published (v = null)";
	for (int32_t seen : GearPublicationProbe::masksSeen)
		EXPECT_EQ(seen, 1 | 8 | 256) << "MAIN_HAND | TORSO | RING_LEFT";
	EXPECT_EQ(gear->getItemsMask(), 1 | 8 | 256);
	GearPublicationProbe::gear = nullptr;
#endif
}

TEST_F(NpcEquippedGearTest, EachItemTakesItsFirstFreeSlot) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	const auto* swordA = test::bindStatic<templates::item::ItemTemplate>(R"(<item_template id="100000001" item_group="SWORD"/>)");
	const auto* swordB = test::bindStatic<templates::item::ItemTemplate>(R"(<item_template id="100000002" item_group="SWORD"/>)");
	const auto* swordC = test::bindStatic<templates::item::ItemTemplate>(R"(<item_template id="100000003" item_group="SWORD"/>)");
	const auto* torso = test::bindStatic<templates::item::ItemTemplate>(R"(<item_template id="110000001" item_group="CL_TORSO"/>)");
	const auto* ring = test::bindStatic<templates::item::ItemTemplate>(R"(<item_template id="122000001" item_group="RING"/>)");
	auto list = std::make_unique<dataholders::loadingutils::adapters::NpcEquipmentList>();
	list->items = {swordA, swordB, swordC, torso, ring};
	Ref<NpcEquippedGear> gear = NpcEquippedGear::create(std::move(list));
	EXPECT_EQ(gear->getItem(ItemSlot::MAIN_HAND), nullptr) << "Java: items is null before init";

	// SWORD (MAIN_OR_SUB): the first takes MAIN_HAND, the second SUB_HAND, the third finds no free slot; the mask is 1 | 2 | 8 | 256
	EXPECT_EQ(gear->getItemsMask(), 267);
	EXPECT_EQ(gear->getItem(ItemSlot::MAIN_HAND), swordA);
	EXPECT_EQ(gear->getItem(ItemSlot::SUB_HAND), swordB);
	EXPECT_EQ(gear->getItem(ItemSlot::TORSO), torso);
	EXPECT_EQ(gear->getItem(ItemSlot::RING_LEFT), ring);
	EXPECT_EQ(gear->getItem(ItemSlot::RING_RIGHT), nullptr);
	std::vector<ItemSlot> slots;
	for (const auto& entry : *gear)
		slots.push_back(entry.getKey());
	EXPECT_EQ(slots, (std::vector<ItemSlot>{ItemSlot::MAIN_HAND, ItemSlot::SUB_HAND, ItemSlot::TORSO, ItemSlot::RING_LEFT})) << "TreeMap order";
	auto it = gear->iterator();
	ASSERT_TRUE(it.hasNext());
	EXPECT_EQ(it.next().getValue(), swordA);
	gear->init();
	EXPECT_EQ(gear->getItemsMask(), 267) << "a second init keeps the items";

	auto noSlot = std::make_unique<dataholders::loadingutils::adapters::NpcEquipmentList>();
	noSlot->items = {test::bindStatic<templates::item::ItemTemplate>(R"(<item_template id="182000001"/>)")};
	EXPECT_THROW(static_cast<void>(NpcEquippedGear::create(std::move(noSlot))->getItemsMask()), runtime::IllegalArgumentException)
		<< "ItemSlot.getSlotsFor(0)";
}

// ---- drop model (Drop.java, DropModifiers.java, DropItem.java, DropGroup.java, NpcDrop.java) -------------------------------------------------

class DropModelTest : public testing::Test {
protected:
	void TearDown() override { runtime::Reclaimer::getInstance().drain(); }

	test::StaticDataScope staticData;
};

TEST_F(DropModelTest, DropConstructorValidatesLikeAfterUnmarshal) {
	drop::Drop drop(100, 3, 0, 12.5f);
	EXPECT_EQ(drop.getMaxAmount(), 3) << "maxAmount 0 defaults to minAmount";
	EXPECT_FALSE(drop.isEachMember());
	EXPECT_EQ(drop.toString(), "Drop [itemId=100, minAmount=3, maxAmount=3, chance=12.5, eachMember=false]");
	const auto message = [](int32_t minAmount, int32_t maxAmount, float chance) {
		try {
			drop::Drop invalid(100, minAmount, maxAmount, chance);
			return std::string("no exception");
		} catch (const runtime::IllegalArgumentException& e) {
			return std::string(e.what());
		}
	};
	EXPECT_EQ(message(1, 1, 0.0f), "chance (0.0) for drop 100 must be greater than zero");
	EXPECT_EQ(message(0, 1, 50.0f), "minAmount (0) for drop 100 must be greater than zero");
	EXPECT_EQ(message(5, 2, 50.0f), "maxAmount (2) for drop 100 must be greater than minAmount (5)");

	const drop::Drop* bound = test::bindStatic<drop::Drop>(R"(<drop item_id="7" min_amount="2" each_member="true"/>)");
	EXPECT_EQ(bound->getMaxAmount(), 2);
	EXPECT_EQ(bound->getChance(), 100.0f);
	EXPECT_TRUE(bound->isEachMember());
	try {
		static_cast<void>(test::bindStatic<drop::Drop>(R"(<drop item_id="7" chance="-1"/>)"));
		ADD_FAILURE() << "the hook rejects a negative chance";
	} catch (const xml::StaticDataException& e) {
		EXPECT_NE(std::string(e.what()).find("chance (-1.0) for drop 7 must be greater than zero"), std::string::npos) << e.what();
	}
}

TEST_F(DropModelTest, DropModifiersApplyTheReductionOnlyWhenAllowed) {
	drop::DropModifiers modifiers;
	modifiers.setBoostDropRate(2.0f);
	EXPECT_EQ(modifiers.calculateDropChance(10.0f, true), 20.0f) << "no reduction rate";
	modifiers.setReductionDropRate(0.25f);
	EXPECT_EQ(modifiers.calculateDropChance(10.0f, true), 5.0f);
	EXPECT_EQ(modifiers.calculateDropChance(10.0f, false), 20.0f);
}

TEST_F(DropModelTest, DropItemReadsTheItemTemplateAndTheLooters) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	registerItem(R"(id="166020001" option_slot_bonus="1")");
	registerItem(R"(id="188053548")");
	static const drop::Drop omega(166020001, 2, 2, 100.0f);
	static const drop::Drop box(188053548, 1, 1, 100.0f);
	static const drop::Drop unknown(123, 1, 1, 100.0f);
	Ref<drop::DropItem> omegaItem = drop::DropItem::create(&omega);
	EXPECT_EQ(omegaItem->getOptionalSocket(), -1) << "option_slot_bonus != 0";
	EXPECT_EQ(omegaItem->getLootEffectId(), 1003);
	omegaItem->calculateCount();
	EXPECT_EQ(omegaItem->getCount(), 2);
	Ref<drop::DropItem> boxItem = drop::DropItem::create(&box);
	EXPECT_EQ(boxItem->getOptionalSocket(), 0);
	EXPECT_EQ(boxItem->getLootEffectId(), 1002);
	EXPECT_THROW(static_cast<void>(drop::DropItem::create(&unknown)), runtime::NullPointerException) << "Java dereferences the missing template";

	EXPECT_TRUE(boxItem->canViewDropItem(42)) << "no looters: everyone";
	boxItem->setPlayerObjId(0);
	boxItem->setPlayerObjId(42);
	boxItem->setPlayerObjId(42);
	EXPECT_EQ(boxItem->getPlayerObjIds().size(), 1) << "ids <= 0 and duplicates are ignored";
	EXPECT_TRUE(boxItem->canViewDropItem(42));
	EXPECT_FALSE(boxItem->canViewDropItem(43));
	EXPECT_FALSE(boxItem->getWinningPlayer());
}

TEST_F(DropModelTest, DropItemOwnsARunTimeDrop) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	registerItem(R"(id="166020002" option_slot_bonus="1")");
	// Java DropRegistrationService/QuestService: new DropItem(new Drop(itemId, 1, 1, 100)), referenced only by the drop item
	Ref<drop::DropItem> fromTemporary = drop::DropItem::create(drop::Drop(166020002, 1, 1, 100.0f));
	Ref<drop::DropItem> fromLocal;
	const drop::Drop* source = nullptr;
	{
		drop::Drop runtimeDrop(166020002, 3, 3, 50.0f);
		source = &runtimeDrop;
		fromLocal = drop::DropItem::create(std::move(runtimeDrop));
		runtimeDrop = drop::Drop(1, 1, 1, 100.0f); // the source object changes and is destroyed below
	}
	ASSERT_NE(fromLocal->getDropTemplate(), nullptr);
	EXPECT_NE(fromLocal->getDropTemplate(), source) << "the drop item keeps its own copy";
	EXPECT_EQ(fromLocal->getDropTemplate()->getItemId(), 166020002);
	EXPECT_EQ(fromLocal->getDropTemplate()->getMinAmount(), 3);
	EXPECT_EQ(fromLocal->getDropTemplate()->getChance(), 50.0f);
	EXPECT_EQ(fromLocal->getOptionalSocket(), -1) << "read from the owned drop while the drop item was constructed";
	EXPECT_EQ(fromLocal->getLootEffectId(), 1003);
	fromLocal->calculateCount();
	EXPECT_EQ(fromLocal->getCount(), 3);
	EXPECT_EQ(fromTemporary->getDropTemplate()->getItemId(), 166020002);
	EXPECT_EQ(fromTemporary->getDropTemplate()->getMaxAmount(), 1);
	EXPECT_NE(fromTemporary->getDropTemplate(), fromLocal->getDropTemplate());
	EXPECT_THROW(static_cast<void>(drop::DropItem::create(drop::Drop(123, 1, 1, 100.0f))), runtime::NullPointerException)
		<< "Java dereferences the missing item template";
}

TEST_F(DropModelTest, DropGroupPicksTheNearestChance) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	registerItem(R"(id="1")");
	registerItem(R"(id="2")");
	const drop::DropGroup* bothCertain = test::bindStatic<drop::DropGroup>(
		R"(<drop_group name="both" max_items="5"><drop item_id="1" min_amount="2" max_amount="4"/><drop item_id="2"/></drop_group>)");
	drop::DropModifiers modifiers;
	modifiers.setBoostDropRate(1.0f);
	Ref<runtime::RcHashSet<Ref<drop::DropItem>>> result = runtime::RcHashSet<Ref<drop::DropItem>>::create();
	EXPECT_EQ(bothCertain->tryAddDropItems(*result, 10, modifiers, {}), 12) << "each drop at most once: 2 items, indexes 10 and 11";
	std::set<int32_t> itemIds;
	std::set<int32_t> indexes;
	for (Ptr<drop::DropItem> item : *result) {
		itemIds.insert(item->getDropTemplate()->getItemId());
		indexes.insert(item->getIndex());
		if (item->getDropTemplate()->getItemId() == 1) {
			EXPECT_GE(item->getCount(), 2);
			EXPECT_LE(item->getCount(), 4);
		}
	}
	EXPECT_EQ(itemIds, (std::set<int32_t>{1, 2}));
	EXPECT_EQ(indexes, (std::set<int32_t>{10, 11}));

	modifiers.setBoostDropRate(0.0f);
	result->clear();
	EXPECT_EQ(bothCertain->tryAddDropItems(*result, 0, modifiers, {}), 0) << "final chance 0: Rnd.chance() < 0 never holds";
	EXPECT_TRUE(result->isEmpty());

	// max_items 1, chances 100 and 50: a roll below 50 qualifies both and the 50% drop is nearer (50 - roll < 100 - roll)
	const drop::DropGroup* nearest =
		test::bindStatic<drop::DropGroup>(R"(<drop_group name="nearest"><drop item_id="1"/><drop item_id="2" chance="50"/></drop_group>)");
	modifiers.setBoostDropRate(1.0f);
	std::set<int32_t> seen;
	for (uint64_t seed = 1; seed <= 40; seed++) {
		commons::utils::Rnd::seedCurrentThreadForTests(seed);
		float roll = commons::utils::Rnd::chance();
		commons::utils::Rnd::seedCurrentThreadForTests(seed);
		result->clear();
		ASSERT_EQ(nearest->tryAddDropItems(*result, 0, modifiers, {}), 1);
		int32_t dropped = (*result->snapshot().begin())->getDropTemplate()->getItemId();
		EXPECT_EQ(dropped, roll < 50.0f ? 2 : 1) << "seed " << seed << " roll " << roll;
		seen.insert(dropped);
	}
	EXPECT_EQ(seen.size(), 2u) << "the seeds cover both branches";
}

TEST_F(DropModelTest, NpcDropUsesTheGroupsOfTheDropRace) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	registerItem(R"(id="11")");
	registerItem(R"(id="12")");
	registerItem(R"(id="13")");
	const drop::NpcDrop* npcDrop = test::bindStatic<drop::NpcDrop>(R"(<npc_drop npc_id="200000">)"
																	 R"(<drop_group race="ELYOS"><drop item_id="11"/></drop_group>)"
																	 R"(<drop_group race="ASMODIANS"><drop item_id="12"/></drop_group>)"
																	 R"(<drop_group><drop item_id="13"/></drop_group></npc_drop>)");
	drop::DropModifiers modifiers;
	modifiers.setBoostDropRate(1.0f);
	modifiers.setDropRace(Race::ELYOS);
	Ref<runtime::RcHashSet<Ref<drop::DropItem>>> result = runtime::RcHashSet<Ref<drop::DropItem>>::create();
	EXPECT_EQ(npcDrop->dropCalculator(*result, 0, modifiers, {}), 2);
	std::set<int32_t> itemIds;
	for (Ptr<drop::DropItem> item : *result)
		itemIds.insert(item->getDropTemplate()->getItemId());
	EXPECT_EQ(itemIds, (std::set<int32_t>{11, 13})) << "ELYOS and PC_ALL groups";

	const drop::NpcDrop* empty = test::bindStatic<drop::NpcDrop>(R"(<npc_drop npc_id="200001"/>)");
	EXPECT_EQ(empty->dropCalculator(*result, 5, modifiers, {}), 5);
}

// ---- trade values (TradeItem.java, TradePSItem.java, TradeList.java, ExchangeItem.java) ------------------------------------------------------

class TradeModelTest : public testing::Test {
protected:
	void TearDown() override { runtime::Reclaimer::getInstance().drain(); }

	test::StaticDataScope staticData;
};

TEST_F(TradeModelTest, PrivateStoreItemCountNeverBecomesNegative) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	const templates::item::ItemTemplate* potion = registerItem(R"(id="160000001" max_stack_count="100")");
	Ref<trade::TradePSItem> item = trade::TradePSItem::create(700, 160000001, 5, 1000);
	EXPECT_EQ(item->getItemObjId(), 700);
	EXPECT_EQ(item->getPrice(), 1000);
	EXPECT_EQ(item->getItemTemplate(), potion);
	item->decreaseCount(0);
	item->decreaseCount(-3);
	EXPECT_EQ(item->getCount(), 5) << "only positive decreases";
	item->decreaseCount(3);
	EXPECT_EQ(item->getCount(), 2);
	item->decreaseCount(10);
	EXPECT_EQ(item->getCount(), 0) << "at most the remaining count";
	EXPECT_EQ(trade::TradeItem::create(1, 1)->getItemTemplate(), nullptr) << "unknown item id";

	Ref<trade::TradeList> list = trade::TradeList::create(77);
	EXPECT_EQ(list->getSellerObjId(), 77);
	EXPECT_EQ(trade::TradeList::create()->getSellerObjId(), 0);
	list->addItem(160000001, 2);
	list->addTradeItem(*item);
	EXPECT_EQ(list->size(), 2);
	EXPECT_EQ(list->getTradeItems().get(0)->getCount(), 2);
	EXPECT_EQ(list->getTradeItems().get(1).get(), static_cast<trade::TradeItem*>(item.get()));
	EXPECT_EQ(list->getRequiredKinah(), 0);
	EXPECT_TRUE(list->getRequiredItems().isEmpty());

	Ref<gameobjects::Item> stack = gameobjects::Item::create(900, potion, 2, false, 0);
	Ref<trade::ExchangeItem> exchangeItem = trade::ExchangeItem::create(900, 2, *stack);
	exchangeItem->addCount(3);
	EXPECT_EQ(exchangeItem->getItemCount(), 5);
	EXPECT_EQ(stack->getItemCount(), 5) << "ExchangeItem.addCount sets the item count";
}

} // namespace
} // namespace aion::gameserver::model::items
