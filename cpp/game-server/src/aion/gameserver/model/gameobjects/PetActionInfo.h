#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/gameobjects/PetAction.h"

namespace aion::gameserver::model::gameobjects {

/**
 * Companion of the generated enum PetAction (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and methods as
 * free functions found by ADL (`getActionId(value)` for Java `value.getActionId()`).
 *
 * @author ATracer
 */

namespace detail {
/** Java constructor argument `id` in ordinal order */
inline constexpr std::array<int32_t, 15> PETACTION_IDS{
	0, // LOAD_PETS
	1, // ADOPT
	2, // SURRENDER
	3, // SPAWN
	4, // DISMISS
	6, // TALK_WITH_MERCHANT
	7, // TALK_WITH_MINDER
	9, // FOOD
	10, // RENAME
	12, // MOOD
	13, // SPECIAL_FUNCTION
	15, // EXTEND_EXPIRATION
	16, // H_ADOPT
	17, // H_ABANDON
	255, // UNKNOWN
};
static_assert(static_cast<size_t>(PetAction::UNKNOWN) + 1 == PETACTION_IDS.size(), "one entry per PetAction constant");
} // namespace detail

constexpr int32_t getActionId(PetAction value) noexcept {
	return detail::PETACTION_IDS[static_cast<size_t>(value)];
}

/** @return the action with the id, UNKNOWN if there is none (Java: lookup in the static petActions map) */
constexpr PetAction getActionById(int32_t actionId) noexcept {
	for (size_t i = 0; i < detail::PETACTION_IDS.size(); ++i) {
		if (detail::PETACTION_IDS[i] == actionId)
			return static_cast<PetAction>(i);
	}
	return PetAction::UNKNOWN;
}

} // namespace aion::gameserver::model::gameobjects
