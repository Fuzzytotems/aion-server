#include "aion/gameserver/model/broker/BrokerItemMaskInfo.h"

#include <array>
#include <cstddef>
#include <optional>

#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/broker/filter/BrokerContainsExtraFilter.h"
#include "aion/gameserver/model/broker/filter/BrokerContainsFilter.h"
#include "aion/gameserver/model/broker/filter/BrokerFilter.h"
#include "aion/gameserver/model/broker/filter/BrokerMinMaxFilter.h"
#include "aion/gameserver/model/broker/filter/BrokerPlayerClassExtraFilter.h"
#include "aion/gameserver/model/broker/filter/BrokerRecipeFilter.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::broker {

namespace {

/** Java constructor arguments (typeId, parent, childrenExist) in ordinal order; the filters are in filters() */
struct BrokerItemMaskData {
	int32_t typeId;
	std::optional<BrokerItemMask> parent;
	bool childrenExist;
};

constexpr std::array<BrokerItemMaskData, 117> BROKER_ITEM_MASK_DATA{{
	{9010, std::nullopt, true}, // WEAPON
	{1000, BrokerItemMask::WEAPON, false}, // WEAPON_SWORD
	{1001, BrokerItemMask::WEAPON, false}, // WEAPON_MACE
	{1002, BrokerItemMask::WEAPON, false}, // WEAPON_DAGGER
	{1005, BrokerItemMask::WEAPON, false}, // WEAPON_ORB
	{1006, BrokerItemMask::WEAPON, false}, // WEAPON_SPELLBOOK
	{1009, BrokerItemMask::WEAPON, false}, // WEAPON_GREATSWORD
	{1013, BrokerItemMask::WEAPON, false}, // WEAPON_POLEARM
	{1015, BrokerItemMask::WEAPON, false}, // WEAPON_STAFF
	{1017, BrokerItemMask::WEAPON, false}, // WEAPON_BOW
	{1018, BrokerItemMask::WEAPON, false}, // WEAPON_PISTOL
	{1019, BrokerItemMask::WEAPON, false}, // WEAPON_AETHERCANNON
	{1020, BrokerItemMask::WEAPON, false}, // WEAPON_GARP
	{1021, BrokerItemMask::WEAPON, false}, // WEAPON_KEYBLADE
	{9020, std::nullopt, true}, // ARMOR
	{8010, BrokerItemMask::ARMOR, true}, // ARMOR_CLOTHING
	{1100, BrokerItemMask::ARMOR_CLOTHING, false}, // ARMOR_CLOTHING_JACKET
	{1110, BrokerItemMask::ARMOR_CLOTHING, false}, // ARMOR_CLOTHING_GLOVES
	{1120, BrokerItemMask::ARMOR_CLOTHING, false}, // ARMOR_CLOTHING_PAULDRONS
	{1130, BrokerItemMask::ARMOR_CLOTHING, false}, // ARMOR_CLOTHING_PANTS
	{1140, BrokerItemMask::ARMOR_CLOTHING, false}, // ARMOR_CLOTHING_SHOES
	{8020, BrokerItemMask::ARMOR, true}, // ARMOR_CLOTH
	{1101, BrokerItemMask::ARMOR_CLOTH, false}, // ARMOR_CLOTH_JACKET
	{1111, BrokerItemMask::ARMOR_CLOTH, false}, // ARMOR_CLOTH_GLOVES
	{1121, BrokerItemMask::ARMOR_CLOTH, false}, // ARMOR_CLOTH_PAULDRONS
	{1131, BrokerItemMask::ARMOR_CLOTH, false}, // ARMOR_CLOTH_PANTS
	{1141, BrokerItemMask::ARMOR_CLOTH, false}, // ARMOR_CLOTH_SHOES
	{8030, BrokerItemMask::ARMOR, true}, // ARMOR_LEATHER
	{1103, BrokerItemMask::ARMOR_LEATHER, false}, // ARMOR_LEATHER_JACKET
	{1113, BrokerItemMask::ARMOR_LEATHER, false}, // ARMOR_LEATHER_GLOVES
	{1123, BrokerItemMask::ARMOR_LEATHER, false}, // ARMOR_LEATHER_PAULDRONS
	{1133, BrokerItemMask::ARMOR_LEATHER, false}, // ARMOR_LEATHER_PANTS
	{1143, BrokerItemMask::ARMOR_LEATHER, false}, // ARMOR_LEATHER_SHOES
	{8040, BrokerItemMask::ARMOR, true}, // ARMOR_CHAIN
	{1105, BrokerItemMask::ARMOR_CHAIN, false}, // ARMOR_CHAIN_JACKET
	{1115, BrokerItemMask::ARMOR_CHAIN, false}, // ARMOR_CHAIN_GLOVES
	{1125, BrokerItemMask::ARMOR_CHAIN, false}, // ARMOR_CHAIN_PAULDRONS
	{1135, BrokerItemMask::ARMOR_CHAIN, false}, // ARMOR_CHAIN_PANTS
	{1145, BrokerItemMask::ARMOR_CHAIN, false}, // ARMOR_CHAIN_SHOES
	{8050, BrokerItemMask::ARMOR, true}, // ARMOR_PLATE
	{1106, BrokerItemMask::ARMOR_PLATE, false}, // ARMOR_PLATE_JACKET
	{1116, BrokerItemMask::ARMOR_PLATE, false}, // ARMOR_PLATE_GLOVES
	{1126, BrokerItemMask::ARMOR_PLATE, false}, // ARMOR_PLATE_PAULDRONS
	{1136, BrokerItemMask::ARMOR_PLATE, false}, // ARMOR_PLATE_PANTS
	{1146, BrokerItemMask::ARMOR_PLATE, false}, // ARMOR_PLATE_SHOES
	{1150, BrokerItemMask::ARMOR, false}, // ARMOR_SHIELD
	{9030, std::nullopt, true}, // ACCESSORY
	{1200, BrokerItemMask::ACCESSORY, false}, // ACCESSORY_EARRINGS
	{1210, BrokerItemMask::ACCESSORY, false}, // ACCESSORY_NECKLACE
	{1220, BrokerItemMask::ACCESSORY, false}, // ACCESSORY_RING
	{1230, BrokerItemMask::ACCESSORY, false}, // ACCESSORY_BELT
	{7030, BrokerItemMask::ACCESSORY, false}, // ACCESSORY_HEADGEAR
	{1871, BrokerItemMask::ACCESSORY, false}, // ACCESSORY_PLUME
	{9040, std::nullopt, true}, // SKILL_RELATED
	{1400, BrokerItemMask::SKILL_RELATED, true}, // SKILL_RELATED_STIGMA
	{6010, BrokerItemMask::SKILL_RELATED_STIGMA, false}, // SKILL_RELATED_STIGMA_GLADIATOR
	{6011, BrokerItemMask::SKILL_RELATED_STIGMA, false}, // SKILL_RELATED_STIGMA_TEMPLAR
	{6012, BrokerItemMask::SKILL_RELATED_STIGMA, false}, // SKILL_RELATED_STIGMA_ASSASSIN
	{6013, BrokerItemMask::SKILL_RELATED_STIGMA, false}, // SKILL_RELATED_STIGMA_RANGER
	{6014, BrokerItemMask::SKILL_RELATED_STIGMA, false}, // SKILL_RELATED_STIGMA_SORCERER
	{6015, BrokerItemMask::SKILL_RELATED_STIGMA, false}, // SKILL_RELATED_STIGMA_SPIRITMASTER
	{6016, BrokerItemMask::SKILL_RELATED_STIGMA, false}, // SKILL_RELATED_STIGMA_CLERIC
	{6017, BrokerItemMask::SKILL_RELATED_STIGMA, false}, // SKILL_RELATED_STIGMA_CHANTER
	{6018, BrokerItemMask::SKILL_RELATED_STIGMA, false}, // SKILL_RELATED_STIGMA_GUNSLINGER
	{6019, BrokerItemMask::SKILL_RELATED_STIGMA, false}, // SKILL_RELATED_STIGMA_SONGWEAVER
	{6048, BrokerItemMask::SKILL_RELATED_STIGMA, false}, // SKILL_RELATED_STIGMA_RIDER
	{1695, BrokerItemMask::SKILL_RELATED, true}, // SKILL_RELATED_SKILL_MANUAL
	{6020, BrokerItemMask::SKILL_RELATED_SKILL_MANUAL, false}, // SKILL_RELATED_SKILL_MANUAL_GLADIATOR
	{6021, BrokerItemMask::SKILL_RELATED_SKILL_MANUAL, false}, // SKILL_RELATED_SKILL_MANUAL_TEMPLAR
	{6022, BrokerItemMask::SKILL_RELATED_SKILL_MANUAL, false}, // SKILL_RELATED_SKILL_MANUAL_ASSASSIN
	{6023, BrokerItemMask::SKILL_RELATED_SKILL_MANUAL, false}, // SKILL_RELATED_SKILL_MANUAL_RANGER
	{6024, BrokerItemMask::SKILL_RELATED_SKILL_MANUAL, false}, // SKILL_RELATED_SKILL_MANUAL_SORCERER
	{6025, BrokerItemMask::SKILL_RELATED_SKILL_MANUAL, false}, // SKILL_RELATED_SKILL_MANUAL_SPIRITMASTER
	{6026, BrokerItemMask::SKILL_RELATED_SKILL_MANUAL, false}, // SKILL_RELATED_SKILL_MANUAL_CLERIC
	{6027, BrokerItemMask::SKILL_RELATED_SKILL_MANUAL, false}, // SKILL_RELATED_SKILL_MANUAL_CHANTER
	{6028, BrokerItemMask::SKILL_RELATED_SKILL_MANUAL, false}, // SKILL_RELATED_SKILL_MANUAL_GUNSLINGER
	{6029, BrokerItemMask::SKILL_RELATED_SKILL_MANUAL, false}, // SKILL_RELATED_SKILL_MANUAL_SONGWEAVER
	{6049, BrokerItemMask::SKILL_RELATED_SKILL_MANUAL, false}, // SKILL_RELATED_SKILL_MANUAL_RIDER
	{9070, std::nullopt, true}, // HOME_DECOR
	{1710, BrokerItemMask::HOME_DECOR, false}, // HOME_DECOR_OUT_DOOR
	{1711, BrokerItemMask::HOME_DECOR, false}, // HOME_DECOR_IN_DOOR
	{9080, std::nullopt, true}, // FURNITURE
	{1703, BrokerItemMask::FURNITURE, false}, // FURNITURE_OUT_DOOR
	{8070, BrokerItemMask::FURNITURE, true}, // FURNITURE_IN_DOOR
	{1700, BrokerItemMask::FURNITURE_IN_DOOR, false}, // FURNITURE_IN_DOOR_WALL_MOUNTED
	{1701, BrokerItemMask::FURNITURE_IN_DOOR, false}, // FURNITURE_IN_DOOR_FREE_STANDING
	{1702, BrokerItemMask::FURNITURE_IN_DOOR, false}, // FURNITURE_IN_DOOR_RUGS
	{1704, BrokerItemMask::FURNITURE, false}, // FURNITURE_IN_DOOR_OUT_DOOR
	{9050, std::nullopt, true}, // CRAFT
	{1520, BrokerItemMask::CRAFT, true}, // CRAFT_MATERIALS
	{6030, BrokerItemMask::CRAFT_MATERIALS, false}, // CRAFT_MATERIALS_GATHERED
	{6031, BrokerItemMask::CRAFT_MATERIALS, false}, // CRAFT_MATERIALS_LOOTED
	{6032, BrokerItemMask::CRAFT_MATERIALS, false}, // CRAFT_MATERIALS_COMPONENTS
	{1522, BrokerItemMask::CRAFT, true}, // CRAFT_DESIGN
	{6040, BrokerItemMask::CRAFT_DESIGN, false}, // CRAFT_DESIGN_WEAPONSMITHING
	{6041, BrokerItemMask::CRAFT_DESIGN, false}, // CRAFT_DESIGN_ARMORSMITHING
	{6042, BrokerItemMask::CRAFT_DESIGN, false}, // CRAFT_DESIGN_TAILORING
	{6043, BrokerItemMask::CRAFT_DESIGN, false}, // CRAFT_DESIGN_HANDICRAFTING
	{6044, BrokerItemMask::CRAFT_DESIGN, false}, // CRAFT_DESIGN_ALCHEMY
	{6045, BrokerItemMask::CRAFT_DESIGN, false}, // CRAFT_DESIGN_COOKING
	{6046, BrokerItemMask::CRAFT_DESIGN, false}, // CRAFT_DESIGN_CONSTRUCTION
	{9060, std::nullopt, true}, // CONSUMABLES
	{1600, BrokerItemMask::CONSUMABLES, false}, // CONSUMABLES_FOOD
	{1620, BrokerItemMask::CONSUMABLES, false}, // CONSUMABLES_POTION
	{7060, BrokerItemMask::CONSUMABLES, false}, // CONSUMABLES_SCROLL
	{8060, BrokerItemMask::CONSUMABLES, true}, // CONSUMABLES_MODIFY
	{1660, BrokerItemMask::CONSUMABLES_MODIFY, false}, // CONSUMABLES_MODIFY_ENCHANTMENT_STONE
	{1670, BrokerItemMask::CONSUMABLES_MODIFY, false}, // CONSUMABLES_MODIFY_MANASTONE
	{7065, BrokerItemMask::CONSUMABLES_MODIFY, false}, // CONSUMABLES_MODIFY_TEMPERING_SOLUTION
	{1680, BrokerItemMask::CONSUMABLES_MODIFY, false}, // CONSUMABLES_MODIFY_GODSTONE
	{7061, BrokerItemMask::CONSUMABLES_MODIFY, false}, // CONSUMABLES_MODIFY_DYE
	{7064, BrokerItemMask::CONSUMABLES_MODIFY, false}, // CONSUMABLES_MODIFY_PAIN
	{1665, BrokerItemMask::CONSUMABLES_MODIFY, false}, // CONSUMABLES_MODIFY_AMPLIFICATION_STONE
	{7063, BrokerItemMask::CONSUMABLES_MODIFY, false}, // CONSUMABLES_MODIFY_OTHER
	{7062, BrokerItemMask::CONSUMABLES, false}, // CONSUMABLES_OTHER
	{7070, std::nullopt, false}, // OTHER
	{1, std::nullopt, false}, // UNKNOWN
}};
static_assert(static_cast<size_t>(BrokerItemMask::UNKNOWN) + 1 == BROKER_ITEM_MASK_DATA.size(), "one entry per BrokerItemMask constant");

const BrokerItemMaskData& maskData(BrokerItemMask mask) noexcept {
	return BROKER_ITEM_MASK_DATA[static_cast<size_t>(mask)];
}

using Filters = std::array<runtime::Ref<filter::BrokerFilter>, 117>;

/**
 * Java: the filter argument of each constant, in ordinal order. Created on the first isMatches call (thread-safe static initialization) and
 * never released, like the Java enum constants; static data never refers to them.
 */
const Filters& filters() {
	static const Filters* const table = new Filters{{
		filter::BrokerMinMaxFilter::create(1000, 1021), // WEAPON
		filter::BrokerContainsFilter::create({1000}), // WEAPON_SWORD
		filter::BrokerContainsFilter::create({1001}), // WEAPON_MACE
		filter::BrokerContainsFilter::create({1002}), // WEAPON_DAGGER
		filter::BrokerContainsFilter::create({1005}), // WEAPON_ORB
		filter::BrokerContainsFilter::create({1006}), // WEAPON_SPELLBOOK
		filter::BrokerContainsFilter::create({1009}), // WEAPON_GREATSWORD
		filter::BrokerContainsFilter::create({1013}), // WEAPON_POLEARM
		filter::BrokerContainsFilter::create({1015}), // WEAPON_STAFF
		filter::BrokerContainsFilter::create({1017}), // WEAPON_BOW
		filter::BrokerContainsFilter::create({1018}), // WEAPON_PISTOL
		filter::BrokerContainsFilter::create({1019}), // WEAPON_AETHERCANNON
		filter::BrokerContainsFilter::create({1020}), // WEAPON_GARP
		filter::BrokerContainsFilter::create({1021}), // WEAPON_KEYBLADE
		filter::BrokerMinMaxFilter::create(1100, 1150), // ARMOR
		filter::BrokerContainsFilter::create({1100, 1110, 1120, 1130, 1140}), // ARMOR_CLOTHING
		filter::BrokerContainsFilter::create({1100}), // ARMOR_CLOTHING_JACKET
		filter::BrokerContainsFilter::create({1110}), // ARMOR_CLOTHING_GLOVES
		filter::BrokerContainsFilter::create({1120}), // ARMOR_CLOTHING_PAULDRONS
		filter::BrokerContainsFilter::create({1130}), // ARMOR_CLOTHING_PANTS
		filter::BrokerContainsFilter::create({1140}), // ARMOR_CLOTHING_SHOES
		filter::BrokerContainsFilter::create({1101, 1111, 1121, 1131, 1141}), // ARMOR_CLOTH
		filter::BrokerContainsFilter::create({1101}), // ARMOR_CLOTH_JACKET
		filter::BrokerContainsFilter::create({1111}), // ARMOR_CLOTH_GLOVES
		filter::BrokerContainsFilter::create({1121}), // ARMOR_CLOTH_PAULDRONS
		filter::BrokerContainsFilter::create({1131}), // ARMOR_CLOTH_PANTS
		filter::BrokerContainsFilter::create({1141}), // ARMOR_CLOTH_SHOES
		filter::BrokerContainsFilter::create({1103, 1113, 1123, 1133, 1143}), // ARMOR_LEATHER
		filter::BrokerContainsFilter::create({1103}), // ARMOR_LEATHER_JACKET
		filter::BrokerContainsFilter::create({1113}), // ARMOR_LEATHER_GLOVES
		filter::BrokerContainsFilter::create({1123}), // ARMOR_LEATHER_PAULDRONS
		filter::BrokerContainsFilter::create({1133}), // ARMOR_LEATHER_PANTS
		filter::BrokerContainsFilter::create({1143}), // ARMOR_LEATHER_SHOES
		filter::BrokerContainsFilter::create({1105, 1115, 1125, 1135, 1145}), // ARMOR_CHAIN
		filter::BrokerContainsFilter::create({1105}), // ARMOR_CHAIN_JACKET
		filter::BrokerContainsFilter::create({1115}), // ARMOR_CHAIN_GLOVES
		filter::BrokerContainsFilter::create({1125}), // ARMOR_CHAIN_PAULDRONS
		filter::BrokerContainsFilter::create({1135}), // ARMOR_CHAIN_PANTS
		filter::BrokerContainsFilter::create({1145}), // ARMOR_CHAIN_SHOES
		filter::BrokerContainsFilter::create({1106, 1116, 1126, 1136, 1146}), // ARMOR_PLATE
		filter::BrokerContainsFilter::create({1106}), // ARMOR_PLATE_JACKET
		filter::BrokerContainsFilter::create({1116}), // ARMOR_PLATE_GLOVES
		filter::BrokerContainsFilter::create({1126}), // ARMOR_PLATE_PAULDRONS
		filter::BrokerContainsFilter::create({1136}), // ARMOR_PLATE_PANTS
		filter::BrokerContainsFilter::create({1146}), // ARMOR_PLATE_SHOES
		filter::BrokerContainsFilter::create({1150}), // ARMOR_SHIELD
		filter::BrokerContainsFilter::create({1200, 1210, 1220, 1230, 1250, 1871}), // ACCESSORY
		filter::BrokerContainsFilter::create({1200}), // ACCESSORY_EARRINGS
		filter::BrokerContainsFilter::create({1210}), // ACCESSORY_NECKLACE
		filter::BrokerContainsFilter::create({1220}), // ACCESSORY_RING
		filter::BrokerContainsFilter::create({1230}), // ACCESSORY_BELT
		filter::BrokerContainsFilter::create({1250}), // ACCESSORY_HEADGEAR
		filter::BrokerContainsFilter::create({1871}), // ACCESSORY_PLUME
		filter::BrokerContainsFilter::create({1400, 1695}), // SKILL_RELATED
		filter::BrokerContainsFilter::create({1400}), // SKILL_RELATED_STIGMA
		filter::BrokerPlayerClassExtraFilter::create(1400, PlayerClass::GLADIATOR), // SKILL_RELATED_STIGMA_GLADIATOR
		filter::BrokerPlayerClassExtraFilter::create(1400, PlayerClass::TEMPLAR), // SKILL_RELATED_STIGMA_TEMPLAR
		filter::BrokerPlayerClassExtraFilter::create(1400, PlayerClass::ASSASSIN), // SKILL_RELATED_STIGMA_ASSASSIN
		filter::BrokerPlayerClassExtraFilter::create(1400, PlayerClass::RANGER), // SKILL_RELATED_STIGMA_RANGER
		filter::BrokerPlayerClassExtraFilter::create(1400, PlayerClass::SORCERER), // SKILL_RELATED_STIGMA_SORCERER
		filter::BrokerPlayerClassExtraFilter::create(1400, PlayerClass::SPIRIT_MASTER), // SKILL_RELATED_STIGMA_SPIRITMASTER
		filter::BrokerPlayerClassExtraFilter::create(1400, PlayerClass::CLERIC), // SKILL_RELATED_STIGMA_CLERIC
		filter::BrokerPlayerClassExtraFilter::create(1400, PlayerClass::CHANTER), // SKILL_RELATED_STIGMA_CHANTER
		filter::BrokerPlayerClassExtraFilter::create(1400, PlayerClass::GUNNER), // SKILL_RELATED_STIGMA_GUNSLINGER
		filter::BrokerPlayerClassExtraFilter::create(1400, PlayerClass::BARD), // SKILL_RELATED_STIGMA_SONGWEAVER
		filter::BrokerPlayerClassExtraFilter::create(1400, PlayerClass::RIDER), // SKILL_RELATED_STIGMA_RIDER
		filter::BrokerContainsFilter::create({1695}), // SKILL_RELATED_SKILL_MANUAL
		filter::BrokerPlayerClassExtraFilter::create(1695, PlayerClass::GLADIATOR), // SKILL_RELATED_SKILL_MANUAL_GLADIATOR
		filter::BrokerPlayerClassExtraFilter::create(1695, PlayerClass::TEMPLAR), // SKILL_RELATED_SKILL_MANUAL_TEMPLAR
		filter::BrokerPlayerClassExtraFilter::create(1695, PlayerClass::ASSASSIN), // SKILL_RELATED_SKILL_MANUAL_ASSASSIN
		filter::BrokerPlayerClassExtraFilter::create(1695, PlayerClass::RANGER), // SKILL_RELATED_SKILL_MANUAL_RANGER
		filter::BrokerPlayerClassExtraFilter::create(1695, PlayerClass::SORCERER), // SKILL_RELATED_SKILL_MANUAL_SORCERER
		filter::BrokerPlayerClassExtraFilter::create(1695, PlayerClass::SPIRIT_MASTER), // SKILL_RELATED_SKILL_MANUAL_SPIRITMASTER
		filter::BrokerPlayerClassExtraFilter::create(1695, PlayerClass::CLERIC), // SKILL_RELATED_SKILL_MANUAL_CLERIC
		filter::BrokerPlayerClassExtraFilter::create(1695, PlayerClass::CHANTER), // SKILL_RELATED_SKILL_MANUAL_CHANTER
		filter::BrokerPlayerClassExtraFilter::create(1695, PlayerClass::GUNNER), // SKILL_RELATED_SKILL_MANUAL_GUNSLINGER
		filter::BrokerPlayerClassExtraFilter::create(1695, PlayerClass::BARD), // SKILL_RELATED_SKILL_MANUAL_SONGWEAVER
		filter::BrokerPlayerClassExtraFilter::create(1695, PlayerClass::RIDER), // SKILL_RELATED_SKILL_MANUAL_RIDER
		filter::BrokerContainsFilter::create({1710, 1711}), // HOME_DECOR
		filter::BrokerContainsFilter::create({1710}), // HOME_DECOR_OUT_DOOR
		filter::BrokerContainsFilter::create({1711}), // HOME_DECOR_IN_DOOR
		filter::BrokerContainsFilter::create({1700, 1701, 1702, 1703, 1704}), // FURNITURE
		filter::BrokerContainsFilter::create({1703}), // FURNITURE_OUT_DOOR
		filter::BrokerContainsFilter::create({1700, 1701, 1702}), // FURNITURE_IN_DOOR
		filter::BrokerContainsFilter::create({1700}), // FURNITURE_IN_DOOR_WALL_MOUNTED
		filter::BrokerContainsFilter::create({1701}), // FURNITURE_IN_DOOR_FREE_STANDING
		filter::BrokerContainsFilter::create({1702}), // FURNITURE_IN_DOOR_RUGS
		filter::BrokerContainsFilter::create({1704}), // FURNITURE_IN_DOOR_OUT_DOOR
		filter::BrokerContainsFilter::create({1520, 1522}), // CRAFT
		filter::BrokerContainsFilter::create({1520}), // CRAFT_MATERIALS
		filter::BrokerContainsExtraFilter::create({15200}), // CRAFT_MATERIALS_GATHERED
		filter::BrokerContainsExtraFilter::create({15201}), // CRAFT_MATERIALS_LOOTED
		filter::BrokerContainsExtraFilter::create({15202}), // CRAFT_MATERIALS_COMPONENTS
		filter::BrokerContainsFilter::create({1522}), // CRAFT_DESIGN
		filter::BrokerRecipeFilter::create(40002, {1522}), // CRAFT_DESIGN_WEAPONSMITHING
		filter::BrokerRecipeFilter::create(40003, {1522}), // CRAFT_DESIGN_ARMORSMITHING
		filter::BrokerRecipeFilter::create(40004, {1522}), // CRAFT_DESIGN_TAILORING
		filter::BrokerRecipeFilter::create(40008, {1522}), // CRAFT_DESIGN_HANDICRAFTING
		filter::BrokerRecipeFilter::create(40007, {1522}), // CRAFT_DESIGN_ALCHEMY
		filter::BrokerRecipeFilter::create(40001, {1522}), // CRAFT_DESIGN_COOKING
		filter::BrokerRecipeFilter::create(40010, {1522}), // CRAFT_DESIGN_CONSTRUCTION
		filter::BrokerContainsFilter::create({1410, 1600, 1620, 1640, 1660, 1661, 1665, 1670, 1680, 1690, 1692, 1693, 1694, 1696}), // CONSUMABLES
		filter::BrokerContainsFilter::create({1600}), // CONSUMABLES_FOOD
		filter::BrokerContainsFilter::create({1620}), // CONSUMABLES_POTION
		filter::BrokerContainsFilter::create({1640}), // CONSUMABLES_SCROLL
		filter::BrokerContainsFilter::create({1660, 1665, 1670, 1680, 1692, 1691}), // CONSUMABLES_MODIFY
		filter::BrokerContainsExtraFilter::create({16600, 16602}), // CONSUMABLES_MODIFY_ENCHANTMENT_STONE
		filter::BrokerContainsFilter::create({1670}), // CONSUMABLES_MODIFY_MANASTONE
		filter::BrokerContainsExtraFilter::create({16603}), // CONSUMABLES_MODIFY_TEMPERING_SOLUTION
		filter::BrokerContainsFilter::create({1680}), // CONSUMABLES_MODIFY_GODSTONE
		filter::BrokerContainsFilter::create({1692}), // CONSUMABLES_MODIFY_DYE
		filter::BrokerContainsFilter::create({1691}), // CONSUMABLES_MODIFY_PAIN
		filter::BrokerContainsFilter::create({1665}), // CONSUMABLES_MODIFY_AMPLIFICATION_STONE
		filter::BrokerContainsFilter::create({1661}), // CONSUMABLES_MODIFY_OTHER
		filter::BrokerContainsFilter::create({1410, 1690, 1693, 1694, 1696}), // CONSUMABLES_OTHER
		filter::BrokerContainsFilter::create({1850, 1860, 1870, 1880, 1881, 1887}), // OTHER
		filter::BrokerContainsFilter::create({0}), // UNKNOWN
	}};
	return *table;
}

} // namespace

int32_t getId(BrokerItemMask mask) noexcept {
	return maskData(mask).typeId;
}

bool isMatches(BrokerItemMask mask, gameobjects::Item& item) {
	return filters()[static_cast<size_t>(mask)]->accept(item.getItemTemplate());
}

bool isChildrenMask(BrokerItemMask mask, int32_t maskId) noexcept {
	for (std::optional<BrokerItemMask> p = maskData(mask).parent; p.has_value(); p = maskData(*p).parent) {
		if (maskData(*p).typeId == maskId)
			return true;
	}
	return false;
}

BrokerItemMask getBrokerMaskById(int32_t id) noexcept {
	for (size_t i = 0; i < BROKER_ITEM_MASK_DATA.size(); ++i) {
		if (BROKER_ITEM_MASK_DATA[i].typeId == id)
			return static_cast<BrokerItemMask>(i);
	}
	return BrokerItemMask::UNKNOWN;
}

bool hasChildren(BrokerItemMask mask) noexcept {
	return maskData(mask).childrenExist;
}

} // namespace aion::gameserver::model::broker
