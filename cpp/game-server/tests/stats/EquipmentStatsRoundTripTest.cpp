// The equip round trip of the item listener (P5-01, m5b3-plan.md P-02/P-06): Equipment.equipItem adds an item's stat functions through
// ItemEquipmentListener.onItemEquipment, and Equipment.unEquipItem must take every one of them away again through onItemUnequipment - the
// case that catches a missing `endEffect(item)` (m5b3-plan.md §10.4, the Y5 mutation) and a missing removeStoneStats.
//
// Java: ItemEquipmentListener.java:38-121 and :198-219; Equipment.java:59-194 (equipItem, equip) and :229-291 (unEquipItem, unEquip).
//
// The player is the item packet fixture's (tests/cm_ak/ItemPacketTestSupport.h: a spawned warrior in Poeta who knows the sword skill), the
// items rows of item_templates.xml: the Manastone Slot Test Fabled Sword (:2070, MAXHP +500 bonus) and the Manastone: HP +20 (:839282).
//
// The armor mastery update of the unequip (:120, updateArmorMasteryStats) is driven with the Training Hauberk (:232833) and the function
// ArmorMasteryEffect.startEffect builds for Basic Chain Armor Proficiency (skill_templates.xml:790-800, skill 42).
//
// NOT COVERED, and named so that nobody mistakes the absence for coverage: the enchant effect (EnchantService.applyEnchantEffect, its equip
// side, is ported since M5c stage 0, E-01; tests/itemsvc/EnchantServiceTest.cpp equips an enchanted sword, and its unequip round trip is not
// driven here yet), the tempering and random bonus effects, the idian stone, the conditioning observer, the buff skill and the item set (each
// needs data holders of its own; their unequip statements are ported one for one with their equip mirrors of M5a).

#include "../cm_ak/ItemPacketTestSupport.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/items/ItemSlot.h"
#include "aion/gameserver/model/items/ItemSlotInfo.h"
#include "aion/gameserver/model/items/ManaStone.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/calc/StatOwner.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatArmorMasteryFunction.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/stats/listeners/ItemEquipmentListener.h"
#include "aion/gameserver/model/templates/item/enums/ItemSubType.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::items {
namespace {

using model::stats::container::StatEnum;

constexpr int64_t MAIN_HAND = 1; // ItemSlot.MAIN_HAND.getSlotIdMask()

/** An item-use observer that counts its aborts (ItemUseObserver.java) */
struct CountingItemUseObserver final : controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND

	int32_t aborts = 0;

	static runtime::Ref<CountingItemUseObserver> create() { return runtime::makeRef<CountingItemUseObserver>(); }

	void abort() override { ++aborts; }

protected:
	CountingItemUseObserver() = default;
	~CountingItemUseObserver() override = default;
};

class EquipmentStatsRoundTripTest : public ItemPacketTest {
protected:
	void SetUp() override {
		ItemPacketTest::SetUp();
		runtime::resetUnportedHitsForTests();
		baseMaxHp = maxHp();
	}

	int32_t maxHp() { return player().getGameStats()->getMaxHp()->getCurrent(); }

	/** The MAXHP functions of the player's stats owned by `owner` */
	size_t maxHpFunctionsOf(const model::stats::calc::StatOwner& owner) {
		size_t count = 0;
		for (const runtime::Ptr<model::stats::calc::functions::IStatFunction>& function : player().getGameStats()->getStatsSorted(StatEnum::MAXHP)) {
			if (function->getOwner().get() == &owner)
				++count;
		}
		return count;
	}

	/** A manastone of the HP +20 row in the item's (or its fusioned weapon's) stone set, as ItemSocketService / the DAO put it there */
	runtime::Ref<model::items::ManaStone> addStone(Item& item, bool fusion) {
		runtime::Ref<model::items::ManaStone> stone = model::items::ManaStone::create(item.getObjectId(), MANASTONE_HP_20, fusion ? 1 : 0,
			model::gameobjects::Persistable_PersistentState::NEW);
		(fusion ? item.getFusionStones() : item.getItemStones())->add(stone);
		return stone;
	}

	int32_t baseMaxHp = 0;
};

TEST_F(EquipmentStatsRoundTripTest, UnequippingTakesAwayEveryFunctionEquippingAdded) {
	Item& sword = stored(790001, FABLED_TEST_SWORD, 1);
	runtime::Ref<model::items::ManaStone> stone = addStone(sword, false);
	runtime::Ref<model::items::ManaStone> fusionStone = addStone(sword, true);
	ASSERT_TRUE(sword.hasManaStones());
	ASSERT_TRUE(sword.hasFusionStones());
	ASSERT_EQ(player().getGameStats()->getStatsSorted(StatEnum::MAXHP).size(), 0u);

	ASSERT_TRUE(player().getEquipment().equipItem(790001, MAIN_HAND));

	// onItemEquipment (ItemEquipmentListener.java:42-49): the sword's modifier with the sword as owner, one function per stone
	EXPECT_EQ(maxHpFunctionsOf(sword), 1u);
	EXPECT_EQ(maxHpFunctionsOf(*stone), 1u);
	EXPECT_EQ(maxHpFunctionsOf(*fusionStone), 1u);
	EXPECT_EQ(maxHp(), baseMaxHp + 500 + 20 + 20);

	ASSERT_TRUE(player().getEquipment().unEquipItem(790001));

	// onItemUnequipment (:94-100): endEffect(item) and removeStoneStats for the stones and the fusion stones - nothing of the three is left
	EXPECT_EQ(maxHpFunctionsOf(sword), 0u) << "endEffect(item)";
	EXPECT_EQ(maxHpFunctionsOf(*stone), 0u) << "removeStoneStats(item.getItemStones())";
	EXPECT_EQ(maxHpFunctionsOf(*fusionStone), 0u) << "removeStoneStats(item.getFusionStones())";
	EXPECT_EQ(player().getGameStats()->getStatsSorted(StatEnum::MAXHP).size(), 0u);
	EXPECT_EQ(maxHp(), baseMaxHp);
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(EquipmentStatsRoundTripTest, ASecondRoundTripEndsWhereTheFirstBegan) {
	Item& sword = stored(790002, FABLED_TEST_SWORD, 1);
	addStone(sword, false);

	for (int32_t round = 1; round <= 2; ++round) {
		SCOPED_TRACE("round " + std::to_string(round));
		ASSERT_TRUE(player().getEquipment().equipItem(790002, MAIN_HAND));
		EXPECT_EQ(maxHp(), baseMaxHp + 520) << "no function left over from the round before adds twice";
		ASSERT_TRUE(player().getEquipment().unEquipItem(790002));
		EXPECT_EQ(maxHp(), baseMaxHp);
	}
}

TEST_F(EquipmentStatsRoundTripTest, RemoveStoneStatsEndsOnlyTheStonesOfTheSet) {
	// ItemEquipmentListener.java:211-219 on its own: the stones' functions go, another owner's stay; an empty set does nothing
	Item& sword = stored(790003, FABLED_TEST_SWORD, 1);
	runtime::Ref<model::items::ManaStone> first = addStone(sword, false);
	runtime::Ref<model::items::ManaStone> second = addStone(sword, false);
	model::stats::container::PlayerGameStats& stats = *player().getGameStats();
	model::stats::listeners::ItemEquipmentListener::addStoneStats(sword, first, stats);
	model::stats::listeners::ItemEquipmentListener::addStoneStats(sword, second, stats);
	ASSERT_EQ(maxHp(), baseMaxHp + 40);

	model::stats::listeners::ItemEquipmentListener::removeStoneStats({}, stats);
	EXPECT_EQ(maxHp(), baseMaxHp + 40);

	model::stats::listeners::ItemEquipmentListener::removeStoneStats({first}, stats);
	EXPECT_EQ(maxHpFunctionsOf(*first), 0u);
	EXPECT_EQ(maxHpFunctionsOf(*second), 1u);
	EXPECT_EQ(maxHp(), baseMaxHp + 20);

	model::stats::listeners::ItemEquipmentListener::removeStoneStats({first, second}, stats);
	EXPECT_EQ(maxHp(), baseMaxHp);
}

TEST_F(EquipmentStatsRoundTripTest, AnUnequipCancelsTheItemUseFirst) {
	// ItemEquipmentListener.java:87: owner.getController().cancelUseItem() - the item-use observers are aborted (PlayerController.java:548-550)
	Item& sword = equipped(790004, FABLED_TEST_SWORD, MAIN_HAND);
	runtime::Ref<CountingItemUseObserver> observer = CountingItemUseObserver::create();

	player().getObserveController()->attach(*observer);
	model::stats::listeners::ItemEquipmentListener::onItemUnequipment(sword, player());

	EXPECT_EQ(observer->aborts, 1);
}

/** A stat owner of the mastery function, standing for the passive's Effect (tests/stats/StatsTestSupport.h's TestStatOwner) */
class MasteryOwner final : public runtime::RefCounted, public model::stats::calc::StatOwner {
	AION_MAKE_REF_FRIEND
public:
	static runtime::Ref<MasteryOwner> create() { return runtime::makeRef<MasteryOwner>(); }

	void retain() const noexcept override { runtime::RefCounted::retain(); }
	void release() const noexcept override { runtime::RefCounted::release(); }

protected:
	MasteryOwner() = default;
	~MasteryOwner() override = default;
};

TEST_F(EquipmentStatsRoundTripTest, TheArmorMasteryFollowsTheArmourOnAndOff) {
	// Basic Chain Armor Proficiency (skill 42): <armormastery armor="CHAIN" value="1" delta="0"> with <change stat="PHYSICAL_DEFENSE"
	// func="PERCENT" value="10"/>. ArmorMasteryEffect.startEffect makes of it a StatArmorMasteryFunction(CHAIN, PHYSICAL_DEFENSE, 10, false,
	// fixedBonus = value + delta * level = 1) over the equipped items and adds it to the player's stats; the Training Hauberk is CH_TORSO
	// (ItemGroup.java: CHAIN, required skill 42 or 49), whose TORSO slot counts 30 (StatArmorMasteryFunction.getEquipmentFactor)
	constexpr int32_t CHAIN_ARMOR_SKILL = 42;
	player().setSkillList(model::skill::PlayerSkillList::create(
		{model::skill::PlayerSkillEntry::create(SWORD_SKILL, 1, 0, model::gameobjects::Persistable_PersistentState::UPDATED),
			model::skill::PlayerSkillEntry::create(CHAIN_ARMOR_SKILL, 1, 0, model::gameobjects::Persistable_PersistentState::UPDATED)}));
	stored(790005, TRAINING_HAUBERK, 1);
	runtime::Ref<model::stats::calc::functions::StatArmorMasteryFunction> mastery = model::stats::calc::functions::StatArmorMasteryFunction::create(
		model::templates::item::enums::ItemSubType::CHAIN, StatEnum::PHYSICAL_DEFENSE, 10, false, 1, player().getEquipment().getEquippedItems());
	runtime::Ref<MasteryOwner> owner = MasteryOwner::create();
	player().getGameStats()->addEffectOnly(runtime::Ptr<model::stats::calc::StatOwner>(*owner),
		{runtime::Ptr<model::stats::calc::functions::IStatFunction>(*mastery)});
	ASSERT_EQ(mastery->getValue(), 0) << "no chain armour on";

	ASSERT_TRUE(player().getEquipment().equipItem(790005, model::items::getSlotIdMask(model::items::ItemSlot::TORSO)));
	EXPECT_EQ(mastery->getValue(), 3) << "onItemEquipment's updateArmorMasteryStats (ItemEquipmentListener.java:77): 10 * 30 / 100";

	ASSERT_TRUE(player().getEquipment().unEquipItem(790005));
	EXPECT_EQ(mastery->getValue(), 0) << "onItemUnequipment's updateArmorMasteryStats (ItemEquipmentListener.java:120) over what is left on";
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::items
