#pragma once

#include <array>
#include <cstddef>

#include "aion/gameserver/model/gameobjects/state/CreatureSeeState.h"
#include "aion/gameserver/model/templates/npc/NpcRating.h"

namespace aion::gameserver::model::templates::npc {

/**
 * Companion of the generated enum NpcRating (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and methods as
 * free functions found by ADL (`getCongenitalSeeState(rating)` for Java `rating.getCongenitalSeeState()`).
 *
 * @author ATracer
 */

namespace detail {
/** Java constructor argument `congenitalSeeState` in ordinal order */
inline constexpr std::array<gameobjects::state::CreatureSeeState, 5> NPC_RATING_SEE_STATES{
	gameobjects::state::CreatureSeeState::NORMAL,  // JUNK
	gameobjects::state::CreatureSeeState::NORMAL,  // NORMAL
	gameobjects::state::CreatureSeeState::SEARCH1, // ELITE
	gameobjects::state::CreatureSeeState::SEARCH2, // HERO
	gameobjects::state::CreatureSeeState::SEARCH2, // LEGENDARY
};
static_assert(static_cast<size_t>(NpcRating::LEGENDARY) + 1 == NPC_RATING_SEE_STATES.size(), "one entry per NpcRating constant");
} // namespace detail

constexpr gameobjects::state::CreatureSeeState getCongenitalSeeState(NpcRating rating) noexcept {
	return detail::NPC_RATING_SEE_STATES[static_cast<size_t>(rating)];
}

} // namespace aion::gameserver::model::templates::npc
