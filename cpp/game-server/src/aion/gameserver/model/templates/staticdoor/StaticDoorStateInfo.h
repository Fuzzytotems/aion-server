#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/templates/staticdoor/StaticDoorState.h"

namespace aion::gameserver::model::templates::staticdoor {

/**
 * Companion of the generated enum StaticDoorState (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and methods
 * as free functions found by ADL (`getFlag(state)` for Java `state.getFlag()`). The static EnumSet functions take any set of states offering
 * `add`, `remove` and `contains` (StaticDoor's TreeSet shim, std::set through a thin adapter).
 */

namespace detail {
/** Java constructor argument `flag` in ordinal order */
inline constexpr std::array<int32_t, 5> STATIC_DOOR_STATE_FLAGS{
	0,      // NONE
	1 << 0, // OPENED
	1 << 1, // CLICKABLE
	1 << 2, // CLOSEABLE
	1 << 3, // ONEWAY
};
static_assert(static_cast<size_t>(StaticDoorState::ONEWAY) + 1 == STATIC_DOOR_STATE_FLAGS.size(), "one entry per StaticDoorState constant");
} // namespace detail

constexpr int32_t getFlag(StaticDoorState state) noexcept {
	return detail::STATIC_DOOR_STATE_FLAGS[static_cast<size_t>(state)];
}

/** Java static setStates(int flags, EnumSet<StaticDoorState> state): NONE is skipped */
template <class StateSet>
void setStates(int32_t flags, StateSet& state) {
	for (size_t ordinal = 0; ordinal < detail::STATIC_DOOR_STATE_FLAGS.size(); ++ordinal) {
		const auto states = static_cast<StaticDoorState>(ordinal);
		if (states == StaticDoorState::NONE)
			continue;
		if ((flags & getFlag(states)) == 0)
			state.remove(states);
		else
			state.add(states);
	}
}

/** Java static getFlags(EnumSet<StaticDoorState> doorStates): NONE is skipped */
template <class StateSet>
int32_t getFlags(const StateSet& doorStates) {
	int32_t result = 0;
	for (size_t ordinal = 0; ordinal < detail::STATIC_DOOR_STATE_FLAGS.size(); ++ordinal) {
		const auto state = static_cast<StaticDoorState>(ordinal);
		if (state == StaticDoorState::NONE)
			continue;
		if (doorStates.contains(state))
			result |= getFlag(state);
	}
	return result;
}

} // namespace aion::gameserver::model::templates::staticdoor
