#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "aion/gameserver/model/items/ItemSlot.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/stats/container/PlumStatEnum.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::network::detail {

/**
 * C++ only, private to P4-15 (the iteminfo writers): constructor data and static methods of Java enums whose companion headers do not exist in
 * the tree yet (ItemSlot: P4-13, ItemGroup: P4-07a, PlumStatEnum and StatEnum: P5-01). Pure data copied from the Java enums. TODO: replace the uses by the
 * companions once they land and delete this header.
 */

/** Java ItemSlot constructor arguments (slotIdMask, combo) in ordinal order */
struct ItemSlotData {
	int64_t slotIdMask;
	bool combo;
};

inline constexpr std::array<ItemSlotData, 37> ITEM_SLOT_DATA{{
	{1LL, false},       // MAIN_HAND
	{1LL << 1, false},  // SUB_HAND
	{1LL << 2, false},  // HELMET
	{1LL << 3, false},  // TORSO
	{1LL << 4, false},  // GLOVES
	{1LL << 5, false},  // BOOTS
	{1LL << 6, false},  // EARRINGS_LEFT
	{1LL << 7, false},  // EARRINGS_RIGHT
	{1LL << 8, false},  // RING_LEFT
	{1LL << 9, false},  // RING_RIGHT
	{1LL << 10, false}, // NECKLACE
	{1LL << 11, false}, // SHOULDER
	{1LL << 12, false}, // PANTS
	{1LL << 13, false}, // POWER_SHARD_RIGHT
	{1LL << 14, false}, // POWER_SHARD_LEFT
	{1LL << 15, false}, // WINGS
	{1LL << 16, false}, // WAIST
	{1LL << 17, false}, // MAIN_OFF_HAND
	{1LL << 18, false}, // SUB_OFF_HAND
	{1LL << 19, false}, // PLUME
	{(1LL) | (1LL << 1), true},                // MAIN_OR_SUB
	{(1LL << 17) | (1LL << 18), true},         // MAIN_OFF_OR_SUB_OFF
	{(1LL << 6) | (1LL << 7), true},           // EARRING_RIGHT_OR_LEFT
	{(1LL << 8) | (1LL << 9), true},           // RING_RIGHT_OR_LEFT
	{(1LL << 14) | (1LL << 13), true},         // SHARD_RIGHT_OR_LEFT
	{(1LL) | (1LL << 17), true},               // RIGHT_HAND
	{(1LL << 1) | (1LL << 18), true},          // LEFT_HAND
	{1LL | (1LL << 1) | (1LL << 2) | (1LL << 3) | (1LL << 4) | (1LL << 5) | (1LL << 6) | (1LL << 7) | (1LL << 10) | (1LL << 11) | (1LL << 12) |
			(1LL << 13) | (1LL << 14) | (1LL << 15) | (1LL << 19),
		true},                                   // VISIBLE
	{1LL << 30, false},                        // STIGMA1
	{1LL << 31, false},                        // STIGMA2
	{1LL << 32, false},                        // STIGMA3
	{(1LL << 30) | (1LL << 31) | (1LL << 32), true}, // REGULAR_STIGMAS
	{1LL << 33, false},                        // ADV_STIGMA1
	{1LL << 34, false},                        // ADV_STIGMA2
	{1LL << 35, false},                        // ADV_STIGMA3
	{(1LL << 33) | (1LL << 34) | (1LL << 35), true}, // ADVANCED_STIGMAS
	{(1LL << 30) | (1LL << 31) | (1LL << 32) | (1LL << 33) | (1LL << 34) | (1LL << 35), true}, // ALL_STIGMA
}};

/** Java: ItemSlot.getSlotIdMask() */
constexpr int64_t slotIdMaskOf(model::items::ItemSlot slot) noexcept {
	return ITEM_SLOT_DATA[static_cast<size_t>(slot)].slotIdMask;
}

/** Java: ItemSlot.getSlotsFor(slotIdMask) - the non-combo slots contained in the mask, in ordinal order */
inline std::vector<model::items::ItemSlot> slotsFor(int64_t slotIdMask) {
	if (slotIdMask == 0)
		throw runtime::IllegalArgumentException("slotIdMask cannot be 0");
	std::vector<model::items::ItemSlot> slots;
	for (size_t i = 0; i < ITEM_SLOT_DATA.size(); i++) {
		if (!ITEM_SLOT_DATA[i].combo && (slotIdMask & ITEM_SLOT_DATA[i].slotIdMask) == ITEM_SLOT_DATA[i].slotIdMask)
			slots.push_back(static_cast<model::items::ItemSlot>(i));
	}
	return slots;
}

/** Java: ItemSlot.getSlotFor(slot) - getSlotsFor(slot)[0] (ArrayIndexOutOfBoundsException if no single slot matches) */
inline model::items::ItemSlot slotFor(int64_t slot) {
	std::vector<model::items::ItemSlot> slots = slotsFor(slot);
	if (slots.empty())
		throw runtime::ArrayIndexOutOfBoundsException("Index 0 out of bounds for length 0");
	return slots[0];
}

/** Java: itemGroup.getArmorType() == ArmorType.ACCESSORY (the groups created with ArmorType.ACCESSORY in ItemGroup.java) */
constexpr bool isAccessoryArmorGroup(model::templates::item::enums::ItemGroup group) noexcept {
	using enum model::templates::item::enums::ItemGroup;
	return group == EARRING || group == RING || group == NECKLACE || group == BELT || group == HEAD || group == CL_SHIELD || group == POWER_SHARDS;
}

/** Java: PlumStatEnum.getId() */
constexpr int32_t plumStatIdOf(model::stats::container::PlumStatEnum stat) noexcept {
	static constexpr std::array<int32_t, 4> IDS{42, 35, 30, 40};
	return IDS[static_cast<size_t>(stat)];
}

/** Java: PlumStatEnum.getBoostValue() */
constexpr int32_t plumStatBoostValueOf(model::stats::container::PlumStatEnum stat) noexcept {
	static constexpr std::array<int32_t, 4> BOOST_VALUES{150, 20, 4, 0};
	return BOOST_VALUES[static_cast<size_t>(stat)];
}

/** Java StatEnum constructor arguments (itemStoneMask, sign) in ordinal order (the no-argument constructor is (0, 1)). TODO(P5-01): StatEnum companion */
struct StatEnumData {
	int32_t itemStoneMask;
	int32_t sign;
};

inline constexpr std::array<StatEnumData, 177> STAT_ENUM_DATA{{
	{22, 1}, // MAXDP
	{18, 1}, // MAXHP
	{20, 1}, // MAXMP
	{9, 1}, // AGILITY
	{33, 1}, // BLOCK
	{31, 1}, // EVASION
	{41, 1}, // CONCENTRATION
	{11, 1}, // WILL
	{7, 1}, // HEALTH
	{8, 1}, // ACCURACY
	{10, 1}, // KNOWLEDGE
	{32, 1}, // PARRY
	{6, 1}, // POWER
	{36, 1}, // SPEED
	{0, 1}, // ALLSPEED
	{39, 1}, // WEIGHT
	{35, 1}, // HIT_COUNT
	{38, 1}, // ATTACK_RANGE
	{29, -1}, // ATTACK_SPEED
	{25, 1}, // PHYSICAL_ATTACK
	{30, 1}, // PHYSICAL_ACCURACY
	{34, 1}, // PHYSICAL_CRITICAL
	{26, 1}, // PHYSICAL_DEFENSE
	{0, 1}, // MAIN_HAND_HITS
	{0, 1}, // MAIN_HAND_ACCURACY
	{0, 1}, // MAIN_HAND_CRITICAL
	{0, 1}, // MAIN_HAND_POWER
	{0, 1}, // MAIN_HAND_ATTACK_SPEED
	{0, 1}, // OFF_HAND_HITS
	{0, 1}, // OFF_HAND_ACCURACY
	{0, 1}, // OFF_HAND_CRITICAL
	{0, 1}, // OFF_HAND_POWER
	{0, 1}, // OFF_HAND_ATTACK_SPEED
	{27, 1}, // MAGICAL_ATTACK
	{105, 1}, // MAGICAL_ACCURACY
	{40, 1}, // MAGICAL_CRITICAL
	{28, 1}, // MAGICAL_RESIST
	{0, 1}, // MAX_DAMAGES
	{0, 1}, // MIN_DAMAGES
	{14, 1}, // EARTH_RESISTANCE
	{15, 1}, // FIRE_RESISTANCE
	{13, 1}, // WIND_RESISTANCE
	{12, 1}, // WATER_RESISTANCE
	{17, 1}, // DARK_RESISTANCE
	{16, 1}, // LIGHT_RESISTANCE
	{104, 1}, // BOOST_MAGICAL_SKILL
	{0, 1}, // BOOST_SPELL_ATTACK
	{108, 1}, // BOOST_CASTING_TIME
	{0, 1}, // BOOST_CASTING_TIME_HEAL
	{0, 1}, // BOOST_CASTING_TIME_TRAP
	{0, 1}, // BOOST_CASTING_TIME_ATTACK
	{0, 1}, // BOOST_CASTING_TIME_SKILL
	{0, 1}, // BOOST_CASTING_TIME_SUMMONHOMING
	{0, 1}, // BOOST_CASTING_TIME_SUMMON
	{109, 1}, // BOOST_HATE
	{23, 1}, // FLY_TIME
	{37, 1}, // FLY_SPEED
	{0, 1}, // DAMAGE_REDUCE
	{0, 1}, // DAMAGE_REDUCE_MAX
	{44, 1}, // BLEED_RESISTANCE
	{48, 1}, // BLIND_RESISTANCE
	{63, 1}, // BIND_RESISTANCE
	{49, 1}, // CHARM_RESISTANCE
	{54, 1}, // CONFUSE_RESISTANCE
	{53, 1}, // CURSE_RESISTANCE
	{50, 1}, // DISEASE_RESISTANCE
	{64, 1}, // DEFORM_RESISTANCE
	{52, 1}, // FEAR_RESISTANCE
	{66, 1}, // NOFLY_RESISTANCE
	{59, 1}, // OPENAERIAL_RESISTANCE
	{45, 1}, // PARALYZE_RESISTANCE
	{56, 1}, // PERIFICATION_RESISTANCE
	{43, 1}, // POISON_RESISTANCE
	{65, 1}, // PULLED_RESISTANCE
	{47, 1}, // ROOT_RESISTANCE
	{51, 1}, // SILENCE_RESISTANCE
	{46, 1}, // SLEEP_RESISTANCE
	{61, 1}, // SLOW_RESISTANCE
	{60, 1}, // SNARE_RESISTANCE
	{62, 1}, // SPIN_RESISTANCE
	{58, 1}, // STAGGER_RESISTANCE
	{57, 1}, // STUMBLE_RESISTANCE
	{55, 1}, // STUN_RESISTANCE
	{70, 1}, // BLEED_RESISTANCE_PENETRATION
	{74, 1}, // BLIND_RESISTANCE_PENETRATION
	{89, 1}, // BIND_RESISTANCE_PENETRATION
	{75, 1}, // CHARM_RESISTANCE_PENETRATION
	{80, 1}, // CONFUSE_RESISTANCE_PENETRATION
	{79, 1}, // CURSE_RESISTANCE_PENETRATION
	{76, 1}, // DISEASE_RESISTANCE_PENETRATION
	{90, 1}, // DEFORM_RESISTANCE_PENETRATION
	{78, 1}, // FEAR_RESISTANCE_PENETRATION
	{92, 1}, // NOFLY_RESISTANCE_PENETRATION
	{85, 1}, // OPENAERIAL_RESISTANCE_PENETRATION
	{71, 1}, // PARALYZE_RESISTANCE_PENETRATION
	{82, 1}, // PERIFICATION_RESISTANCE_PENETRATION
	{69, 1}, // POISON_RESISTANCE_PENETRATION
	{91, 1}, // PULLED_RESISTANCE_PENETRATION
	{73, 1}, // ROOT_RESISTANCE_PENETRATION
	{77, 1}, // SILENCE_RESISTANCE_PENETRATION
	{72, 1}, // SLEEP_RESISTANCE_PENETRATION
	{87, 1}, // SLOW_RESISTANCE_PENETRATION
	{86, 1}, // SNARE_RESISTANCE_PENETRATION
	{88, 1}, // SPIN_RESISTANCE_PENETRATION
	{84, 1}, // STAGGER_RESISTANCE_PENETRATION
	{83, 1}, // STUMBLE_RESISTANCE_PENETRATION
	{81, 1}, // STUN_RESISTANCE_PENETRATION
	{21, 1}, // REGEN_MP
	{19, 1}, // REGEN_HP
	{24, 1}, // REGEN_FP
	{110, 1}, // HEAL_BOOST
	{2, 1}, // ALLRESIST
	{0, 1}, // STUNLIKE_RESISTANCE
	{0, 1}, // ELEMENTAL_RESISTANCE_DARK
	{0, 1}, // ELEMENTAL_RESISTANCE_LIGHT
	{116, 1}, // MAGICAL_CRITICAL_RESIST
	{118, 1}, // MAGICAL_CRITICAL_DAMAGE_REDUCE
	{115, 1}, // PHYSICAL_CRITICAL_RESIST
	{117, 1}, // PHYSICAL_CRITICAL_DAMAGE_REDUCE
	{0, 1}, // ERFIRE
	{0, 1}, // ERAIR
	{0, 1}, // EREARTH
	{0, 1}, // ERWATER
	{1, 1}, // ABNORMAL_RESISTANCE_ALL
	{0, 1}, // ALLPARA
	{4, 1}, // KNOWIL
	{5, 1}, // AGIDEX
	{3, 1}, // STRVIT
	{125, 1}, // MAGICAL_DEFEND
	{126, 1}, // MAGIC_SKILL_BOOST_RESIST
	{0, 1}, // HEAL_SKILL_BOOST
	{0, 1}, // HEAL_SKILL_DEBOOST
	{0, 1}, // BOOST_HUNTING_XP_RATE
	{0, 1}, // BOOST_GROUP_HUNTING_XP_RATE
	{0, 1}, // BOOST_QUEST_XP_RATE
	{0, 1}, // BOOST_CRAFTING_XP_RATE
	{0, 1}, // BOOST_COOKING_XP_RATE
	{0, 1}, // BOOST_WEAPONSMITHING_XP_RATE
	{0, 1}, // BOOST_ARMORSMITHING_XP_RATE
	{0, 1}, // BOOST_TAILORING_XP_RATE
	{0, 1}, // BOOST_ALCHEMY_XP_RATE
	{0, 1}, // BOOST_HANDICRAFTING_XP_RATE
	{0, 1}, // BOOST_MENUISIER_XP_RATE
	{0, 1}, // BOOST_GATHERING_XP_RATE
	{0, 1}, // BOOST_AETHERTAPPING_XP_RATE
	{0, 1}, // BOOST_ESSENCETAPPING_XP_RATE
	{0, 1}, // BOOST_DROP_RATE
	{0, 1}, // BOOST_MANTRA_RANGE
	{0, 1}, // BOOST_RESIST_DEBUFF
	{0, 1}, // ELEMENTAL_FIRE
	{111, 1}, // PVP_PHYSICAL_ATTACK
	{112, 1}, // PVP_PHYSICAL_DEFEND
	{113, 1}, // PVP_MAGICAL_ATTACK
	{114, 1}, // PVP_MAGICAL_DEFEND
	{106, 1}, // PVP_ATTACK_RATIO
	{0, 1}, // PVP_ATTACK_RATIO_MAGICAL
	{0, 1}, // PVP_ATTACK_RATIO_PHYSICAL
	{107, 1}, // PVP_DEFEND_RATIO
	{0, 1}, // PVP_DEFEND_RATIO_PHYSICAL
	{0, 1}, // PVP_DEFEND_RATIO_MAGICAL
	{0, 1}, // PVE_ATTACK_RATIO
	{0, 1}, // PVE_ATTACK_RATIO_MAGICAL
	{0, 1}, // PVE_ATTACK_RATIO_PHYSICAL
	{0, 1}, // PVE_DEFEND_RATIO
	{0, 1}, // PVE_DEFEND_RATIO_PHYSICAL
	{0, 1}, // PVE_DEFEND_RATIO_MAGICAL
	{0, 1}, // AP_BOOST
	{0, 1}, // DR_BOOST
	{0, 1}, // PROC_REDUCE_RATE
	{0, 1}, // BOOST_CHARGE_TIME
	{0, 1}, // PVP_DODGE
	{0, 1}, // PVP_BLOCK
	{0, 1}, // PVP_PARRY
	{0, 1}, // PVP_HIT_ACCURACY
	{0, 1}, // PVP_MAGICAL_RESIST
	{0, 1}, // PVP_MAGICAL_HIT_ACCURACY
	{0, 1}, // BLOCK_PENETRATION
}};
static_assert(STAT_ENUM_DATA.size() == xml::EnumTraits<model::stats::container::StatEnum>::names.size());

/** Java: StatEnum.getItemStoneMask() */
constexpr int32_t itemStoneMaskOf(model::stats::container::StatEnum stat) noexcept {
	return STAT_ENUM_DATA[static_cast<size_t>(stat)].itemStoneMask;
}

/** Java: StatEnum.getSign() */
constexpr int32_t signOf(model::stats::container::StatEnum stat) noexcept {
	return STAT_ENUM_DATA[static_cast<size_t>(stat)].sign;
}

} // namespace aion::gameserver::network::detail
