// P4-07a item templates (docs/design/handlers-and-porting-plan.md §3.2): the enum companions, the binder hooks and setters of the item package
// on small XML fixtures, and the logic methods. Expected values are derived by hand from the Java sources (ItemTemplate.java, ItemGroup.java,
// ItemSlot.java, ItemMask.java, RandomItem.java, ResultedItem.java, Stigma.java, ...).

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemSetData.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataException.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/templates/item/AcquisitionTypeInfo.h"
#include "aion/gameserver/model/templates/item/AssembledItem.h"
#include "aion/gameserver/model/templates/item/AssemblyItem.bind.h"
#include "aion/gameserver/model/templates/item/ExtractedItemsCollection.bind.h"
#include "aion/gameserver/model/templates/item/ItemActivationTargetInfo.h"
#include "aion/gameserver/model/templates/item/ItemAttackTypeInfo.h"
#include "aion/gameserver/model/templates/item/ItemQualityInfo.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.bind.h"
#include "aion/gameserver/model/templates/item/RandomItem.bind.h"
#include "aion/gameserver/model/templates/item/RandomTypeInfo.h"
#include "aion/gameserver/model/templates/item/RequireSkill.h"
#include "aion/gameserver/model/templates/item/ResultedItem.bind.h"
#include "aion/gameserver/model/templates/item/ReturnLocList.bind.h"
#include "aion/gameserver/model/templates/item/Stigma.bind.h"
#include "aion/gameserver/model/templates/item/TradeinItem.bind.h"
#include "aion/gameserver/model/templates/item/WeaponStats.bind.h"
#include "aion/gameserver/model/templates/item/WeaponTypeInfo.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroupInfo.h"
#include "aion/gameserver/model/templates/item/enums/ItemSubTypeInfo.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::templates::item {
namespace {

using enums::ArmorType;
using enums::EquipType;
using enums::ItemGroup;
using enums::ItemSubType;

// ---- enum companions (Java constructor data) ------------------------------------------------------------------------------------------------

/** ItemSlot.java bit definitions (P4-13's enum has no companion yet): the masks in ItemGroupInfo.h must be these values */
constexpr int64_t MAIN_HAND = 1LL;
constexpr int64_t SUB_HAND = 1LL << 1;
constexpr int64_t HELMET = 1LL << 2;
constexpr int64_t TORSO = 1LL << 3;
constexpr int64_t GLOVES = 1LL << 4;
constexpr int64_t BOOTS = 1LL << 5;
constexpr int64_t EARRINGS_LEFT = 1LL << 6;
constexpr int64_t EARRINGS_RIGHT = 1LL << 7;
constexpr int64_t RING_LEFT = 1LL << 8;
constexpr int64_t RING_RIGHT = 1LL << 9;
constexpr int64_t NECKLACE = 1LL << 10;
constexpr int64_t SHOULDER = 1LL << 11;
constexpr int64_t PANTS = 1LL << 12;
constexpr int64_t POWER_SHARD_RIGHT = 1LL << 13;
constexpr int64_t POWER_SHARD_LEFT = 1LL << 14;
constexpr int64_t WINGS = 1LL << 15;
constexpr int64_t WAIST = 1LL << 16;
constexpr int64_t PLUME = 1LL << 19;
constexpr int64_t MAIN_OR_SUB = MAIN_HAND | SUB_HAND;
constexpr int64_t ALL_STIGMA = (1LL << 30) | (1LL << 31) | (1LL << 32) | (1LL << 33) | (1LL << 34) | (1LL << 35);

static_assert(enums::getValidEquipmentSlots(ItemGroup::NONE) == 0 && enums::getValidEquipmentSlots(ItemGroup::SWORD) == MAIN_OR_SUB);
static_assert(enums::getValidEquipmentSlots(ItemGroup::SHIELD) == SUB_HAND && enums::getValidEquipmentSlots(ItemGroup::GLOVE) == GLOVES);
static_assert(enums::getValidEquipmentSlots(ItemGroup::SHOES) == BOOTS && enums::getValidEquipmentSlots(ItemGroup::PL_SHOULDER) == SHOULDER);
static_assert(enums::getValidEquipmentSlots(ItemGroup::EARRING) == (EARRINGS_LEFT | EARRINGS_RIGHT));
static_assert(enums::getValidEquipmentSlots(ItemGroup::RING) == (RING_LEFT | RING_RIGHT));
static_assert(enums::getValidEquipmentSlots(ItemGroup::NECKLACE) == NECKLACE);
static_assert(enums::getValidEquipmentSlots(ItemGroup::BELT) == WAIST && enums::getValidEquipmentSlots(ItemGroup::WING) == WINGS);
static_assert(enums::getValidEquipmentSlots(ItemGroup::PLUME) == PLUME && enums::getValidEquipmentSlots(ItemGroup::HEAD) == HELMET);
static_assert(enums::getValidEquipmentSlots(ItemGroup::CL_MULTISLOT) == (TORSO | PANTS));
static_assert(enums::getValidEquipmentSlots(ItemGroup::POWER_SHARDS) == (POWER_SHARD_RIGHT | POWER_SHARD_LEFT));
static_assert(enums::getValidEquipmentSlots(ItemGroup::STIGMA) == ALL_STIGMA && enums::getValidEquipmentSlots(ItemGroup::NPC_MACE) == MAIN_HAND);
static_assert(enums::getValidEquipmentSlots(ItemGroup::ARROW) == 0 && enums::getValidEquipmentSlots(ItemGroup::STIGMA_SHARD) == 0);

// ItemGroup(long, ArmorType): subtype NONE, armor type set, equip type ARMOR;
// ItemGroup(long, ItemSubType): armor type null, equip type of the subtype
static_assert(enums::getItemSubType(ItemGroup::EARRING) == ItemSubType::NONE && enums::getArmorType(ItemGroup::EARRING) == ArmorType::ACCESSORY);
static_assert(enums::getEquipType(ItemGroup::EARRING) == EquipType::ARMOR);
static_assert(enums::getItemSubType(ItemGroup::GREATSWORD) == ItemSubType::TWO_HAND && !enums::getArmorType(ItemGroup::GREATSWORD).has_value());
static_assert(enums::getEquipType(ItemGroup::GREATSWORD) == EquipType::WEAPON && enums::getEquipType(ItemGroup::RB_TORSO) == EquipType::ARMOR);
static_assert(enums::getEquipType(ItemGroup::STIGMA) == EquipType::STIGMA && enums::getEquipType(ItemGroup::PLUME) == EquipType::PLUME);
static_assert(enums::getEquipType(ItemGroup::ARROW) == EquipType::NONE && enums::getEquipType(ItemGroup::MANASTONE) == EquipType::NONE);
static_assert(enums::getArmorType(ItemSubType::WING) == ArmorType::GENERAL && !enums::getArmorType(ItemSubType::ONE_HAND).has_value());
static_assert(enums::getEquipType(ItemSubType::SHIELD) == EquipType::ARMOR && enums::getEquipType(ItemSubType::STIGMA) == EquipType::STIGMA);

static_assert(getId(AcquisitionType::REWARD) == 2 && getId(AcquisitionType::COUPON) == 2 && getId(AcquisitionType::ABYSS) == 1);
static_assert(getRace(ItemActivationTarget::KRALL) == Race::KRALL && !getRace(ItemActivationTarget::TARGET).has_value());
static_assert(isMagical(ItemAttackType::MAGICAL_WIND) && getMagicalElement(ItemAttackType::MAGICAL_WIND) == SkillElement::WIND);
static_assert(!isMagical(ItemAttackType::PHYSICAL) && getMagicalElement(ItemAttackType::PHYSICAL) == SkillElement::NONE);
static_assert(getQualityId(ItemQuality::JUNK) == 0 && getQualityId(ItemQuality::MYTHIC) == 6);
static_assert(getLevel(RandomType::ENCHANTMENT) == 0 && getLevel(RandomType::MANASTONE_RARE_GRADE_40) == 40);
static_assert(getLevel(RandomType::SPECIAL_MANASTONE_EPIC_GRADE) == 70 && getLevel(RandomType::PREMIUM_OPHIDAN_RECIPE) == 0);
static_assert(getRequiredSlots(WeaponType::DAGGER_1H) == 1 && getRequiredSlots(WeaponType::KEYHAMMER_2H) == 2);
static_assert(getMask(WeaponType::DAGGER_1H) == 1 && getMask(WeaponType::BOW) == (1 << 12));

TEST(ItemEnumCompanionsTest, RequiredSkillArraysKeepJavaOrder) {
	EXPECT_EQ(std::vector<int32_t>(enums::getRequiredSkills(ItemGroup::DAGGER).begin(), enums::getRequiredSkills(ItemGroup::DAGGER).end()),
		(std::vector<int32_t>{66, 45}));
	EXPECT_EQ(std::vector<int32_t>(enums::getRequiredSkills(ItemGroup::HARP).begin(), enums::getRequiredSkills(ItemGroup::HARP).end()),
		(std::vector<int32_t>{124, 114}));
	EXPECT_TRUE(enums::getRequiredSkills(ItemGroup::NONE).empty()) << "Java new int[] {}, never null";
	EXPECT_TRUE(enums::getRequiredSkills(ItemGroup::BELT).empty());
	EXPECT_EQ(std::vector<int32_t>(getRequiredSkills(WeaponType::HARP_2H).begin(), getRequiredSkills(WeaponType::HARP_2H).end()),
		(std::vector<int32_t>{92, 78}));
	EXPECT_TRUE(getRequiredSkills(WeaponType::TOOLHOE_1H).empty());
}

// ---- binding helpers --------------------------------------------------------------------------------------------------------------------------

template <class T>
std::unique_ptr<T> bindXml(xml::LoadContext& context, std::string_view text) {
	return xml::bindString<T>(context, text);
}

template <class T>
std::unique_ptr<T> bindXml(std::string_view text) {
	xml::LoadContext context;
	return bindXml<T>(context, text);
}

// ---- ItemTemplate -----------------------------------------------------------------------------------------------------------------------------

TEST(ItemTemplateTest, SetXmlUidParsesTheIdAndTheHookFillsDefaults) {
	std::unique_ptr<ItemTemplate> item = bindXml<ItemTemplate>(R"(<item_template id="100000001" name="Sword" desc="900001" level="10"/>)");
	EXPECT_EQ(item->getTemplateId(), 100000001);
	EXPECT_EQ(item->getName(), "Sword");
	EXPECT_EQ(item->getL10nId(), 900001);
	ASSERT_NE(item->getWeaponStats(), nullptr) << "afterUnmarshal: emptyWeaponStats";
	EXPECT_EQ(item->getWeaponStats()->getMaxDamage(), 0);
	ASSERT_NE(item->getUseLimits(), nullptr) << "afterUnmarshal: emptyUseLimits";
	EXPECT_EQ(item->getUseLimits()->getMinRank(), 1);
	EXPECT_EQ(item->getUseLimits()->getMaxRank(), 18);
	EXPECT_EQ(item->getMaxTuneCount(), 0) << "item group NONE has no slot: cannot be randomized";
	EXPECT_EQ(item->getExtraInventoryId(), -1);
	EXPECT_EQ(item->getModifiers(), nullptr) << "Java null without a modifiers element";
	EXPECT_FALSE(item->hasWorldRestrictions());
	EXPECT_FALSE(item->hasAreaRestriction()) << "no usearea";
	EXPECT_EQ(item->getRobotId(), 0);
	EXPECT_FALSE(item->isStigma());
}

TEST(ItemTemplateTest, MaxTuneCountRules) {
	// equipment without a tune count, bonus or random bonus: 0; with any of them: -1 stays; an explicit count stays
	EXPECT_EQ(bindXml<ItemTemplate>(R"(<item_template id="1" item_group="SWORD"/>)")->getMaxTuneCount(), 0);
	EXPECT_EQ(bindXml<ItemTemplate>(R"(<item_template id="1" item_group="SWORD" max_enchant_bonus="1"/>)")->getMaxTuneCount(), -1);
	EXPECT_EQ(bindXml<ItemTemplate>(R"(<item_template id="1" item_group="SWORD" option_slot_bonus="2"/>)")->getMaxTuneCount(), -1);
	EXPECT_EQ(bindXml<ItemTemplate>(R"(<item_template id="1" item_group="SWORD" rnd_bonus="5"/>)")->getMaxTuneCount(), -1);
	EXPECT_EQ(bindXml<ItemTemplate>(R"(<item_template id="1" item_group="SWORD" rnd_count="3"/>)")->getMaxTuneCount(), 3);
	EXPECT_EQ(bindXml<ItemTemplate>(R"(<item_template id="1" item_group="RECIPE" rnd_count="3" rnd_bonus="5"/>)")->getMaxTuneCount(), 0);
	EXPECT_TRUE(bindXml<ItemTemplate>(R"(<item_template id="1" item_group="SWORD" rnd_bonus="5"/>)")->canTune());
	EXPECT_FALSE(bindXml<ItemTemplate>(R"(<item_template id="1" item_group="SWORD"/>)")->canTune());
}

TEST(ItemTemplateTest, HooksDoNotRunWithoutRunHooks) {
	xml::LoadOptions options;
	options.runHooks = false;
	xml::LoadContext context(options);
	std::unique_ptr<ItemTemplate> item = bindXml<ItemTemplate>(context, R"(<item_template id="7" item_group="SWORD"/>)");
	EXPECT_EQ(item->getTemplateId(), 7) << "the annotated setter is part of binding";
	EXPECT_EQ(item->getWeaponStats(), nullptr);
	EXPECT_EQ(item->getMaxTuneCount(), -1);
}

TEST(ItemTemplateTest, EquipmentTypesAndHands) {
	std::unique_ptr<ItemTemplate> sword = bindXml<ItemTemplate>(R"(<item_template id="1" item_group="SWORD"/>)");
	EXPECT_EQ(sword->getItemSlot(), MAIN_OR_SUB);
	EXPECT_TRUE(sword->isWeapon());
	EXPECT_FALSE(sword->isArmor());
	EXPECT_TRUE(sword->isOneHandWeapon());
	EXPECT_FALSE(sword->isTwoHandWeapon());
	EXPECT_EQ(sword->getItemSubType(), ItemSubType::ONE_HAND);
	EXPECT_EQ(std::vector<int32_t>(sword->getRequiredSkills().begin(), sword->getRequiredSkills().end()), (std::vector<int32_t>{37, 44}));
	std::unique_ptr<ItemTemplate> staff = bindXml<ItemTemplate>(R"(<item_template id="1" item_group="STAFF"/>)");
	EXPECT_TRUE(staff->isTwoHandWeapon());
	std::unique_ptr<ItemTemplate> shield = bindXml<ItemTemplate>(R"(<item_template id="1" item_group="SHIELD"/>)");
	EXPECT_TRUE(shield->isArmor()) << "ItemSubType.SHIELD is an armor subtype";
	EXPECT_FALSE(shield->isTwoHandWeapon());

	// isCloth: armor and ((armorType != ACCESSORY and not BELT) or HEAD)
	EXPECT_TRUE(bindXml<ItemTemplate>(R"(<item_template id="1" item_group="CL_TORSO"/>)")->isCloth()) << "armor type null != ACCESSORY";
	EXPECT_TRUE(bindXml<ItemTemplate>(R"(<item_template id="1" item_group="HEAD"/>)")->isCloth()) << "an accessory, but HEAD";
	EXPECT_FALSE(bindXml<ItemTemplate>(R"(<item_template id="1" item_group="NECKLACE"/>)")->isCloth());
	EXPECT_FALSE(bindXml<ItemTemplate>(R"(<item_template id="1" item_group="BELT"/>)")->isCloth());
	EXPECT_FALSE(bindXml<ItemTemplate>(R"(<item_template id="1" item_group="SWORD"/>)")->isCloth());
	EXPECT_TRUE(bindXml<ItemTemplate>(R"(<item_template id="1" item_group="COMBINATION"/>)")->isCombinationItem());
	EXPECT_TRUE(bindXml<ItemTemplate>(R"(<item_template id="1" item_group="ENCHANTMENT"/>)")->isEnchantmentStone());
}

TEST(ItemTemplateTest, LevelRestrictionsByClassWithStartingClassFallback) {
	// 17 classes in PlayerClass order: WARRIOR GLADIATOR TEMPLAR SCOUT ASSASSIN RANGER MAGE SORCERER SPIRIT_MASTER PRIEST CLERIC CHANTER ENGINEER
	// RIDER GUNNER ARTIST BARD
	std::unique_ptr<ItemTemplate> item = bindXml<ItemTemplate>(
		R"(<item_template id="1" restrict="10 0 0 0 20 0 0 0 0 0 0 0 0 0 0 0 0" restrict_max="0 0 0 0 55 0 0 0 0 0 0 0 0 0 0 0 0"/>)");
	EXPECT_TRUE(item->isClassSpecific(PlayerClass::WARRIOR));
	EXPECT_TRUE(item->isClassSpecific(PlayerClass::GLADIATOR)) << "0 for itself, but its starting class WARRIOR has 10";
	EXPECT_TRUE(item->isClassSpecific(PlayerClass::TEMPLAR));
	EXPECT_FALSE(item->isClassSpecific(PlayerClass::SCOUT)) << "a starting class is not checked twice";
	EXPECT_TRUE(item->isClassSpecific(PlayerClass::ASSASSIN));
	EXPECT_FALSE(item->isClassSpecific(PlayerClass::RANGER)) << "SCOUT has 0";
	EXPECT_FALSE(item->isClassSpecific(PlayerClass::BARD));
	EXPECT_EQ(item->getRequiredLevel(PlayerClass::WARRIOR), 10);
	EXPECT_EQ(item->getRequiredLevel(PlayerClass::GLADIATOR), -1) << "no fallback in getRequiredLevel";
	EXPECT_EQ(item->getRequiredLevel(PlayerClass::ASSASSIN), 20);
	EXPECT_EQ(item->getMaxLevelRestrict(PlayerClass::ASSASSIN), 55);
	EXPECT_EQ(item->getMaxLevelRestrict(PlayerClass::WARRIOR), 0);

	std::unique_ptr<ItemTemplate> defaults = bindXml<ItemTemplate>(R"(<item_template id="2"/>)");
	EXPECT_EQ(defaults->getRequiredLevel(PlayerClass::BARD), 1) << "DEFAULT_LEVEL_RESTRICTION: 17 times 1";
	EXPECT_EQ(defaults->getMaxLevelRestrict(PlayerClass::BARD), 0) << "restrict_max absent: 0";

	std::unique_ptr<ItemTemplate> shortRestrict = bindXml<ItemTemplate>(R"(<item_template id="3" restrict="1 1"/>)");
	EXPECT_THROW(shortRestrict->getRequiredLevel(PlayerClass::BARD), runtime::ArrayIndexOutOfBoundsException) << "Java array access";
}

TEST(ItemTemplateTest, MaskFlagsAndMaskChanges) {
	// ItemMask: LIMIT_ONE 1, TRADEABLE 2, BREAKABLE 64, SOUL_BOUND 128, NO_ENCHANT 512, CAN_COMPOSITE_WEAPON 2048, CAN_SPLIT 8192, DELETABLE 16384,
	// DYEABLE 32768, CAN_POLISH 131072
	// 175047 = 1 + 2 + 4 + 64 + 128 + 256 + 512 + 2048 + 8192 + 32768 + 131072
	std::unique_ptr<ItemTemplate> item = bindXml<ItemTemplate>(R"(<item_template id="1" mask="175047"/>)");
	EXPECT_TRUE(item->hasLimitOne());
	EXPECT_TRUE(item->isTradeable());
	EXPECT_TRUE(item->isBreakable());
	EXPECT_TRUE(item->isSoulBound());
	EXPECT_TRUE(item->isNoEnchant());
	EXPECT_TRUE(item->canSplit());
	EXPECT_TRUE(item->isItemDyePermitted());
	EXPECT_TRUE(item->isCanPolish());
	EXPECT_TRUE(item->isCanFuse());
	EXPECT_FALSE(item->isDeletable());
	item->modifyMask(true, 16384);
	EXPECT_TRUE(item->isDeletable());
	item->modifyMask(false, 2);
	EXPECT_FALSE(item->isTradeable());
	EXPECT_EQ(item->getMask(), 175047 + 16384 - 2);
}

TEST(ItemTemplateTest, StackCountKinahCapAndPotions) {
	EXPECT_EQ(bindXml<ItemTemplate>(R"(<item_template id="1" max_stack_count="100"/>)")->getMaxStackCount(), 100);
	EXPECT_TRUE(bindXml<ItemTemplate>(R"(<item_template id="1" max_stack_count="2"/>)")->isStackable());
	EXPECT_FALSE(bindXml<ItemTemplate>(R"(<item_template id="1"/>)")->isStackable()) << "max_stack_count defaults to 1";
	std::unique_ptr<ItemTemplate> kinah = bindXml<ItemTemplate>(R"(<item_template id="182400001" max_stack_count="1"/>)");
	EXPECT_TRUE(kinah->isKinah());
	bool capEnabled = configs::main::CustomConfig::ENABLE_KINAH_CAP.load();
	int64_t capValue = configs::main::CustomConfig::KINAH_CAP_VALUE.load();
	configs::main::CustomConfig::ENABLE_KINAH_CAP.store(false);
	EXPECT_EQ(kinah->getMaxStackCount(), std::numeric_limits<int64_t>::max()) << "Long.MAX_VALUE";
	configs::main::CustomConfig::ENABLE_KINAH_CAP.store(true);
	configs::main::CustomConfig::KINAH_CAP_VALUE.store(999999999999LL);
	EXPECT_EQ(kinah->getMaxStackCount(), 999999999999LL);
	configs::main::CustomConfig::ENABLE_KINAH_CAP.store(capEnabled);
	configs::main::CustomConfig::KINAH_CAP_VALUE.store(capValue);
	EXPECT_TRUE(bindXml<ItemTemplate>(R"(<item_template id="162000000"/>)")->isPotion());
	EXPECT_FALSE(bindXml<ItemTemplate>(R"(<item_template id="163000000"/>)")->isPotion());
}

TEST(ItemTemplateTest, ActivationTargetAndRace) {
	std::unique_ptr<ItemTemplate> race = bindXml<ItemTemplate>(R"(<item_template id="1" activate_target="KRALL"/>)");
	EXPECT_EQ(race->getActivationRace(), Race::KRALL);
	EXPECT_EQ(race->getActivationTarget(), std::nullopt) << "a race target is not returned as target";
	std::unique_ptr<ItemTemplate> target = bindXml<ItemTemplate>(R"(<item_template id="1" activate_target="TARGET"/>)");
	EXPECT_EQ(target->getActivationRace(), std::nullopt);
	EXPECT_EQ(target->getActivationTarget(), ItemActivationTarget::TARGET);
	std::unique_ptr<ItemTemplate> none = bindXml<ItemTemplate>(R"(<item_template id="1"/>)");
	EXPECT_EQ(none->getActivationRace(), std::nullopt);
	EXPECT_EQ(none->getActivationTarget(), std::nullopt);
	EXPECT_EQ(bindXml<ItemTemplate>(R"(<item_template id="1" robot="7"/>)")->getRobotId(), 7);
}

TEST(ItemTemplateTest, UseLimitsAndWorldRestrictions) {
	std::unique_ptr<ItemTemplate> item = bindXml<ItemTemplate>(
		R"(<item_template id="1"><uselimits ownership_worlds="210010000 220010000" ride_usable="true" rank_min="3" rank_max="9"/></item_template>)");
	EXPECT_TRUE(item->hasWorldRestrictions());
	EXPECT_TRUE(item->isItemRestrictedToWorld(220010000));
	EXPECT_FALSE(item->isItemRestrictedToWorld(110010000));
	const ItemUseLimits* limits = item->getUseLimits();
	EXPECT_TRUE(limits->isRideUsable());
	EXPECT_TRUE(limits->verifyRank(3));
	EXPECT_TRUE(limits->verifyRank(9));
	EXPECT_FALSE(limits->verifyRank(2));
	EXPECT_FALSE(limits->verifyRank(10));
	EXPECT_EQ(limits->getUseArea(), nullptr);
	EXPECT_FALSE(bindXml<ItemTemplate>(R"(<item_template id="1"><uselimits/></item_template>)")->getUseLimits()->isRideUsable()) << "Boolean null";
	EXPECT_FALSE(bindXml<ItemTemplate>(R"(<item_template id="1"/>)")->isItemRestrictedToWorld(1)) << "empty ownership worlds";
}

// ---- small item classes ------------------------------------------------------------------------------------------------------------------------

TEST(ItemTemplatePartsTest, RandomItemHookDefaultsAndValidatesCounts) {
	EXPECT_EQ(bindXml<RandomItem>(R"(<random_item type="ENCHANTMENT" min_count="3"/>)")->getMaxCount(), 3) << "max_count 0 becomes min_count";
	EXPECT_EQ(bindXml<RandomItem>(R"(<random_item type="ENCHANTMENT"/>)")->getMaxCount(), 1);
	EXPECT_EQ(bindXml<RandomItem>(R"(<random_item type="ENCHANTMENT" min_count="2" max_count="5"/>)")->getMaxCount(), 5);
	try {
		bindXml<RandomItem>(R"(<random_item type="POTION" min_count="0"/>)");
		FAIL() << "min_count 0 must fail";
	} catch (const xml::StaticDataException& e) {
		EXPECT_NE(std::string(e.what()).find("Decomposable random reward item of type POTION min_count (0) must be greater than 0"), std::string::npos)
			<< e.what();
	}
	try {
		bindXml<RandomItem>(R"(<random_item min_count="4" max_count="3"/>)");
		FAIL() << "max_count below min_count must fail";
	} catch (const xml::StaticDataException& e) {
		EXPECT_NE(std::string(e.what()).find("of type null max_count (3) must be unset or greater than min_count (4)"), std::string::npos) << e.what();
	}
}

TEST(ItemTemplatePartsTest, ResultedItemHookChecksTheItemIdAgainstTheLoadedItems) {
	xml::LoadContext context;
	std::unique_ptr<ItemTemplate> item = bindXml<ItemTemplate>(context, R"(<item_template id="188051234"/>)");
	std::unique_ptr<ResultedItem> resulted = bindXml<ResultedItem>(context, R"(<item id="188051234" min_count="2"/>)");
	EXPECT_EQ(resulted->getMaxCount(), 2);
	EXPECT_EQ(resulted->getRace(), Race::PC_ALL);
	try {
		bindXml<ResultedItem>(context, R"(<item id="188059999"/>)");
		FAIL() << "an unknown item id must fail";
	} catch (const xml::StaticDataException& e) {
		EXPECT_NE(std::string(e.what()).find("Decomposable reward item ID is invalid: 188059999"), std::string::npos) << e.what();
	}
	try {
		bindXml<ResultedItem>(context, R"(<item id="188051234" min_count="5" max_count="1"/>)");
		FAIL() << "max_count below min_count must fail";
	} catch (const xml::StaticDataException& e) {
		EXPECT_NE(std::string(e.what()).find("Decomposable reward item [188051234] max_count (1) must be unset or greater than min_count (5)"),
			std::string::npos)
			<< e.what();
	}
}

TEST(ItemTemplatePartsTest, ResultedItemsCollectionListsAreEmptyWhenAbsent) {
	xml::LoadContext context;
	bindXml<ItemTemplate>(context, R"(<item_template id="1"/>)");
	std::unique_ptr<ExtractedItemsCollection> collection =
		bindXml<ExtractedItemsCollection>(context, R"(<items minlevel="10"><item id="1"/><random_item type="SCROLLS"/></items>)");
	EXPECT_EQ(collection->getItems().size(), 1u);
	EXPECT_EQ(collection->getRandomItems().size(), 1u);
	EXPECT_EQ(collection->getMinLevel(), 10);
	EXPECT_EQ(collection->getMaxLevel(), 99);
	EXPECT_FLOAT_EQ(collection->getChance(), 100.0f);
	std::unique_ptr<ExtractedItemsCollection> empty = bindXml<ExtractedItemsCollection>(context, R"(<items/>)");
	EXPECT_TRUE(empty->getItems().empty()) << "Collections.emptyList()";
	EXPECT_TRUE(empty->getRandomItems().empty());
	EXPECT_FLOAT_EQ(bindXml<ReturnLocList>(context, R"(<return index="3" worldid="210010000"/>)")->getIndex(), 3.0f);
}

TEST(ItemTemplatePartsTest, StigmaHookBuildsTheSkillGroups) {
	std::unique_ptr<Stigma> one = bindXml<Stigma>(R"(<stigma gain_skill_group1="G1"/>)");
	EXPECT_EQ(one->getGainSkillGroups(), (std::vector<std::string>{"G1"}));
	EXPECT_EQ(one->getGainSkillsByGroup(0), nullptr) << "group numbers are 1-based";
	EXPECT_EQ(one->getGainSkillsByGroup(2), nullptr);
	std::unique_ptr<Stigma> two = bindXml<Stigma>(R"(<stigma gain_skill_group1="G1" gain_skill_group2="G2" chargeable="true"/>)");
	EXPECT_EQ(two->getGainSkillGroups(), (std::vector<std::string>{"G1", "G2"}));
	EXPECT_TRUE(two->isChargeable());
}

TEST(ItemTemplatePartsTest, StigmaSkillsOfAGroupComeFromTheSkillData) {
	// Java Stigma.getGainSkillsByGroup: DataManager.SKILL_DATA.getSkillTemplatesByGroup(gainSkillGroups[groupNo - 1]) (header request pre-1)
	std::unique_ptr<Stigma> stigma = bindXml<Stigma>(R"(<stigma gain_skill_group1="G1" gain_skill_group2="G2"/>)");
	EXPECT_THROW(stigma->getGainSkillsByGroup(1), runtime::NullPointerException) << "SKILL_DATA is not published";
	EXPECT_EQ(stigma->getGainSkillsByGroup(3), nullptr) << "an invalid group number does not read the skill data";
	xml::LoadContext context;
	dataholders::DataManager::SKILL_DATA.publish(bindXml<dataholders::SkillData>(context, R"(<skill_data>)"
		R"(<skill_template skill_id="11" name="a" nameId="1" skilltype="MAGICAL" skillsubtype="BUFF" activation="ACTIVE" duration="0")"
		R"( stack="A1" group="G1"/>)"
		R"(<skill_template skill_id="12" name="b" nameId="1" skilltype="MAGICAL" skillsubtype="BUFF" activation="ACTIVE" duration="0")"
		R"( stack="B1" group="OTHER"/>)"
		R"(<skill_template skill_id="13" name="c" nameId="1" skilltype="MAGICAL" skillsubtype="BUFF" activation="ACTIVE" duration="0")"
		R"( stack="A2" group="G1"/>)"
		R"(</skill_data>)"));
	const std::vector<const skillengine::model::SkillTemplate*>* group1 = stigma->getGainSkillsByGroup(1);
	ASSERT_NE(group1, nullptr);
	ASSERT_EQ(group1->size(), 2u);
	EXPECT_EQ((*group1)[0]->getSkillId(), 11);
	EXPECT_EQ((*group1)[1]->getSkillId(), 13);
	EXPECT_EQ(stigma->getGainSkillsByGroup(2), nullptr) << "no skill of group G2: Java null";
	dataholders::DataManager::SKILL_DATA.resetForTests();
}

TEST(ItemTemplateTest, ItemSetComesFromTheItemSetData) {
	// Java ItemTemplate.getItemSet: DataManager.ITEM_SET_DATA.getItemSetTemplateByItemId(itemId) (header request pre-2); the holder with sets is
	// tested in tests/dataholders (its binding needs ItemSetTemplate's hook, P4-07b)
	std::unique_ptr<ItemTemplate> item = bindXml<ItemTemplate>(R"(<item_template id="100000001" name="Sword"/>)");
	EXPECT_THROW(item->isItemSet(), runtime::NullPointerException) << "ITEM_SET_DATA is not published";
	dataholders::DataManager::ITEM_SET_DATA.publish(std::make_unique<dataholders::ItemSetData>());
	EXPECT_EQ(item->getItemSet(), nullptr);
	EXPECT_FALSE(item->isItemSet());
	dataholders::DataManager::ITEM_SET_DATA.resetForTests();
}

TEST(ItemTemplatePartsTest, SmallAccessors) {
	EXPECT_EQ(bindXml<TradeinItem>(R"(<tradein_item id="5" price="3000000000"/>)")->toString(), "TradeinItem [id=5, price=3000000000]");
	EXPECT_FLOAT_EQ(bindXml<WeaponStats>(R"(<weapon_stats min_damage="11" max_damage="20"/>)")->getMeanDamage(), 15.5f);
	EXPECT_EQ(bindXml<AssemblyItem>(R"(<item id="3" parts="1 2 3"/>)")->getParts(), (std::vector<int32_t>{1, 2, 3}));
	EXPECT_EQ(AssembledItem().getId(), 0);
	RequireSkill requireSkill;
	requireSkill.getSkillIds().push_back(5);
	EXPECT_EQ(requireSkill.getSkillIds(), (std::vector<int32_t>{5}));
}

} // namespace
} // namespace aion::gameserver::model::templates::item
