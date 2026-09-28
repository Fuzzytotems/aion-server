#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"

namespace aion::gameserver::model::gameobjects::player {

/** Companion of the generated enum CustomPlayerState (docs/design/static-data.md §2.5): Java's constructor data as constexpr free functions (ADL). */

namespace detail {
/** Java CustomPlayerState bit masks in ordinal order */
inline constexpr std::array<int32_t, 12> CUSTOM_PLAYER_STATE_MASKS{
	1,                   // WATCHING_CUTSCENE
	1 << 1,              // INVULNERABLE
	1 << 2,              // EVENT_MODE
	1 << 3,              // TELEPORTATION_MODE
	1 << 4,              // NO_SKILL_COOLDOWN_MODE
	1 << 5,              // NO_WHISPERS_MODE
	1 << 6,              // ENEMY_OF_ALL_NPCS
	1 << 7,              // ENEMY_OF_ALL_PLAYERS
	1 << 8,              // NEUTRAL_TO_ALL_NPCS
	1 << 9,              // NEUTRAL_TO_ALL_PLAYERS
	(1 << 6) | (1 << 7), // ENEMY_OF_EVERYONE
	(1 << 8) | (1 << 9), // NEUTRAL_TO_EVERYONE
};
static_assert(static_cast<size_t>(CustomPlayerState::NEUTRAL_TO_EVERYONE) + 1 == CUSTOM_PLAYER_STATE_MASKS.size());
} // namespace detail

/** Java: CustomPlayerState.getMask() (package-private) */
constexpr int32_t getMask(CustomPlayerState state) noexcept {
	return detail::CUSTOM_PLAYER_STATE_MASKS[static_cast<size_t>(state)];
}

} // namespace aion::gameserver::model::gameobjects::player
