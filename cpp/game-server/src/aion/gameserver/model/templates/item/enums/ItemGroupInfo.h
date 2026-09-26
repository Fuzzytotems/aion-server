#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "aion/gameserver/model/templates/item/enums/ArmorType.h"
#include "aion/gameserver/model/templates/item/enums/EquipType.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"
#include "aion/gameserver/model/templates/item/enums/ItemSubType.h"
#include "aion/gameserver/model/templates/item/enums/ItemSubTypeInfo.h"

namespace aion::gameserver::model::templates::item::enums {

/**
 * Companion of the generated enum ItemGroup (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and methods as
 * free functions found by ADL (`getItemSubType(group)` for Java `group.getItemSubType()`). A class with a same-named member (ItemTemplate)
 * calls them qualified (`enums::getItemSubType(itemGroup)`).
 * The slot masks are the values of Java's `ItemSlot.X.getSlotIdMask()` (the comment names the slots): ItemSlot is P4-13's enum and has no
 * companion yet; ItemGroupInfoTest checks the values against the ItemSlot bit definitions.
 *
 * @author xTz
 */

namespace detail {
/** Java `new int[] { ... }` required skill arrays (shared by equal contents; Java creates one array per constant) */
inline constexpr std::array<int32_t, 2> ITEM_GROUP_SKILLS_37_44{37, 44};
inline constexpr std::array<int32_t, 1> ITEM_GROUP_SKILLS_51{51};
inline constexpr std::array<int32_t, 2> ITEM_GROUP_SKILLS_66_45{66, 45};
inline constexpr std::array<int32_t, 2> ITEM_GROUP_SKILLS_39_46{39, 46};
inline constexpr std::array<int32_t, 1> ITEM_GROUP_SKILLS_111{111};
inline constexpr std::array<int32_t, 1> ITEM_GROUP_SKILLS_100{100};
inline constexpr std::array<int32_t, 1> ITEM_GROUP_SKILLS_52{52};
inline constexpr std::array<int32_t, 1> ITEM_GROUP_SKILLS_89{89};
inline constexpr std::array<int32_t, 1> ITEM_GROUP_SKILLS_53{53};
inline constexpr std::array<int32_t, 2> ITEM_GROUP_SKILLS_124_114{124, 114};
inline constexpr std::array<int32_t, 2> ITEM_GROUP_SKILLS_117_112{117, 112};
inline constexpr std::array<int32_t, 1> ITEM_GROUP_SKILLS_113{113};
inline constexpr std::array<int32_t, 2> ITEM_GROUP_SKILLS_112_115{112, 115};
inline constexpr std::array<int32_t, 2> ITEM_GROUP_SKILLS_43_50{43, 50};
inline constexpr std::array<int32_t, 2> ITEM_GROUP_SKILLS_103_106{103, 106};
inline constexpr std::array<int32_t, 1> ITEM_GROUP_SKILLS_40{40};
inline constexpr std::array<int32_t, 2> ITEM_GROUP_SKILLS_41_48{41, 48};
inline constexpr std::array<int32_t, 2> ITEM_GROUP_SKILLS_42_49{42, 49};
inline constexpr std::array<int32_t, 1> ITEM_GROUP_SKILLS_54{54};

/** Java constructor data: slots (ItemSlot masks), itemSubType (NONE for the ArmorType constructors), armorType (null for the ItemSubType
 * constructors), requiredSkill (an empty array where Java passes `new int[] {}`) */
struct ItemGroupData {
	int64_t validEquipmentSlots;
	ItemSubType itemSubType;
	std::optional<ArmorType> armorType;
	std::span<const int32_t> requiredSkill;
};

inline constexpr std::array<ItemGroupData, 88> ITEM_GROUP_DATA{{
	{0, ItemSubType::NONE, std::nullopt, std::span<const int32_t>()}, // NONE (0)
	{3, ItemSubType::TWO_HAND, std::nullopt, std::span<const int32_t>()}, // NOWEAPON (MAIN_OR_SUB)
	{3, ItemSubType::ONE_HAND, std::nullopt, ITEM_GROUP_SKILLS_37_44}, // SWORD (MAIN_OR_SUB)
	{3, ItemSubType::TWO_HAND, std::nullopt, ITEM_GROUP_SKILLS_51}, // GREATSWORD (MAIN_OR_SUB)
	{0, ItemSubType::NONE, std::nullopt, std::span<const int32_t>()}, // EXTRACT_SWORD (0)
	{3, ItemSubType::ONE_HAND, std::nullopt, ITEM_GROUP_SKILLS_66_45}, // DAGGER (MAIN_OR_SUB)
	{3, ItemSubType::ONE_HAND, std::nullopt, ITEM_GROUP_SKILLS_39_46}, // MACE (MAIN_OR_SUB)
	{3, ItemSubType::TWO_HAND, std::nullopt, ITEM_GROUP_SKILLS_111}, // ORB (MAIN_OR_SUB)
	{3, ItemSubType::TWO_HAND, std::nullopt, ITEM_GROUP_SKILLS_100}, // SPELLBOOK (MAIN_OR_SUB)
	{3, ItemSubType::TWO_HAND, std::nullopt, ITEM_GROUP_SKILLS_52}, // POLEARM (MAIN_OR_SUB)
	{3, ItemSubType::TWO_HAND, std::nullopt, ITEM_GROUP_SKILLS_89}, // STAFF (MAIN_OR_SUB)
	{3, ItemSubType::TWO_HAND, std::nullopt, ITEM_GROUP_SKILLS_53}, // BOW (MAIN_OR_SUB)
	{3, ItemSubType::TWO_HAND, std::nullopt, ITEM_GROUP_SKILLS_124_114}, // HARP (MAIN_OR_SUB)
	{3, ItemSubType::ONE_HAND, std::nullopt, ITEM_GROUP_SKILLS_117_112}, // GUN (MAIN_OR_SUB)
	{3, ItemSubType::TWO_HAND, std::nullopt, ITEM_GROUP_SKILLS_113}, // CANNON (MAIN_OR_SUB)
	{3, ItemSubType::TWO_HAND, std::nullopt, ITEM_GROUP_SKILLS_112_115}, // KEYBLADE (MAIN_OR_SUB)
	{2, ItemSubType::SHIELD, std::nullopt, ITEM_GROUP_SKILLS_43_50}, // SHIELD (SUB_HAND)
	{8, ItemSubType::ALL_ARMOR, std::nullopt, ITEM_GROUP_SKILLS_103_106}, // TORSO (TORSO)
	{16, ItemSubType::ALL_ARMOR, std::nullopt, ITEM_GROUP_SKILLS_103_106}, // GLOVE (GLOVES)
	{2048, ItemSubType::ALL_ARMOR, std::nullopt, ITEM_GROUP_SKILLS_103_106}, // SHOULDER (SHOULDER)
	{4096, ItemSubType::ALL_ARMOR, std::nullopt, ITEM_GROUP_SKILLS_103_106}, // PANTS (PANTS)
	{32, ItemSubType::ALL_ARMOR, std::nullopt, ITEM_GROUP_SKILLS_103_106}, // SHOES (BOOTS)
	{8, ItemSubType::ROBE, std::nullopt, ITEM_GROUP_SKILLS_103_106}, // RB_TORSO (TORSO)
	{16, ItemSubType::ROBE, std::nullopt, ITEM_GROUP_SKILLS_103_106}, // RB_GLOVE (GLOVES)
	{2048, ItemSubType::ROBE, std::nullopt, ITEM_GROUP_SKILLS_103_106}, // RB_SHOULDER (SHOULDER)
	{4096, ItemSubType::ROBE, std::nullopt, ITEM_GROUP_SKILLS_103_106}, // RB_PANTS (PANTS)
	{32, ItemSubType::ROBE, std::nullopt, ITEM_GROUP_SKILLS_103_106}, // RB_SHOES (BOOTS)
	{8, ItemSubType::CLOTHES, std::nullopt, ITEM_GROUP_SKILLS_40}, // CL_TORSO (TORSO)
	{16, ItemSubType::CLOTHES, std::nullopt, ITEM_GROUP_SKILLS_40}, // CL_GLOVE (GLOVES)
	{2048, ItemSubType::CLOTHES, std::nullopt, ITEM_GROUP_SKILLS_40}, // CL_SHOULDER (SHOULDER)
	{4096, ItemSubType::CLOTHES, std::nullopt, ITEM_GROUP_SKILLS_40}, // CL_PANTS (PANTS)
	{32, ItemSubType::CLOTHES, std::nullopt, ITEM_GROUP_SKILLS_40}, // CL_SHOES (BOOTS)
	{8, ItemSubType::LEATHER, std::nullopt, ITEM_GROUP_SKILLS_41_48}, // LT_TORSO (TORSO)
	{16, ItemSubType::LEATHER, std::nullopt, ITEM_GROUP_SKILLS_41_48}, // LT_GLOVE (GLOVES)
	{2048, ItemSubType::LEATHER, std::nullopt, ITEM_GROUP_SKILLS_41_48}, // LT_SHOULDER (SHOULDER)
	{4096, ItemSubType::LEATHER, std::nullopt, ITEM_GROUP_SKILLS_41_48}, // LT_PANTS (PANTS)
	{32, ItemSubType::LEATHER, std::nullopt, ITEM_GROUP_SKILLS_41_48}, // LT_SHOES (BOOTS)
	{8, ItemSubType::CHAIN, std::nullopt, ITEM_GROUP_SKILLS_42_49}, // CH_TORSO (TORSO)
	{16, ItemSubType::CHAIN, std::nullopt, ITEM_GROUP_SKILLS_42_49}, // CH_GLOVE (GLOVES)
	{2048, ItemSubType::CHAIN, std::nullopt, ITEM_GROUP_SKILLS_42_49}, // CH_SHOULDER (SHOULDER)
	{4096, ItemSubType::CHAIN, std::nullopt, ITEM_GROUP_SKILLS_42_49}, // CH_PANTS (PANTS)
	{32, ItemSubType::CHAIN, std::nullopt, ITEM_GROUP_SKILLS_42_49}, // CH_SHOES (BOOTS)
	{8, ItemSubType::PLATE, std::nullopt, ITEM_GROUP_SKILLS_54}, // PL_TORSO (TORSO)
	{16, ItemSubType::PLATE, std::nullopt, ITEM_GROUP_SKILLS_54}, // PL_GLOVE (GLOVES)
	{2048, ItemSubType::PLATE, std::nullopt, ITEM_GROUP_SKILLS_54}, // PL_SHOULDER (SHOULDER)
	{4096, ItemSubType::PLATE, std::nullopt, ITEM_GROUP_SKILLS_54}, // PL_PANTS (PANTS)
	{32, ItemSubType::PLATE, std::nullopt, ITEM_GROUP_SKILLS_54}, // PL_SHOES (BOOTS)
	{192, ItemSubType::NONE, ArmorType::ACCESSORY, std::span<const int32_t>()}, // EARRING (EARRINGS_LEFT | EARRINGS_RIGHT)
	{768, ItemSubType::NONE, ArmorType::ACCESSORY, std::span<const int32_t>()}, // RING (RING_LEFT | RING_RIGHT)
	{1024, ItemSubType::NONE, ArmorType::ACCESSORY, std::span<const int32_t>()}, // NECKLACE (NECKLACE)
	{65536, ItemSubType::NONE, ArmorType::ACCESSORY, std::span<const int32_t>()}, // BELT (WAIST)
	{32768, ItemSubType::WING, std::nullopt, std::span<const int32_t>()}, // WING (WINGS)
	{524288, ItemSubType::PLUME, std::nullopt, std::span<const int32_t>()}, // PLUME (PLUME)
	{4, ItemSubType::NONE, ArmorType::ACCESSORY, std::span<const int32_t>()}, // HEAD (HELMET)
	{4, ItemSubType::LEATHER, std::nullopt, std::span<const int32_t>()}, // LT_HEADS (HELMET)
	{4, ItemSubType::CLOTHES, std::nullopt, std::span<const int32_t>()}, // CL_HEADS (HELMET)
	{4104, ItemSubType::CLOTHES, std::nullopt, std::span<const int32_t>()}, // CL_MULTISLOT (TORSO | PANTS)
	{2, ItemSubType::NONE, ArmorType::ACCESSORY, std::span<const int32_t>()}, // CL_SHIELD (SUB_HAND)
	{24576, ItemSubType::NONE, ArmorType::ACCESSORY, std::span<const int32_t>()}, // POWER_SHARDS (POWER_SHARD_RIGHT | POWER_SHARD_LEFT)
	{67645734912, ItemSubType::STIGMA, std::nullopt, std::span<const int32_t>()}, // STIGMA (ALL_STIGMA)
	{0, ItemSubType::ARROW, std::nullopt, std::span<const int32_t>()}, // ARROW (0)
	{1, ItemSubType::ONE_HAND, std::nullopt, std::span<const int32_t>()}, // NPC_MACE (MAIN_HAND)
	{3, ItemSubType::TWO_HAND, std::nullopt, std::span<const int32_t>()}, // TOOLRODS (MAIN_OR_SUB)
	{1, ItemSubType::ONE_HAND, std::nullopt, std::span<const int32_t>()}, // TOOLHOES (MAIN_HAND)
	{3, ItemSubType::TWO_HAND, std::nullopt, std::span<const int32_t>()}, // TOOLPICKS (MAIN_OR_SUB)
	{0, ItemSubType::NONE, std::nullopt, std::span<const int32_t>()}, // MANASTONE (0)
	{0, ItemSubType::NONE, std::nullopt, std::span<const int32_t>()}, // SPECIAL_MANASTONE (0)
	{0, ItemSubType::NONE, std::nullopt, std::span<const int32_t>()}, // RECIPE (0)
	{0, ItemSubType::NONE, std::nullopt, std::span<const int32_t>()}, // ENCHANTMENT (0)
	{0, ItemSubType::NONE, std::nullopt, std::span<const int32_t>()}, // PACK_SCROLL (0)
	{0, ItemSubType::NONE, std::nullopt, std::span<const int32_t>()}, // FLUX (0)
	{0, ItemSubType::NONE, std::nullopt, std::span<const int32_t>()}, // BALIC_EMOTION (0)
	{0, ItemSubType::NONE, std::nullopt, std::span<const int32_t>()}, // BALIC_MATERIAL (0)
	{0, ItemSubType::NONE, std::nullopt, std::span<const int32_t>()}, // RAWHIDE (0)
	{0, ItemSubType::NONE, std::nullopt, std::span<const int32_t>()}, // SOULSTONE (0)
	{0, ItemSubType::NONE, std::nullopt, std::span<const int32_t>()}, // GATHERABLE (0)
	{0, ItemSubType::NONE, std::nullopt, std::span<const int32_t>()}, // GATHERABLE_BONUS (0)
	{0, ItemSubType::NONE, std::nullopt, std::span<const int32_t>()}, // DROP_MATERIAL (0)
	{0, ItemSubType::NONE, std::nullopt, std::span<const int32_t>()}, // COINS (0)
	{0, ItemSubType::NONE, std::nullopt, std::span<const int32_t>()}, // MEDALS (0)
	{0, ItemSubType::NONE, std::nullopt, std::span<const int32_t>()}, // QUEST (0)
	{0, ItemSubType::NONE, std::nullopt, std::span<const int32_t>()}, // KEY (0)
	{0, ItemSubType::NONE, std::nullopt, std::span<const int32_t>()}, // CRAFT_BOOST (0)
	{0, ItemSubType::NONE, std::nullopt, std::span<const int32_t>()}, // TAMPERING (0)
	{0, ItemSubType::NONE, std::nullopt, std::span<const int32_t>()}, // COMBINATION (0)
	{0, ItemSubType::NONE, std::nullopt, std::span<const int32_t>()}, // SKILLBOOK (0)
	{0, ItemSubType::NONE, std::nullopt, std::span<const int32_t>()}, // GODSTONE (0)
	{0, ItemSubType::NONE, std::nullopt, std::span<const int32_t>()}, // STIGMA_SHARD (0)
}};
static_assert(static_cast<size_t>(ItemGroup::STIGMA_SHARD) + 1 == ITEM_GROUP_DATA.size(), "one entry per ItemGroup constant");
} // namespace detail

constexpr int64_t getValidEquipmentSlots(ItemGroup group) noexcept {
	return detail::ITEM_GROUP_DATA[static_cast<size_t>(group)].validEquipmentSlots;
}

constexpr ItemSubType getItemSubType(ItemGroup group) noexcept {
	return detail::ITEM_GROUP_DATA[static_cast<size_t>(group)].itemSubType;
}

/** @return the armor type, std::nullopt (Java null) for the groups created with an ItemSubType */
constexpr std::optional<ArmorType> getArmorType(ItemGroup group) noexcept {
	return detail::ITEM_GROUP_DATA[static_cast<size_t>(group)].armorType;
}

/** @return the required skill ids (never null in Java: groups without skills have an empty array) */
constexpr std::span<const int32_t> getRequiredSkills(ItemGroup group) noexcept {
	return detail::ITEM_GROUP_DATA[static_cast<size_t>(group)].requiredSkill;
}

constexpr EquipType getEquipType(ItemGroup group) noexcept {
	if (getArmorType(group).has_value())
		return EquipType::ARMOR;
	else
		return getEquipType(getItemSubType(group));
}

} // namespace aion::gameserver::model::templates::item::enums
