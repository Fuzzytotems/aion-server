#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "aion/gameserver/model/items/ItemSlot.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::items {

/**
 * Companion of the generated enum ItemSlot (docs/design/static-data.md §2.5): the Java constructor data and methods as free functions found by
 * ADL (`getSlotIdMask(slot)` for Java `slot.getSlotIdMask()`), the static methods as free functions of the same name. Pure data.
 * <p>
 * This enum is defining inventory slots, to which items can be equipped.
 *
 * @author Luno, xTz
 */

namespace detail {
/** Java constructor arguments (slotIdMask, combo) in ordinal order */
struct ItemSlotData {
	int64_t slotIdMask;
	bool combo;
};

inline constexpr int64_t SLOT_MAIN_HAND = 1LL;
inline constexpr int64_t SLOT_SUB_HAND = 1LL << 1;
inline constexpr int64_t SLOT_HELMET = 1LL << 2;
inline constexpr int64_t SLOT_TORSO = 1LL << 3;
inline constexpr int64_t SLOT_GLOVES = 1LL << 4;
inline constexpr int64_t SLOT_BOOTS = 1LL << 5;
inline constexpr int64_t SLOT_EARRINGS_LEFT = 1LL << 6;
inline constexpr int64_t SLOT_EARRINGS_RIGHT = 1LL << 7;
inline constexpr int64_t SLOT_RING_LEFT = 1LL << 8;
inline constexpr int64_t SLOT_RING_RIGHT = 1LL << 9;
inline constexpr int64_t SLOT_NECKLACE = 1LL << 10;
inline constexpr int64_t SLOT_SHOULDER = 1LL << 11;
inline constexpr int64_t SLOT_PANTS = 1LL << 12;
inline constexpr int64_t SLOT_POWER_SHARD_RIGHT = 1LL << 13;
inline constexpr int64_t SLOT_POWER_SHARD_LEFT = 1LL << 14;
inline constexpr int64_t SLOT_WINGS = 1LL << 15;
inline constexpr int64_t SLOT_WAIST = 1LL << 16;
inline constexpr int64_t SLOT_MAIN_OFF_HAND = 1LL << 17;
inline constexpr int64_t SLOT_SUB_OFF_HAND = 1LL << 18;
inline constexpr int64_t SLOT_PLUME = 1LL << 19;
inline constexpr int64_t SLOT_STIGMA1 = 1LL << 30;
inline constexpr int64_t SLOT_STIGMA2 = 1LL << 31;
inline constexpr int64_t SLOT_STIGMA3 = 1LL << 32;
inline constexpr int64_t SLOT_ADV_STIGMA1 = 1LL << 33;
inline constexpr int64_t SLOT_ADV_STIGMA2 = 1LL << 34;
inline constexpr int64_t SLOT_ADV_STIGMA3 = 1LL << 35;
inline constexpr int64_t SLOT_REGULAR_STIGMAS = SLOT_STIGMA1 | SLOT_STIGMA2 | SLOT_STIGMA3;
inline constexpr int64_t SLOT_ADVANCED_STIGMAS = SLOT_ADV_STIGMA1 | SLOT_ADV_STIGMA2 | SLOT_ADV_STIGMA3;

inline constexpr std::array<ItemSlotData, 37> ITEM_SLOT_DATA{{
	{SLOT_MAIN_HAND, false},
	{SLOT_SUB_HAND, false},
	{SLOT_HELMET, false},
	{SLOT_TORSO, false},
	{SLOT_GLOVES, false},
	{SLOT_BOOTS, false},
	{SLOT_EARRINGS_LEFT, false},
	{SLOT_EARRINGS_RIGHT, false},
	{SLOT_RING_LEFT, false},
	{SLOT_RING_RIGHT, false},
	{SLOT_NECKLACE, false},
	{SLOT_SHOULDER, false},
	{SLOT_PANTS, false},
	{SLOT_POWER_SHARD_RIGHT, false},
	{SLOT_POWER_SHARD_LEFT, false},
	{SLOT_WINGS, false},
	{SLOT_WAIST, false},
	{SLOT_MAIN_OFF_HAND, false},
	{SLOT_SUB_OFF_HAND, false},
	{SLOT_PLUME, false},
	// combo
	{SLOT_MAIN_HAND | SLOT_SUB_HAND, true},                     // MAIN_OR_SUB 3
	{SLOT_MAIN_OFF_HAND | SLOT_SUB_OFF_HAND, true},             // MAIN_OFF_OR_SUB_OFF
	{SLOT_EARRINGS_LEFT | SLOT_EARRINGS_RIGHT, true},           // EARRING_RIGHT_OR_LEFT 192
	{SLOT_RING_LEFT | SLOT_RING_RIGHT, true},                   // RING_RIGHT_OR_LEFT 768
	{SLOT_POWER_SHARD_LEFT | SLOT_POWER_SHARD_RIGHT, true},     // SHARD_RIGHT_OR_LEFT 24576
	{SLOT_MAIN_HAND | SLOT_MAIN_OFF_HAND, true},                // RIGHT_HAND
	{SLOT_SUB_HAND | SLOT_SUB_OFF_HAND, true},                  // LEFT_HAND
	{SLOT_MAIN_HAND | SLOT_SUB_HAND | SLOT_HELMET | SLOT_TORSO | SLOT_GLOVES | SLOT_BOOTS | SLOT_EARRINGS_LEFT | SLOT_EARRINGS_RIGHT | SLOT_NECKLACE
			| SLOT_SHOULDER | SLOT_PANTS | SLOT_POWER_SHARD_RIGHT | SLOT_POWER_SHARD_LEFT | SLOT_WINGS | SLOT_PLUME,
		true}, // VISIBLE: rings were designed to be visible (at the players thumbs), but they have no skins
	// STIGMA slots
	{SLOT_STIGMA1, false},
	{SLOT_STIGMA2, false},
	{SLOT_STIGMA3, false},
	{SLOT_REGULAR_STIGMAS, true}, // REGULAR_STIGMAS
	{SLOT_ADV_STIGMA1, false},
	{SLOT_ADV_STIGMA2, false},
	{SLOT_ADV_STIGMA3, false},
	{SLOT_ADVANCED_STIGMAS, true},                         // ADVANCED_STIGMAS
	{SLOT_REGULAR_STIGMAS | SLOT_ADVANCED_STIGMAS, true}, // ALL_STIGMA
}};
static_assert(static_cast<size_t>(ItemSlot::ALL_STIGMA) + 1 == ITEM_SLOT_DATA.size(), "one entry per ItemSlot constant");

constexpr const ItemSlotData& itemSlotData(ItemSlot slot) noexcept {
	return ITEM_SLOT_DATA[static_cast<size_t>(slot)];
}
} // namespace detail

constexpr int64_t getSlotIdMask(ItemSlot slot) noexcept {
	return detail::itemSlotData(slot).slotIdMask;
}

/** @return the combo */
constexpr bool isCombo(ItemSlot slot) noexcept {
	return detail::itemSlotData(slot).combo;
}

/** Java ItemSlot.isAdvancedStigma(slot) */
constexpr bool isAdvancedStigma(int64_t slot) noexcept {
	return (getSlotIdMask(ItemSlot::ADVANCED_STIGMAS) & slot) == slot;
}

/** Java ItemSlot.isRegularStigma(slot) */
constexpr bool isRegularStigma(int64_t slot) noexcept {
	return (getSlotIdMask(ItemSlot::REGULAR_STIGMAS) & slot) == slot;
}

/** Java ItemSlot.isStigma(slot) */
constexpr bool isStigma(int64_t slot) noexcept {
	return (getSlotIdMask(ItemSlot::ALL_STIGMA) & slot) == slot;
}

/** Java ItemSlot.isVisible(slot) */
constexpr bool isVisible(int64_t slot) noexcept {
	return (getSlotIdMask(ItemSlot::VISIBLE) & slot) == slot;
}

/** Java ItemSlot.isTwoHandedWeapon(slot) */
constexpr bool isTwoHandedWeapon(int64_t slot) noexcept {
	return (slot & getSlotIdMask(ItemSlot::MAIN_OR_SUB)) == getSlotIdMask(ItemSlot::MAIN_OR_SUB)
		|| (slot & getSlotIdMask(ItemSlot::MAIN_OFF_OR_SUB_OFF)) == getSlotIdMask(ItemSlot::MAIN_OFF_OR_SUB_OFF);
}

/** Java ItemSlot.getEquipmentSlotType(slot): 0 not equippable, 1 default (right-hand) slot, 2 secondary (left-hand) slot */
constexpr int8_t getEquipmentSlotType(int64_t slot) noexcept {
	if (!isVisible(slot))
		return 0; // not equippable

	const int64_t leftSlotMask = getSlotIdMask(ItemSlot::SUB_HAND) | getSlotIdMask(ItemSlot::EARRINGS_LEFT) | getSlotIdMask(ItemSlot::RING_LEFT)
		| getSlotIdMask(ItemSlot::POWER_SHARD_LEFT) | getSlotIdMask(ItemSlot::SUB_OFF_HAND);
	if ((slot & leftSlotMask) == 0 || isTwoHandedWeapon(slot))
		return 1; // default (right-hand) slot

	return 2; // secondary (left-hand) slot
}

/**
 * Java ItemSlot.getSlotsFor(slotIdMask): the non-combo slots contained in the mask, in ordinal order.
 *
 * @throws IllegalArgumentException if the mask is 0
 */
inline std::vector<ItemSlot> getSlotsFor(int64_t slotIdMask) {
	if (slotIdMask == 0)
		throw runtime::IllegalArgumentException("slotIdMask cannot be 0");
	std::vector<ItemSlot> slots;
	for (size_t i = 0; i < detail::ITEM_SLOT_DATA.size(); ++i) {
		const detail::ItemSlotData& itemSlot = detail::ITEM_SLOT_DATA[i];
		if (!itemSlot.combo && (slotIdMask & itemSlot.slotIdMask) == itemSlot.slotIdMask)
			slots.push_back(static_cast<ItemSlot>(i));
	}
	return slots;
}

/**
 * Java ItemSlot.getSlotFor(slot): the first slot of getSlotsFor(slot).
 *
 * @throws IllegalArgumentException if the mask is 0
 * @throws ArrayIndexOutOfBoundsException if the mask contains no single slot (Java indexes the empty array)
 */
inline ItemSlot getSlotFor(int64_t slot) {
	std::vector<ItemSlot> slots = getSlotsFor(slot);
	if (slots.empty())
		throw runtime::ArrayIndexOutOfBoundsException("Index 0 out of bounds for length 0");
	return slots[0];
}

} // namespace aion::gameserver::model::items
