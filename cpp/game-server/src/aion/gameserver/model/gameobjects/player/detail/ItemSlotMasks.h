#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "aion/gameserver/model/items/ItemSlot.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::gameobjects::player::detail {

/**
 * C++ only, private to P4-12 (Equipment, PlayerAccountData): the constructor data and static methods of the Java enum
 * com.aionemu.gameserver.model.items.ItemSlot. The ItemSlot companion header belongs to P4-13 (wave 3a-2) and does not exist yet; replace the
 * uses by that companion once it lands and delete this header.
 */

/** Java ItemSlot constructor arguments (slotIdMask, combo) in ordinal order */
struct ItemSlotData {
	int64_t slotIdMask;
	bool combo;
};

inline constexpr int64_t MAIN_HAND = 1LL;
inline constexpr int64_t SUB_HAND = 1LL << 1;
inline constexpr int64_t HELMET = 1LL << 2;
inline constexpr int64_t TORSO = 1LL << 3;
inline constexpr int64_t GLOVES = 1LL << 4;
inline constexpr int64_t BOOTS = 1LL << 5;
inline constexpr int64_t EARRINGS_LEFT = 1LL << 6;
inline constexpr int64_t EARRINGS_RIGHT = 1LL << 7;
inline constexpr int64_t RING_LEFT = 1LL << 8;
inline constexpr int64_t RING_RIGHT = 1LL << 9;
inline constexpr int64_t NECKLACE = 1LL << 10;
inline constexpr int64_t SHOULDER = 1LL << 11;
inline constexpr int64_t PANTS = 1LL << 12;
inline constexpr int64_t POWER_SHARD_RIGHT = 1LL << 13;
inline constexpr int64_t POWER_SHARD_LEFT = 1LL << 14;
inline constexpr int64_t WINGS = 1LL << 15;
inline constexpr int64_t WAIST = 1LL << 16;
inline constexpr int64_t MAIN_OFF_HAND = 1LL << 17;
inline constexpr int64_t SUB_OFF_HAND = 1LL << 18;
inline constexpr int64_t PLUME = 1LL << 19;
inline constexpr int64_t MAIN_OR_SUB = MAIN_HAND | SUB_HAND;
inline constexpr int64_t MAIN_OFF_OR_SUB_OFF = MAIN_OFF_HAND | SUB_OFF_HAND;
inline constexpr int64_t EARRING_RIGHT_OR_LEFT = EARRINGS_LEFT | EARRINGS_RIGHT;
inline constexpr int64_t RING_RIGHT_OR_LEFT = RING_LEFT | RING_RIGHT;
inline constexpr int64_t SHARD_RIGHT_OR_LEFT = POWER_SHARD_LEFT | POWER_SHARD_RIGHT;
inline constexpr int64_t RIGHT_HAND = MAIN_HAND | MAIN_OFF_HAND;
inline constexpr int64_t LEFT_HAND = SUB_HAND | SUB_OFF_HAND;
// rings were designed to be visible (at the players thumbs), but they have no skins
inline constexpr int64_t VISIBLE = MAIN_HAND | SUB_HAND | HELMET | TORSO | GLOVES | BOOTS | EARRINGS_LEFT | EARRINGS_RIGHT | NECKLACE | SHOULDER | PANTS
	| POWER_SHARD_RIGHT | POWER_SHARD_LEFT | WINGS | PLUME;
inline constexpr int64_t STIGMA1 = 1LL << 30;
inline constexpr int64_t STIGMA2 = 1LL << 31;
inline constexpr int64_t STIGMA3 = 1LL << 32;
inline constexpr int64_t REGULAR_STIGMAS = STIGMA1 | STIGMA2 | STIGMA3;
inline constexpr int64_t ADV_STIGMA1 = 1LL << 33;
inline constexpr int64_t ADV_STIGMA2 = 1LL << 34;
inline constexpr int64_t ADV_STIGMA3 = 1LL << 35;
inline constexpr int64_t ADVANCED_STIGMAS = ADV_STIGMA1 | ADV_STIGMA2 | ADV_STIGMA3;
inline constexpr int64_t ALL_STIGMA = REGULAR_STIGMAS | ADVANCED_STIGMAS;

inline constexpr std::array<ItemSlotData, 37> ITEM_SLOT_DATA{{
	{MAIN_HAND, false},
	{SUB_HAND, false},
	{HELMET, false},
	{TORSO, false},
	{GLOVES, false},
	{BOOTS, false},
	{EARRINGS_LEFT, false},
	{EARRINGS_RIGHT, false},
	{RING_LEFT, false},
	{RING_RIGHT, false},
	{NECKLACE, false},
	{SHOULDER, false},
	{PANTS, false},
	{POWER_SHARD_RIGHT, false},
	{POWER_SHARD_LEFT, false},
	{WINGS, false},
	{WAIST, false},
	{MAIN_OFF_HAND, false},
	{SUB_OFF_HAND, false},
	{PLUME, false},
	{MAIN_OR_SUB, true},
	{MAIN_OFF_OR_SUB_OFF, true},
	{EARRING_RIGHT_OR_LEFT, true},
	{RING_RIGHT_OR_LEFT, true},
	{SHARD_RIGHT_OR_LEFT, true},
	{RIGHT_HAND, true},
	{LEFT_HAND, true},
	{VISIBLE, true},
	{STIGMA1, false},
	{STIGMA2, false},
	{STIGMA3, false},
	{REGULAR_STIGMAS, true},
	{ADV_STIGMA1, false},
	{ADV_STIGMA2, false},
	{ADV_STIGMA3, false},
	{ADVANCED_STIGMAS, true},
	{ALL_STIGMA, true},
}};
static_assert(static_cast<size_t>(items::ItemSlot::ALL_STIGMA) + 1 == ITEM_SLOT_DATA.size(), "one entry per ItemSlot constant");

/** Java itemSlot.getSlotIdMask() */
constexpr int64_t getSlotIdMask(items::ItemSlot slot) noexcept {
	return ITEM_SLOT_DATA[static_cast<size_t>(slot)].slotIdMask;
}

/** Java ItemSlot.isAdvancedStigma(slot) */
constexpr bool isAdvancedStigma(int64_t slot) noexcept {
	return (ADVANCED_STIGMAS & slot) == slot;
}

/** Java ItemSlot.isRegularStigma(slot) */
constexpr bool isRegularStigma(int64_t slot) noexcept {
	return (REGULAR_STIGMAS & slot) == slot;
}

/** Java ItemSlot.isStigma(slot) */
constexpr bool isStigma(int64_t slot) noexcept {
	return (ALL_STIGMA & slot) == slot;
}

/** Java ItemSlot.isVisible(slot) */
constexpr bool isVisible(int64_t slot) noexcept {
	return (VISIBLE & slot) == slot;
}

/** Java ItemSlot.isTwoHandedWeapon(slot) */
constexpr bool isTwoHandedWeapon(int64_t slot) noexcept {
	return (slot & MAIN_OR_SUB) == MAIN_OR_SUB || (slot & MAIN_OFF_OR_SUB_OFF) == MAIN_OFF_OR_SUB_OFF;
}

/** Java ItemSlot.getEquipmentSlotType(slot) */
constexpr int8_t getEquipmentSlotType(int64_t slot) noexcept {
	if (!isVisible(slot))
		return 0; // not equippable
	const int64_t leftSlotMask = SUB_HAND | EARRINGS_LEFT | RING_LEFT | POWER_SHARD_LEFT | SUB_OFF_HAND;
	if ((slot & leftSlotMask) == 0 || isTwoHandedWeapon(slot))
		return 1; // default (right-hand) slot
	return 2; // secondary (left-hand) slot
}

/** Java ItemSlot.getSlotsFor(slotIdMask): the non-combo slots contained in the mask, in ordinal order. @throws IllegalArgumentException for 0 */
inline std::vector<items::ItemSlot> getSlotsFor(int64_t slotIdMask) {
	if (slotIdMask == 0)
		throw runtime::IllegalArgumentException("slotIdMask cannot be 0");
	std::vector<items::ItemSlot> slots;
	for (size_t i = 0; i < ITEM_SLOT_DATA.size(); ++i) {
		const ItemSlotData& data = ITEM_SLOT_DATA[i];
		if (!data.combo && (slotIdMask & data.slotIdMask) == data.slotIdMask)
			slots.push_back(static_cast<items::ItemSlot>(i));
	}
	return slots;
}

} // namespace aion::gameserver::model::gameobjects::player::detail
