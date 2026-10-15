#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/model/house/HouseDoorState.h"

namespace aion::gameserver::model::house {

/** Companion of the generated enum HouseDoorState (docs/design/static-data.md §2.5): Java's constructor data and lookup as free functions (ADL). */

/** Java: HouseDoorState.getId() - OPEN(1), CLOSED_EXCEPT_FRIENDS(2), CLOSED(3) */
constexpr int8_t getId(HouseDoorState state) noexcept {
	return static_cast<int8_t>(static_cast<int32_t>(state) + 1);
}

/** Java: HouseDoorState.get(byte) - std::nullopt (Java null) for an unknown id */
constexpr std::optional<HouseDoorState> houseDoorStateOf(int8_t id) noexcept {
	for (HouseDoorState state : {HouseDoorState::OPEN, HouseDoorState::CLOSED_EXCEPT_FRIENDS, HouseDoorState::CLOSED}) {
		if (id == getId(state))
			return state;
	}
	return std::nullopt;
}

} // namespace aion::gameserver::model::house
