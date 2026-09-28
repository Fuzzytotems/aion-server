#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "aion/gameserver/model/templates/item/WeaponType.h"

namespace aion::gameserver::model::templates::item {

/**
 * Companion of the generated enum WeaponType (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and methods as
 * free functions found by ADL (`getRequiredSlots(type)` for Java `type.getRequiredSlots()`).
 *
 * @author ATracer
 */

namespace detail {
/** Java `new int[] { ... }` required skill arrays */
inline constexpr std::array<int32_t, 2> WEAPON_TYPE_SKILLS_DAGGER_1H{30, 9};
inline constexpr std::array<int32_t, 2> WEAPON_TYPE_SKILLS_MACE_1H{3, 10};
inline constexpr std::array<int32_t, 2> WEAPON_TYPE_SKILLS_SWORD_1H{1, 8};
inline constexpr std::array<int32_t, 2> WEAPON_TYPE_SKILLS_GUN_1H{83, 76};
inline constexpr std::array<int32_t, 1> WEAPON_TYPE_SKILLS_BOOK_2H{64};
inline constexpr std::array<int32_t, 1> WEAPON_TYPE_SKILLS_ORB_2H{64};
inline constexpr std::array<int32_t, 1> WEAPON_TYPE_SKILLS_POLEARM_2H{16};
inline constexpr std::array<int32_t, 1> WEAPON_TYPE_SKILLS_STAFF_2H{53};
inline constexpr std::array<int32_t, 1> WEAPON_TYPE_SKILLS_SWORD_2H{15};
inline constexpr std::array<int32_t, 1> WEAPON_TYPE_SKILLS_BOW{17};
inline constexpr std::array<int32_t, 1> WEAPON_TYPE_SKILLS_CANNON_2H{77};
inline constexpr std::array<int32_t, 2> WEAPON_TYPE_SKILLS_HARP_2H{92, 78};
inline constexpr std::array<int32_t, 2> WEAPON_TYPE_SKILLS_KEYBLADE_2H{76, 79};

/** Java constructor arguments (requiredSkills, slots) in ordinal order */
struct WeaponTypeData {
	std::span<const int32_t> requiredSkill;
	int32_t slots;
};

inline constexpr std::array<WeaponTypeData, 18> WEAPON_TYPE_DATA{{
	{WEAPON_TYPE_SKILLS_DAGGER_1H, 1}, // DAGGER_1H
	{WEAPON_TYPE_SKILLS_MACE_1H, 1}, // MACE_1H
	{WEAPON_TYPE_SKILLS_SWORD_1H, 1}, // SWORD_1H
	{std::span<const int32_t>(), 1}, // TOOLHOE_1H
	{WEAPON_TYPE_SKILLS_GUN_1H, 1}, // GUN_1H
	{WEAPON_TYPE_SKILLS_BOOK_2H, 2}, // BOOK_2H
	{WEAPON_TYPE_SKILLS_ORB_2H, 2}, // ORB_2H
	{WEAPON_TYPE_SKILLS_POLEARM_2H, 2}, // POLEARM_2H
	{WEAPON_TYPE_SKILLS_STAFF_2H, 2}, // STAFF_2H
	{WEAPON_TYPE_SKILLS_SWORD_2H, 2}, // SWORD_2H
	{std::span<const int32_t>(), 2}, // TOOLPICK_2H
	{std::span<const int32_t>(), 2}, // TOOLROD_2H
	{WEAPON_TYPE_SKILLS_BOW, 2}, // BOW
	{WEAPON_TYPE_SKILLS_CANNON_2H, 2}, // CANNON_2H
	{WEAPON_TYPE_SKILLS_HARP_2H, 2}, // HARP_2H
	{std::span<const int32_t>(), 2}, // GUN_2H
	{WEAPON_TYPE_SKILLS_KEYBLADE_2H, 2}, // KEYBLADE_2H
	{std::span<const int32_t>(), 2}, // KEYHAMMER_2H
}};
static_assert(static_cast<size_t>(WeaponType::KEYHAMMER_2H) + 1 == WEAPON_TYPE_DATA.size(), "one entry per WeaponType constant");
} // namespace detail

/** @return the required skill ids (never null in Java: types without skills have an empty array) */
constexpr std::span<const int32_t> getRequiredSkills(WeaponType type) noexcept {
	return detail::WEAPON_TYPE_DATA[static_cast<size_t>(type)].requiredSkill;
}

constexpr int32_t getRequiredSlots(WeaponType type) noexcept {
	return detail::WEAPON_TYPE_DATA[static_cast<size_t>(type)].slots;
}

/** Java `1 << ordinal()` */
constexpr int32_t getMask(WeaponType type) noexcept {
	return 1 << static_cast<int32_t>(type);
}

} // namespace aion::gameserver::model::templates::item
