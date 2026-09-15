#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/player/HouseOwnerState.h"

namespace aion::gameserver::model::gameobjects::player {

/** Companion of the generated enum HouseOwnerState (docs/design/static-data.md §2.5): Java's constructor data as constexpr free functions (ADL). */

/** Java: HouseOwnerState.getId() - HAS_OWNER 1, SINGLE_HOUSE 2, BIDDING_ALLOWED 4 ((byte) (1 << ordinal)) */
constexpr int8_t getId(HouseOwnerState state) noexcept {
	return static_cast<int8_t>(1 << static_cast<int32_t>(state));
}

} // namespace aion::gameserver::model::gameobjects::player
